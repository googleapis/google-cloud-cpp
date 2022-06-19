// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/wait_for_consistency.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_options.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <deque>

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace btadmin = ::google::bigtable::admin::v2;
using MockConnection =
    ::google::cloud::bigtable_admin_mocks::MockBigtableTableAdminConnection;
using RespType = StatusOr<btadmin::CheckConsistencyResponse>;
using TimerResult = StatusOr<std::chrono::system_clock::time_point>;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

auto constexpr kLimitedErrorCount = 3;

Options TestOptions() {
  using us = std::chrono::microseconds;
  auto retry =
      BigtableTableAdminLimitedErrorCountRetryPolicy(kLimitedErrorCount);
  auto backoff = ExponentialBackoffPolicy(us(1), us(5), 2.0);
  auto polling =
      GenericPollingPolicy<BigtableTableAdminRetryPolicyOption::Type,
                           BigtableTableAdminBackoffPolicyOption::Type>(
          retry.clone(), backoff.clone());

  return Options{}.set<BigtableTableAdminPollingPolicyOption>(polling.clone());
}

RespType MakeResponse(bool consistent) {
  btadmin::CheckConsistencyResponse r;
  r.set_consistent(consistent);
  return make_status_or(std::move(r));
}

TEST(AsyncWaitForConsistency, Simple) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  CompletionQueue cq;
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return make_ready_future(MakeResponse(true));
      });

  auto status = AsyncWaitForConsistency(cq, client, table_name, token).get();
  EXPECT_STATUS_OK(status);
}

TEST(AsyncWaitForConsistency, NotConsistentThenSuccess) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return make_ready_future(MakeResponse(false));
      })
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return make_ready_future(MakeResponse(true));
      });

  auto status = AsyncWaitForConsistency(background.cq(), client, table_name,
                                        token, TestOptions())
                    .get();
  EXPECT_STATUS_OK(status);
}

TEST(AsyncWaitForConsistency, PermanentFailure) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return make_ready_future<RespType>(
            Status(StatusCode::kPermissionDenied, "fail"));
      });

  auto status = AsyncWaitForConsistency(background.cq(), client, table_name,
                                        token, TestOptions())
                    .get();
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied, "fail"));
}

TEST(AsyncWaitForConsistency, TooManyTransientFailures) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .Times(kLimitedErrorCount + 1)
      .WillRepeatedly([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return make_ready_future<RespType>(
            Status(StatusCode::kUnavailable, "try again"));
      });

  auto status = AsyncWaitForConsistency(background.cq(), client, table_name,
                                        token, TestOptions())
                    .get();
  EXPECT_THAT(status,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try again")));
}

TEST(AsyncWaitForConsistency, NeverConsistent) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .Times(kLimitedErrorCount + 1)
      .WillRepeatedly([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return make_ready_future(MakeResponse(false));
      });

  auto status = AsyncWaitForConsistency(background.cq(), client, table_name,
                                        token, TestOptions())
                    .get();
  EXPECT_THAT(status, StatusIs(StatusCode::kDeadlineExceeded,
                               HasSubstr("Polling loop terminated")));
}

TEST(AsyncWaitForConsistency, PassesOptionsToConnection) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  struct TestOption {
    using Type = std::string;
  };

  CompletionQueue cq;
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        EXPECT_TRUE(internal::CurrentOptions().has<TestOption>());
        return make_ready_future(MakeResponse(true));
      });

  EXPECT_FALSE(internal::CurrentOptions().has<TestOption>());
  auto status = AsyncWaitForConsistency(cq, client, table_name, token,
                                        Options{}.set<TestOption>("value"))
                    .get();
  EXPECT_STATUS_OK(status);
}

class AsyncWaitForConsistencyCancelTest : public ::testing::Test {
 protected:
  std::size_t RequestCancelCount() { return sequencer_.CancelCount("Request"); }
  std::size_t TimerCancelCount() { return sequencer_.CancelCount("Timer"); }

  future<RespType> SimulateRequest() {
    return sequencer_.PushBack("Request").then(
        [](future<Status> g) -> RespType {
          auto status = g.get();
          if (!status.ok()) return status;
          return MakeResponse(true);
        });
  }

  future<TimerResult> SimulateTimer(std::chrono::nanoseconds d) {
    using std::chrono::system_clock;
    auto tp = system_clock::now() +
              std::chrono::duration_cast<system_clock::duration>(d);
    return sequencer_.PushBack("Timer").then(
        [tp](future<Status> g) -> TimerResult {
          auto status = g.get();
          if (!status.ok()) return status;
          return tp;
        });
  }

  promise<Status> WaitForRequest() {
    auto p = sequencer_.PopFrontWithName();
    EXPECT_EQ("Request", p.second);
    return std::move(p.first);
  }
  promise<Status> WaitForTimer() {
    auto p = sequencer_.PopFrontWithName();
    EXPECT_THAT(p.second, "Timer");
    return std::move(p.first);
  }

  std::shared_ptr<testing_util::MockCompletionQueueImpl>
  MakeMockCompletionQueue() {
    auto mock = std::make_shared<testing_util::MockCompletionQueueImpl>();
    EXPECT_CALL(*mock, MakeRelativeTimer)
        .WillRepeatedly(
            [this](std::chrono::nanoseconds d) { return SimulateTimer(d); });
    return mock;
  }

 private:
  AsyncSequencer<Status> sequencer_;
};

TEST_F(AsyncWaitForConsistencyCancelTest, CancelAndSuccess) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock_cq = MakeMockCompletionQueue();
  CompletionQueue cq(mock_cq);
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .Times(2)
      .WillRepeatedly([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return SimulateRequest();
      });

  auto actual =
      AsyncWaitForConsistency(cq, client, table_name, token, TestOptions());

  // First simulate a regular request that results in a transient failure.
  auto p = WaitForRequest();
  p.set_value(transient);
  // Then simulate the backoff timer expiring.
  p = WaitForTimer();
  p.set_value(Status{});
  // Then another request that gets cancelled.
  p = WaitForRequest();
  EXPECT_EQ(0, RequestCancelCount());
  EXPECT_EQ(0, TimerCancelCount());
  actual.cancel();
  EXPECT_EQ(1, RequestCancelCount());
  EXPECT_EQ(0, TimerCancelCount());
  p.set_value(Status{});
  auto value = actual.get();
  ASSERT_STATUS_OK(value);
}

TEST_F(AsyncWaitForConsistencyCancelTest, CancelWithFailure) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock_cq = MakeMockCompletionQueue();
  CompletionQueue cq(mock_cq);
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .Times(2)
      .WillRepeatedly([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return SimulateRequest();
      });

  auto actual =
      AsyncWaitForConsistency(cq, client, table_name, token, TestOptions());

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);
  // Then simulate the backoff timer expiring.
  p = WaitForTimer();
  p.set_value(Status{});
  // This triggers a second request, which is called and fails too
  p = WaitForRequest();
  EXPECT_EQ(0, RequestCancelCount());
  EXPECT_EQ(0, TimerCancelCount());
  actual.cancel();
  EXPECT_EQ(1, RequestCancelCount());
  EXPECT_EQ(0, TimerCancelCount());
  p.set_value(transient);
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kCancelled,
                              HasSubstr("Operation cancelled")));
}

TEST_F(AsyncWaitForConsistencyCancelTest, CancelDuringTimer) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock_cq = MakeMockCompletionQueue();
  CompletionQueue cq(mock_cq);
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return SimulateRequest();
      });

  auto actual =
      AsyncWaitForConsistency(cq, client, table_name, token, TestOptions());

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);

  // Wait for the timer to be set
  p = WaitForTimer();
  // At this point there is a timer in the completion queue, cancel the call and
  // simulate a cancel for the timer.
  EXPECT_EQ(0, RequestCancelCount());
  EXPECT_EQ(0, TimerCancelCount());
  actual.cancel();
  EXPECT_EQ(0, RequestCancelCount());
  EXPECT_EQ(1, TimerCancelCount());
  p.set_value(Status(StatusCode::kCancelled, "timer cancel"));
  // the retry loop should *not* create any more calls, the value should be
  // available immediately.
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kCancelled,
                              HasSubstr("Operation cancelled")));
}

TEST_F(AsyncWaitForConsistencyCancelTest, ShutdownDuringTimer) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock_cq = MakeMockCompletionQueue();
  CompletionQueue cq(mock_cq);
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return SimulateRequest();
      });

  auto actual =
      AsyncWaitForConsistency(cq, client, table_name, token, TestOptions());

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);

  // Wait for the timer to be set
  p = WaitForTimer();

  // At this point there is a timer in the completion queue, simulate a
  // CancelAll() + Shutdown().
  EXPECT_CALL(*mock_cq, CancelAll).Times(1);
  EXPECT_CALL(*mock_cq, Shutdown).Times(1);
  cq.CancelAll();
  cq.Shutdown();
  p.set_value(Status(StatusCode::kCancelled, "timer cancelled"));

  // the retry loop should exit
  auto value = actual.get();
  EXPECT_THAT(value,
              StatusIs(StatusCode::kCancelled, HasSubstr("timer cancelled")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google
