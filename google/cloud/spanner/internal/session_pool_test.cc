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
#include "google/cloud/internal/disable_deprecation_warnings.inc"
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
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;

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
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq(), {});
  // Options{}.set<spanner::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
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
  auto pool =
      MakeTestSessionPool(db, {mock}, threads.cq(),
                          Options{}.set<spanner::RouteToLeaderOption>(true));

  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, AllocateRouteToLeader) {
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
  auto pool =
      MakeTestSessionPool(db, {mock}, threads.cq(),
                          Options{}
                              .set<spanner::RouteToLeaderOption>(true)
                              .set<spanner::SessionPoolMinSessionsOption>(42));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
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
  auto pool =
      MakeTestSessionPool(db, {mock}, threads.cq(),
                          Options{}.set<spanner::RouteToLeaderOption>(false));

  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, AllocateNoRouteToLeader) {
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
  auto pool =
      MakeTestSessionPool(db, {mock}, threads.cq(),
                          Options{}
                              .set<spanner::RouteToLeaderOption>(false)
                              .set<spanner::SessionPoolMinSessionsOption>(42));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
  EXPECT_EQ(pool->GetStub(**session), mock);
}

TEST_F(SessionPoolTest, MultiplexedCreateError) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, CreateSession)
      .WillRepeatedly(
          Return(ByMove(Status(StatusCode::kInternal, "init failure"))));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq(), {});

  auto session = pool->Multiplexed();
  EXPECT_THAT(session,
              StatusIs(StatusCode::kInternal, HasSubstr("init failure")));
}

TEST_F(SessionPoolTest, ReuseSession) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce([](grpc::ClientContext&, Options const&,
                   google::spanner::v1::CreateSessionRequest const&) {
        return MakeMultiplexedSession("multiplexed");
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads threads;
  auto pool = MakeTestSessionPool(db, {mock}, threads.cq());
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
  session->reset();

  auto session2 = pool->Multiplexed();
  ASSERT_STATUS_OK(session2);
  EXPECT_EQ((*session2)->session_name(), "multiplexed");
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
      Options{}.set<spanner::SessionPoolLabelsOption>(std::move(labels)));
  // .set<spanner::EnableMultiplexedSessionOption>({}));
  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
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
      Options{}.set<spanner::SessionCreatorRoleOption>(role));

  auto session = pool->Multiplexed();
  ASSERT_STATUS_OK(session);
  EXPECT_EQ((*session)->session_name(), "multiplexed");
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

TEST_F(SessionPoolTest, MultilpexedSessionReplacementSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce(Return(ByMove(MakeMultiplexedSession({"multiplexed1"}))));
  EXPECT_CALL(*mock, AsyncCreateSession(_, _, _, _))
      .WillOnce(Return(make_ready_future(StatusOr<google::spanner::v1::Session>(
          MakeMultiplexedSession({"multiplexed2"})))));

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  auto background_interval =
      std::chrono::duration_cast<std::chrono::minutes>(std::chrono::seconds(1));
  auto replacement_interval =
      std::chrono::duration_cast<std::chrono::hours>(std::chrono::seconds(2));
  auto clock = std::make_shared<FakeSteadyClock>();
  auto pool = MakeTestSessionPool(
      db, {mock}, CompletionQueue(impl),
      Options{}
          .set<SessionPoolClockOption>(clock)
          .set<MultiplexedSessionBackgroundWorkIntervalOption>(
              background_interval)
          .set<MultiplexedSessionReplacementIntervalOption>(
              replacement_interval));

  auto s1 = pool->Multiplexed();
  ASSERT_STATUS_OK(s1);
  EXPECT_EQ((*s1)->session_name(), "multiplexed1");

  clock->AdvanceTime(background_interval);
  impl->SimulateCompletion(true);

  auto s2 = pool->Multiplexed();
  ASSERT_STATUS_OK(s2);
  EXPECT_EQ((*s2)->session_name(), "multiplexed2");

  // Cancel all pending operations, satisfying any remaining futures.
  impl->SimulateCompletion(false);
}

TEST_F(SessionPoolTest, MultilpexedSessionReplacementRpcPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(
      *mock,
      CreateSession(_, _, AllOf(DatabaseIs(db.FullName()), IsMultiplexed())))
      .WillOnce(Return(ByMove(MakeMultiplexedSession({"multiplexed"}))));
  EXPECT_CALL(*mock, AsyncCreateSession(_, _, _, _))
      .WillOnce(Return(make_ready_future(StatusOr<google::spanner::v1::Session>(
          Status(StatusCode::kResourceExhausted, "retry policy exhausted")))));

  auto impl = std::make_shared<FakeCompletionQueueImpl>();
  auto background_interval =
      std::chrono::duration_cast<std::chrono::minutes>(std::chrono::seconds(1));
  auto replacement_interval =
      std::chrono::duration_cast<std::chrono::hours>(std::chrono::seconds(2));
  auto clock = std::make_shared<FakeSteadyClock>();
  auto pool = MakeTestSessionPool(
      db, {mock}, CompletionQueue(impl),
      Options{}
          .set<SessionPoolClockOption>(clock)
          .set<MultiplexedSessionBackgroundWorkIntervalOption>(
              background_interval)
          .set<MultiplexedSessionReplacementIntervalOption>(
              replacement_interval));

  auto s1 = pool->Multiplexed();
  ASSERT_STATUS_OK(s1);
  EXPECT_EQ((*s1)->session_name(), "multiplexed");

  clock->AdvanceTime(background_interval);
  impl->SimulateCompletion(true);

  auto s2 = pool->Multiplexed();
  ASSERT_STATUS_OK(s2);
  EXPECT_EQ((*s2)->session_name(), "multiplexed");

  // Cancel all pending operations, satisfying any remaining futures.
  impl->SimulateCompletion(false);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
#include "google/cloud/internal/diagnostics_pop.inc"
