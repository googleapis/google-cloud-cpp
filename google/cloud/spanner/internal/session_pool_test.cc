// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace spanner_proto = ::google::spanner::v1;

// Matches a BatchCreateSessionsRequest with the specified `session_count`.
MATCHER_P(SessionCountIs, session_count,
          "BatchCreateSessionsRequest has expected session_count") {
  return arg.session_count() == session_count;
}

// Matches a BatchCreateSessionsRequest with the specified `labels`.
MATCHER_P(LabelsAre, labels, "BatchCreateSessionsRequest has expected labels") {
  auto const& arg_labels = arg.session_template().labels();
  return labels_type(arg_labels.begin(), arg_labels.end()) == labels;
}

// Create a response with the given `sessions`
spanner_proto::BatchCreateSessionsResponse MakeSessionsResponse(
    std::vector<std::string> sessions) {
  spanner_proto::BatchCreateSessionsResponse response;
  for (auto& session : sessions) {
    response.add_session()->set_name(std::move(session));
  }
  return response;
}

std::shared_ptr<SessionPool> MakeSessionPool(
    Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    SessionPoolOptions options) {
  return std::make_shared<SessionPool>(
      std::move(db), std::move(stubs), std::move(options),
      google::cloud::internal::make_unique<LimitedTimeRetryPolicy>(
          std::chrono::minutes(10)),
      google::cloud::internal::make_unique<ExponentialBackoffPolicy>(
          std::chrono::milliseconds(100), std::chrono::minutes(1), 2.0));
}

TEST(SessionPool, Allocate) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(
          [&db](grpc::ClientContext&,
                spanner_proto::BatchCreateSessionsRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            EXPECT_EQ(1, request.session_count());
            return MakeSessionsResponse({"session1"});
          });

  auto pool = MakeSessionPool(db, {mock}, {});
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST(SessionPool, CreateError) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "some failure"))));

  auto pool = MakeSessionPool(db, {mock}, {});
  auto session = pool->Allocate();
  EXPECT_EQ(session.status().code(), StatusCode::kInternal);
  EXPECT_THAT(session.status().message(), HasSubstr("some failure"));
}

TEST(SessionPool, ReuseSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));

  auto pool = MakeSessionPool(db, {mock}, {});
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
  session->reset();

  auto session2 = pool->Allocate();
  ASSERT_STATUS_OK(session2);
  EXPECT_EQ((*session2)->session_name(), "session1");
}

TEST(SessionPool, Lifo) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session2"}))));

  auto pool = MakeSessionPool(db, {mock}, {});
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");

  auto session2 = pool->Allocate();
  ASSERT_STATUS_OK(session2);
  EXPECT_EQ((*session2)->session_name(), "session2");

  session->reset();
  session2->reset();

  // The pool is Last-In-First-Out (LIFO), so we expect to get the sessions
  // back in the reverse order they were released.
  auto session3 = pool->Allocate();
  ASSERT_STATUS_OK(session3);
  EXPECT_EQ((*session3)->session_name(), "session2");

  auto session4 = pool->Allocate();
  ASSERT_STATUS_OK(session4);
  EXPECT_EQ((*session4)->session_name(), "session1");
}

TEST(SessionPool, MinSessionsEagerAllocation) {
  int const min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));

  SessionPoolOptions options;
  options.set_min_sessions(min_sessions);
  auto pool = MakeSessionPool(db, {mock}, options);
  auto session = pool->Allocate();
}

TEST(SessionPool, MinSessionsMultipleAllocations) {
  int const min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  // The constructor will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));

  SessionPoolOptions options;
  options.set_min_sessions(min_sessions);
  auto pool = MakeSessionPool(db, {mock}, options);

  // When we run out of sessions it will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions + 1)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s7", "s6", "s5", "s4"}))));
  std::vector<SessionHolder> sessions;
  std::vector<std::string> session_names;
  for (int i = 1; i <= 7; ++i) {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    session_names.push_back((*session)->session_name());
    sessions.push_back(*std::move(session));
  }
  EXPECT_THAT(session_names,
              UnorderedElementsAre("s1", "s2", "s3", "s4", "s5", "s6", "s7"));
}

TEST(SessionPool, MaxSessionsFailOnExhaustion) {
  int const max_sessions_per_channel = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3"}))));

  SessionPoolOptions options;
  options.set_max_sessions_per_channel(max_sessions_per_channel)
      .set_action_on_exhaustion(ActionOnExhaustion::FAIL);
  auto pool = MakeSessionPool(db, {mock}, options);
  std::vector<SessionHolder> sessions;
  std::vector<std::string> session_names;
  for (int i = 1; i <= 3; ++i) {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    session_names.push_back((*session)->session_name());
    sessions.push_back(*std::move(session));
  }
  EXPECT_THAT(session_names, UnorderedElementsAre("s1", "s2", "s3"));
  auto session = pool->Allocate();
  EXPECT_EQ(session.status().code(), StatusCode::kResourceExhausted);
  EXPECT_EQ(session.status().message(), "session pool exhausted");
}

TEST(SessionPool, MaxSessionsBlockUntilRelease) {
  int const max_sessions_per_channel = 1;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))));

  SessionPoolOptions options;
  options.set_max_sessions_per_channel(max_sessions_per_channel)
      .set_action_on_exhaustion(ActionOnExhaustion::BLOCK);
  auto pool = MakeSessionPool(db, {mock}, options);
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "s1");

  // This thread will block in Allocate() until the main thread releases s1.
  std::thread t([&pool]() {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    EXPECT_EQ((*session)->session_name(), "s1");
  });

  session->reset();
  t.join();
}

TEST(SessionPool, Labels) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  std::map<std::string, std::string> labels = {
      {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
  EXPECT_CALL(*mock, BatchCreateSessions(_, LabelsAre(labels)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));

  SessionPoolOptions options;
  options.set_labels(std::move(labels));
  auto pool = MakeSessionPool(db, {mock}, options);
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST(SessionPool, MultipleChannels) {
  auto mock1 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock2 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock1, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s3"}))));
  EXPECT_CALL(*mock2, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s3"}))));

  auto pool = MakeSessionPool(db, {mock1, mock2}, {});
  std::vector<SessionHolder> sessions;
  std::vector<std::string> session_names;
  for (int i = 1; i <= 6; ++i) {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    session_names.push_back((*session)->session_name());
    sessions.push_back(*std::move(session));
  }
  EXPECT_THAT(session_names, UnorderedElementsAre("c1s1", "c1s2", "c1s3",
                                                  "c2s1", "c2s2", "c2s3"));
}

TEST(SessionPool, MultipleChannelsPreAllocation) {
  auto mock1 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock2 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock3 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock1, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s1", "c1s2", "c1s3"}))));
  EXPECT_CALL(*mock2, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s1", "c2s2", "c2s3"}))));
  EXPECT_CALL(*mock3, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c3s1", "c3s2", "c3s3"}))));

  SessionPoolOptions options;
  // note that min_sessions will effectively be reduced to 9
  // (max_sessions_per_channel * num_channels).
  options.set_min_sessions(20)
      .set_max_sessions_per_channel(3)
      .set_action_on_exhaustion(ActionOnExhaustion::FAIL);
  auto pool = MakeSessionPool(db, {mock1, mock2, mock3}, options);
  std::vector<SessionHolder> sessions;
  std::vector<std::string> session_names;
  for (int i = 1; i <= 9; ++i) {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    session_names.push_back((*session)->session_name());
    sessions.push_back(*std::move(session));
  }
  EXPECT_THAT(session_names,
              UnorderedElementsAre("c1s1", "c1s2", "c1s3", "c2s1", "c2s2",
                                   "c2s3", "c3s1", "c3s2", "c3s3"));
  auto session = pool->Allocate();
  EXPECT_EQ(session.status().code(), StatusCode::kResourceExhausted);
  EXPECT_EQ(session.status().message(), "session pool exhausted");
}

TEST(SessionPool, GetStubForStublessSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  auto pool = MakeSessionPool(db, {mock}, {});
  // ensure we get a stub even if we didn't allocate from the pool.
  auto session = MakeDissociatedSessionHolder("session_id");
  EXPECT_EQ(pool->GetStub(*session), mock);
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
