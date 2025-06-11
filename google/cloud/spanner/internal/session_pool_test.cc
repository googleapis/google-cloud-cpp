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
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/spanner/testing/status_utils.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/clock.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/time/clock.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::FakeSteadyClock;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

auto constexpr kRouteToLeader = "x-goog-spanner-route-to-leader";

class SessionPoolTest : public testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  testing_util::ValidateMetadataFixture validate_metadata_fixture_;
};

// Matches a CreateSessionRequest or BatchCreateSessionsRequest with the
// specified `database_name`.
MATCHER_P(DatabaseIs, database_name, "request has expected database name") {
  return arg.database() == database_name;
}

// Matches a multiplexed CreateSessionRequest.
MATCHER(IsMultiplexed, "is a multiplexed CreateSessionRequest") {
  return arg.session().multiplexed();
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

// Matches a DeleteSessionRequest with the specified `name`.
MATCHER_P(SessionNameIs, name,
          "DeleteSessionRequest has expected session name") {
  return arg.name() == name;
}

google::protobuf::Timestamp Now() {
  auto now = spanner::MakeTimestamp(absl::Now()).value();
  return now.get<google::protobuf::Timestamp>().value();
}

// Create a session with the given `name`.
google::spanner::v1::Session MakeMultiplexedSession(std::string name,
                                                    std::string role = "") {
  google::spanner::v1::Session session;
  session.set_name(std::move(name));
  *session.mutable_create_time() = Now();
  *session.mutable_approximate_last_use_time() = Now();
  if (!role.empty()) session.set_creator_role(std::move(role));
  session.set_multiplexed(true);
  return session;
}

// Create a response with the given `sessions`.
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

TEST_F(SessionPoolTest, Multiplexed) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce(Return(ByMove(MakeMultiplexedSession({"multiplexed"}))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner_experimental::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
}

TEST_F(SessionPoolTest, MultiplexedFallback) {
  // Falling back from Multiplexed to SessionPool is not supported. Update
  // this test accordingly.
  GTEST_SKIP();
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq());
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST_F(SessionPoolTest, MultiplexedAllocateRouteToLeader) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::spanner::v1::CreateSessionRequest const&) {
        EXPECT_THAT(GetMetadata(context),
                    Contains(Pair(kRouteToLeader, "true")));
        return MakeMultiplexedSession("multiplexed");
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}
          .set<spanner::RouteToLeaderOption>(true)
          .set<spanner_experimental::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, AllocateRouteToLeader) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock,
              BatchCreateSessions(
                  _, _, AllOf(DatabaseIs(db.FullName()), SessionCountIs(42))))
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::spanner::v1::BatchCreateSessionsRequest const&) {
        EXPECT_THAT(GetMetadata(context),
                    Contains(Pair(kRouteToLeader, "true")));
        return MakeSessionsResponse({"session1"});
      });
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool =
      MakeTestSessionPool(db, {mock}, threads.cq(),
                          Options{}
                              .set<spanner::RouteToLeaderOption>(true)
                              .set<spanner::SessionPoolMinSessionsOption>(42));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, MultiplexedAllocateNoRouteToLeader) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::spanner::v1::CreateSessionRequest const&) {
        EXPECT_THAT(GetMetadata(context),
                    AnyOf(Contains(Pair(kRouteToLeader, "false")),
                          Not(Contains(Pair(kRouteToLeader, _)))));
        return MakeMultiplexedSession("multiplexed");
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}
          .set<spanner::RouteToLeaderOption>(false)
          .set<spanner_experimental::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, AllocateNoRouteToLeader) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  //  EXPECT_CALL(
  //      *mock,
  //      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()),
  //      IsMultiplexed()))) .WillOnce([this](grpc::ClientContext& context,
  //      Options const&,
  //                       google::spanner::v1::CreateSessionRequest const&) {
  //        EXPECT_THAT(GetMetadata(context),
  //                    AnyOf(Contains(Pair(kRouteToLeader, "false")),
  //                          Not(Contains(Pair(kRouteToLeader, _)))));
  //        return MakeMultiplexedSession("multiplexed");
  //      });
  EXPECT_CALL(*mock,
              BatchCreateSessions(
                  _, _, AllOf(DatabaseIs(db.FullName()), SessionCountIs(42))))
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::spanner::v1::BatchCreateSessionsRequest const&) {
        EXPECT_THAT(GetMetadata(context),
                    AnyOf(Contains(Pair(kRouteToLeader, "false")),
                          Not(Contains(Pair(kRouteToLeader, _)))));
        return MakeSessionsResponse({"session1"});
      });
  //  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _,
  //  SessionNameIs("multiplexed")))
  //      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool =
      MakeTestSessionPool(db, {mock}, threads.cq(),
                          Options{}
                              .set<spanner::RouteToLeaderOption>(false)
                              .set<spanner::SessionPoolMinSessionsOption>(42));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, ReleaseBadSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock,
              BatchCreateSessions(
                  _, _, AllOf(DatabaseIs(db.FullName()), SessionCountIs(1))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));
  EXPECT_CALL(*mock,
              BatchCreateSessions(
                  _, _, AllOf(DatabaseIs(db.FullName()), SessionCountIs(2))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session2"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .Times(0);
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session2")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, CreateError) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "init failure"))))
      .WillOnce(Return(ByMove(Status(StatusCode::kInternal, "some failure"))));
  EXPECT_CALL(*mock, AsyncDeleteSession).Times(0);

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq());
  auto session = pool->Allocate();
  EXPECT_THAT(session,
              StatusIs(StatusCode::kInternal, HasSubstr("some failure")));
}

TEST_F(SessionPoolTest, ReuseSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, Lifo) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session2"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session2")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, MinSessionsEagerAllocation) {
  int const min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, _, SessionCountIs(min_sessions)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s3")))
      .WillOnce(Return(make_ready_future(Status{})));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(min_sessions));
  auto session = pool->Allocate();
}

TEST_F(SessionPoolTest, MinSessionsMultipleAllocations) {
  int const min_sessions = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  // The constructor will make this call.
  EXPECT_CALL(*mock, BatchCreateSessions(_, _, SessionCountIs(min_sessions)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3", "s2", "s1"}))));
  // When we run out of sessions it will make this call.
  EXPECT_CALL(*mock,
              BatchCreateSessions(_, _, SessionCountIs(min_sessions + 1)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s7", "s6", "s5", "s4"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s3")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s4")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s5")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s6")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s7")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, MaxSessionsFailOnExhaustion) {
  int const max_sessions_per_channel = 3;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s3")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, MaxSessionsBlockUntilRelease) {
  int const max_sessions_per_channel = 1;
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s1")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, MultiplexedLabels) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::map<std::string, std::string> labels = {
      {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce(
          [labels](grpc::ClientContext&, Options const&,
                   google::spanner::v1::CreateSessionRequest const& request) {
            auto const& request_labels = request.session().labels();
            EXPECT_EQ((std::map<std::string, std::string>(
                          request_labels.begin(), request_labels.end())),
                      labels);
            return MakeMultiplexedSession("multiplexed");
          });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}
          .set<spanner::SessionPoolLabelsOption>(std::move(labels))
          .set<spanner_experimental::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
}

TEST_F(SessionPoolTest, Labels) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::map<std::string, std::string> labels = {
      {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
  EXPECT_CALL(*mock, BatchCreateSessions(_, _, LabelsAre(labels)))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolLabelsOption>(std::move(labels)));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST_F(SessionPoolTest, MultiplexedCreatorRole) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const role = "public";
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce(
          [role](grpc::ClientContext&, Options const&,
                 google::spanner::v1::CreateSessionRequest const& request) {
            EXPECT_EQ(request.session().creator_role(), role);
            return MakeMultiplexedSession("multiplexed");
          });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}
          .set<spanner::SessionCreatorRoleOption>(role)
          .set<spanner_experimental::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
}

TEST_F(SessionPoolTest, CreatorRole) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const role = "public";
  EXPECT_CALL(*mock,
              BatchCreateSessions(
                  _, _, AllOf(DatabaseIs(db.FullName()), CreatorRoleIs(role))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"session1"}, role))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("session1")))
      .WillOnce(Return(make_ready_future(Status{})));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionCreatorRoleOption>(role));
  auto session = pool->Allocate();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "session1");
}

TEST_F(SessionPoolTest, MultipleChannels) {
  auto mock1 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock2 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock1, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock1, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s3"}))));
  EXPECT_CALL(*mock1, AsyncDeleteSession(_, _, _, SessionNameIs("c1s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock1, AsyncDeleteSession(_, _, _, SessionNameIs("c1s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock1, AsyncDeleteSession(_, _, _, SessionNameIs("c1s3")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock2, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock2, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s3"}))));
  EXPECT_CALL(*mock2, AsyncDeleteSession(_, _, _, SessionNameIs("c2s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock2, AsyncDeleteSession(_, _, _, SessionNameIs("c2s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock2, AsyncDeleteSession(_, _, _, SessionNameIs("c2s3")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, MultipleChannelsPreAllocation) {
  auto mock1 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock2 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto mock3 = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock1, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock1, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c1s1", "c1s2", "c1s3"}))));
  EXPECT_CALL(*mock1, AsyncDeleteSession(_, _, _, SessionNameIs("c1s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock1, AsyncDeleteSession(_, _, _, SessionNameIs("c1s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock1, AsyncDeleteSession(_, _, _, SessionNameIs("c1s3")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock2, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock2, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c2s1", "c2s2", "c2s3"}))));
  EXPECT_CALL(*mock2, AsyncDeleteSession(_, _, _, SessionNameIs("c2s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock2, AsyncDeleteSession(_, _, _, SessionNameIs("c2s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock2, AsyncDeleteSession(_, _, _, SessionNameIs("c2s3")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock3, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  EXPECT_CALL(*mock3, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"c3s1", "c3s2", "c3s3"}))));
  EXPECT_CALL(*mock3, AsyncDeleteSession(_, _, _, SessionNameIs("c3s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock3, AsyncDeleteSession(_, _, _, SessionNameIs("c3s2")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock3, AsyncDeleteSession(_, _, _, SessionNameIs("c3s3")))
      .WillOnce(Return(make_ready_future(Status{})));

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

TEST_F(SessionPoolTest, GetStubForStublessSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(
      db, {mock}, threads.cq(),
      Options{}.set<spanner::SessionPoolMinSessionsOption>(0));
  // ensure we get a stub even if we didn't allocate from the pool.
  auto session = MakeDissociatedSessionHolder("session_id");
  EXPECT_EQ(pool->GetStub(*session), mock);
}

TEST_F(SessionPoolTest, MultilpexedSessionRefresh) { GTEST_SKIP(); }

TEST_F(SessionPoolTest, SessionRefresh) {
  auto mock = std::make_shared<StrictMock<spanner_testing::MockSpannerStub>>();
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s2")))
      .WillOnce(Return(make_ready_future(Status{})));

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
          [&result](CompletionQueue&, auto, auto,
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

  // We should still be able to allocate sessions "s1" and "s2".
  auto s1 = pool->Allocate();
  ASSERT_STATUS_OK(s1);
  EXPECT_EQ("s1", (*s1)->session_name());
  auto s2 = pool->Allocate();
  ASSERT_STATUS_OK(s2);
  EXPECT_EQ("s2", (*s2)->session_name());

  // Cancel all pending operations, satisfying any remaining futures. When
  // compiling with exceptions disabled the destructors eventually invoke
  // `std::abort()`. On real programs, shutting down the completion queue
  // will have the same effect.
  impl->SimulateCompletion(false);
}

TEST_F(SessionPoolTest, MultiplexedSessionRefreshNotFound) {
  // This isn't supposed to happen, but we need to handle it if it does.
  GTEST_SKIP();
}

TEST_F(SessionPoolTest, SessionRefreshNotFound) {
  auto mock = std::make_shared<StrictMock<spanner_testing::MockSpannerStub>>();
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s1"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s2"}))))
      .WillOnce(Return(ByMove(MakeSessionsResponse({"s3"}))));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s1")))
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s2"))).Times(0);
  EXPECT_CALL(*mock, AsyncDeleteSession(_, _, _, SessionNameIs("s3")))
      .WillOnce(Return(make_ready_future(Status{})));

  EXPECT_CALL(*mock, AsyncExecuteSql)
      .WillOnce([](CompletionQueue&, auto, auto,
                   google::spanner::v1::ExecuteSqlRequest const& request) {
        EXPECT_EQ("s2", request.session());
        // The "SELECT 1" refresh returns "Session not found".
        return make_ready_future(StatusOr<google::spanner::v1::ResultSet>(
            spanner_testing::SessionNotFoundError(request.session())));
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
  // the first two BatchCreateSessions() expectations.
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
  EXPECT_EQ(pool->total_sessions(), 2);

  // Simulate completion of pending operations, which will result in
  // a call to RefreshExpiringSessions(). This should refresh "s2" and
  // satisfy the AsyncExecuteSql() expectation, which fails the call.
  impl->SimulateCompletion(true);
  EXPECT_EQ(pool->total_sessions(), 1);

  // We should still be able to allocate session "s1".
  auto s1 = pool->Allocate();
  ASSERT_STATUS_OK(s1);
  EXPECT_EQ("s1", (*s1)->session_name());
  EXPECT_EQ(pool->total_sessions(), 1);

  // However "s2" will be gone now, so a new allocation will produce
  // "s3", satisfying the final BatchCreateSessions() expectation.
  auto s3 = pool->Allocate();
  ASSERT_STATUS_OK(s3);
  EXPECT_EQ("s3", (*s3)->session_name());
  EXPECT_EQ(pool->total_sessions(), 2);

  // Cancel all pending operations, satisfying any remaining futures. When
  // compiling with exceptions disabled the destructors eventually invoke
  // `std::abort()`. In non-test programs, the completion queue does this
  // automatically as part of its shutdown.
  impl->SimulateCompletion(false);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
