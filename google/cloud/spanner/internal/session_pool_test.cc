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
using ::testing::Invoke;
using ::testing::Return;

namespace spanner_proto = ::google::spanner::v1;

// Matches a BatchCreateSessionsRequest with the specified `session_count`.
MATCHER_P(SessionCountIs, session_count,
          "BatchCreateSessionsRequest has expected session_count") {
  return arg.session_count() == session_count;
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
    Database db, std::shared_ptr<SpannerStub> stub,
    SessionPoolOptions options = SessionPoolOptions()) {
  return std::make_shared<SessionPool>(
      std::move(db), std::move(stub),
      google::cloud::internal::make_unique<LimitedTimeRetryPolicy>(
          std::chrono::minutes(10)),
      google::cloud::internal::make_unique<ExponentialBackoffPolicy>(
          std::chrono::milliseconds(100), std::chrono::minutes(1), 2.0),
      options);
}

TEST(SessionPool, Allocate) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::BatchCreateSessionsRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            EXPECT_EQ(1, request.session_count());
            return MakeSessionsResponse({"session1"});
          }));

  auto pool = MakeSessionPool(db, mock);
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST(SessionPool, CreateError) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "some failure"))));

  auto pool = MakeSessionPool(db, mock);
  auto session = pool->Allocate();
  EXPECT_EQ(session.status().code(), StatusCode::kInternal);
  EXPECT_THAT(session.status().message(), HasSubstr("some failure"));
}

TEST(SessionPool, ReuseSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));

  auto pool = MakeSessionPool(db, mock);
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

  auto pool = MakeSessionPool(db, mock);
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
  const int min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));

  SessionPoolOptions options;
  options.min_sessions = min_sessions;
  auto pool = MakeSessionPool(db, mock, options);
  auto session = pool->Allocate();
}

TEST(SessionPool, MinSessionsMultipleAllocations) {
  const int min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  // The constructor will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));

  SessionPoolOptions options;
  options.min_sessions = min_sessions;
  auto pool = MakeSessionPool(db, mock, options);

  // When we run out of sessions it will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, SessionCountIs(min_sessions + 1)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s7", "s6", "s5", "s4"}))));
  std::vector<SessionHolder> sessions;
  for (int i = 1; i <= 7; ++i) {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    EXPECT_EQ((*session)->session_name(), "s" + std::to_string(i));
    sessions.push_back(*std::move(session));
  }
}

TEST(SessionPool, MaxSessionsFailOnExhaustion) {
  const int max_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3"}))));

  SessionPoolOptions options;
  options.max_sessions = max_sessions;
  options.action_on_exhaustion = ActionOnExhaustion::FAIL;
  auto pool = MakeSessionPool(db, mock, options);
  std::vector<SessionHolder> sessions;
  for (int i = 1; i <= 3; ++i) {
    auto session = pool->Allocate();
    ASSERT_STATUS_OK(session);
    EXPECT_EQ((*session)->session_name(), "s" + std::to_string(i));
    sessions.push_back(*std::move(session));
  }
  auto session = pool->Allocate();
  EXPECT_EQ(session.status().code(), StatusCode::kResourceExhausted);
  EXPECT_EQ(session.status().message(), "session pool exhausted");
}

TEST(SessionPool, MaxSessionsBlockUntilRelease) {
  const int max_sessions = 1;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))));

  SessionPoolOptions options;
  options.max_sessions = max_sessions;
  options.action_on_exhaustion = ActionOnExhaustion::BLOCK;
  auto pool = MakeSessionPool(db, mock, options);
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

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
