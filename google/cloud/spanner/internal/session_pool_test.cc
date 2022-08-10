// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/clock.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/fake_clock.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::spanner_testing::FakeSteadyClock;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

// Matches a BatchCreateSessionsRequest with the specified `database_name`.
MATCHER_P(DatabaseIs, database_name,
          "BatchCreateSessionsRequest has expected database name") {
  return arg.database() == database_name;
}

// Matches a BatchCreateSessionsRequest with the specified `labels`.
MATCHER_P(LabelsAre, labels, "BatchCreateSessionsRequest has expected labels") {
  auto const& arg_labels = arg.session_template().labels();
  return labels_type(arg_labels.begin(), arg_labels.end()) == labels;
}

// Matches a BatchCreateSessionsRequest with the specified `role`.
MATCHER_P(CreatorRoleIs, role,
          "BatchCreateSessionsRequest has expected creator role") {
  return arg.session_template().creator_role() == role;
}

// Matches a BatchCreateSessionsRequest with the specified `session_count`.
MATCHER_P(SessionCountIs, session_count,
          "BatchCreateSessionsRequest has expected session count") {
  return arg.session_count() == session_count;
}

google::protobuf::Timestamp Now() {
  auto now = spanner::MakeTimestamp(absl::Now()).value();
  return now.get<google::protobuf::Timestamp>().value();
}

// Create a response with the given `sessions`
google::spanner::v1::BatchCreateSessionsResponse MakeSessionsResponse(
    std::vector<std::string> sessions, std::string role = "") {
  google::spanner::v1::BatchCreateSessionsResponse response;
  for (auto& session : sessions) {
    auto* s = response.add_session();
    s->set_name(std::move(session));
    *s->mutable_create_time() = Now();
    *s->mutable_approximate_last_use_time() = Now();
    if (!role.empty()) s->set_creator_role(role);
  }
  return response;
}

std::shared_ptr<SessionPool> MakeTestSessionPool(
    spanner::Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    CompletionQueue cq, Options opts = {}) {
  opts.set<spanner::SpannerRetryPolicyOption>(
      std::make_shared<spanner::LimitedTimeRetryPolicy>(
          std::chrono::minutes(10)));
  opts.set<spanner::SpannerBackoffPolicyOption>(
      std::make_shared<spanner::ExponentialBackoffPolicy>(
          std::chrono::milliseconds(100), std::chrono::minutes(1), 2.0));
  opts = DefaultOptions(std::move(opts));
  return MakeSessionPool(std::move(db), std::move(stubs), std::move(cq),
                         std::move(opts));
}

TEST(SessionPool, Allocate) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, AllOf(DatabaseIs(db.FullName()),
                                                  SessionCountIs(42))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(42));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST(SessionPool, ReleaseBadSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, AllOf(DatabaseIs(db.FullName()),
                                                  SessionCountIs(1))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));
  EXPECT_CALL(*mock, BatchCreateSessions(_, AllOf(DatabaseIs(db.FullName()),
                                                  SessionCountIs(2))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session2"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(1));
  {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    EXPECT_EQ((*session)->session_name(), "session1");
  }
  {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    EXPECT_EQ((*session)->session_name(), "session1");
    (*session)->set_bad();  // Marking session1 as bad
  }
  {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    EXPECT_EQ((*session)->session_name(), "session2");  // Got a new session
  }
}

TEST(SessionPool, CreateError) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "init failure"))))
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "some failure"))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq());
  auto session = pool->Allocate();
  EXPECT_THAT(session,
              StatusIs(StatusCode::kInternal, HasSubstr("some failure")));
}

TEST(SessionPool, ReuseSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq());
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
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session2"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq());
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
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(min_sessions));
  auto session = pool->Allocate();
}

TEST(SessionPool, MinSessionsMultipleAllocations) {
  int const min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  // The constructor will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));
  // When we run out of sessions it will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions + 1)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s7", "s6", "s5", "s4"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(min_sessions));
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
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}
          .set<spanner::SessionPoolMaxSessionsPerChannelOption>(
              max_sessions_per_channel)
          .set<spanner::SessionPoolActionOnExhaustionOption>(
              spanner::ActionOnExhaustion::kFail));
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
  EXPECT_THAT(session, StatusIs(StatusCode::kResourceExhausted,
                                "session pool exhausted"));
}

TEST(SessionPool, MaxSessionsBlockUntilRelease) {
  int const max_sessions_per_channel = 1;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}
          .set<spanner::SessionPoolMaxSessionsPerChannelOption>(
              max_sessions_per_channel)
          .set<spanner::SessionPoolActionOnExhaustionOption>(
              spanner::ActionOnExhaustion::kBlock));
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
  auto db = spanner::Database("project", "instance", "database");
  std::map<std::string, std::string> labels = {
      {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
  EXPECT_CALL(*mock, BatchCreateSessions(_, LabelsAre(labels)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolLabelsOption>(std::move(labels)));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST(SessionPool, CreatorRole) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const role = "public";
  EXPECT_CALL(*mock, BatchCreateSessions(_, AllOf(DatabaseIs(db.FullName()),
                                                  CreatorRoleIs(role))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}, role))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionCreatorRoleOption>(role));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST(SessionPool, MultipleChannels) {
  auto mock1 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock2 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock1, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s3"}))));
  EXPECT_CALL(*mock2, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s3"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock1, mock2}, threads.cq());
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
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock1, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s1", "c1s2", "c1s3"}))));
  EXPECT_CALL(*mock2, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s1", "c2s2", "c2s3"}))));
  EXPECT_CALL(*mock3, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c3s1", "c3s2", "c3s3"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  // note that min_sessions will effectively be reduced to 9
  // (max_sessions_per_channel * num_channels).
  auto pool = MakeTestSessionPool(
      db, {mock1, mock2, mock3}, threads.cq(),
      Options{}
          .set<spanner::SessionPoolMaxSessionsPerChannelOption>(3)
          .set<spanner::SessionPoolActionOnExhaustionOption>(
              spanner::ActionOnExhaustion::kFail));
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
  EXPECT_THAT(session, StatusIs(StatusCode::kResourceExhausted,
                                "session pool exhausted"));
}

TEST(SessionPool, GetStubForStublessSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(0));
  // ensure we get a stub even if we didn't allocate from the pool.
  auto session = MakeDissociatedSessionHolder("session_id");
  EXPECT_EQ(pool->GetStub(*session), mock);
}

TEST(SessionPool, SessionRefresh) {
  auto mock = std::make_shared<StrictMock<spanner_testing::MockSpannerStub>>();
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))));

  google::spanner::v1::ResultSet result;
  auto constexpr kResultSetText = R"pb(
    metadata: {
      row_type: { fields: { type: { code: INT64 } } }
      transaction: {}
    }
    rows: { values: { string_value: "1" } }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(kResultSetText, &result));

  EXPECT_CALL(*mock, AsyncExecuteSql)
      .WillOnce(
          [&result](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::spanner::v1::ExecuteSqlRequest const& request) {
            EXPECT_EQ("s2", request.session());
            return make_ready_future(make_status_or(std::move(result)));
          });

  auto db = spanner::Database("project", "instance", "database");
  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  auto keep_alive_interval = std::chrono::seconds(1);
  auto clock = std::make_shared<FakeSteadyClock>();
  auto pool = MakeTestSessionPool(
      db, {mock}, CompletionQueue(impl),
      Options{}
          .set<spanner::SessionPoolKeepAliveIntervalOption>(keep_alive_interval)
          .set<SessionPoolClockOption>(clock));

  // Allocate and release two session, "s1" and "s2". This will satisfy the
  // BatchCreateSessions() expectations.
  {
    auto s1 = pool->Allocate();
    ASSERT_STATUS_OK(s1);
    EXPECT_EQ("s1", (*s1)->session_name());
    {
      auto s2 = pool->Allocate();
      ASSERT_STATUS_OK(s2);
      EXPECT_EQ("s2", (*s2)->session_name());
    }
    // Wait for "s2" to need refreshing before releasing "s1".
    clock->AdvanceTime(keep_alive_interval * 2);
  }

  // Simulate completion of pending operations, which will result in
  // a call to RefreshExpiringSessions(). This should refresh "s2" and
  // satisfy the AsyncExecuteSql() expectation.
  impl->SimulateCompletion(true);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
