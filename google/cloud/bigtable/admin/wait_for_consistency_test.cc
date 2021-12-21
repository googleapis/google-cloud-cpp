// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/admin/wait_for_consistency.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_options.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
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

Options SlowTestOptions() {
  using std::chrono::hours;
  auto retry =
      BigtableTableAdminLimitedErrorCountRetryPolicy(kLimitedErrorCount);
  auto backoff = ExponentialBackoffPolicy(hours(24), hours(24), 2.0);
  auto polling =
      GenericPollingPolicy<BigtableTableAdminRetryPolicyOption::Type,
                           BigtableTableAdminBackoffPolicyOption::Type>(
          retry.clone(), backoff.clone());

  return Options{}.set<BigtableTableAdminPollingPolicyOption>(polling.clone());
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
        btadmin::CheckConsistencyResponse response;
        response.set_consistent(true);
        return make_ready_future(make_status_or(response));
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
        btadmin::CheckConsistencyResponse response;
        response.set_consistent(false);
        return make_ready_future(make_status_or(response));
      })
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        btadmin::CheckConsistencyResponse response;
        response.set_consistent(true);
        return make_ready_future(make_status_or(response));
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
        return make_ready_future<StatusOr<btadmin::CheckConsistencyResponse>>(
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
        return make_ready_future<StatusOr<btadmin::CheckConsistencyResponse>>(
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
        btadmin::CheckConsistencyResponse response;
        response.set_consistent(false);
        return make_ready_future(make_status_or(response));
      });

  auto status = AsyncWaitForConsistency(background.cq(), client, table_name,
                                        token, TestOptions())
                    .get();
  EXPECT_THAT(status, StatusIs(StatusCode::kDeadlineExceeded,
                               HasSubstr("Polling loop terminated")));
}

class AsyncWaitForConsistencyCancelTest : public ::testing::Test {
 protected:
  int CancelCount() {
    std::lock_guard<std::mutex> lk(mu_);
    int n = cancel_count_;
    cancel_count_ = 0;
    return n;
  }

  future<StatusOr<btadmin::CheckConsistencyResponse>> SimulateRequest() {
    promise<bool> p([this] {
      std::lock_guard<std::mutex> lk(mu_);
      ++cancel_count_;
    });
    auto f = p.get_future().then([](future<bool> g) {
      btadmin::CheckConsistencyResponse r;
      r.set_consistent(g.get());
      return make_status_or(r);
    });
    std::lock_guard<std::mutex> lk(mu_);
    requests_.push_back(std::move(p));
    cv_.notify_one();
    return f;
  }

  promise<bool> WaitForRequest() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return !requests_.empty(); });
    auto p = std::move(requests_.front());
    requests_.pop_front();
    return p;
  }

 private:
  // We will simulate the asynchronous requests by pushing promises into this
  // queue. The test pulls element from the queue to verify the work.
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<promise<bool>> requests_;
  int cancel_count_ = 0;
};

TEST_F(AsyncWaitForConsistencyCancelTest, CancelAndSuccess) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  CompletionQueue cq(fake);
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

  // First simulate a regular request, that is not consistent.
  auto p = WaitForRequest();
  p.set_value(false);
  // Then simulate the backoff timer expiring.
  fake->SimulateCompletion(true);
  // Then another request that gets cancelled, but is consistent.
  p = WaitForRequest();
  EXPECT_EQ(0, CancelCount());
  actual.cancel();
  EXPECT_EQ(1, CancelCount());
  p.set_value(true);
  auto value = actual.get();
  ASSERT_STATUS_OK(value);
}

TEST_F(AsyncWaitForConsistencyCancelTest, CancelWithFailure) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  CompletionQueue cq(fake);
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
  p.set_value(false);
  // Then simulate the backoff timer expiring.
  fake->SimulateCompletion(true);
  p = WaitForRequest();
  EXPECT_EQ(0, CancelCount());
  actual.cancel();
  EXPECT_EQ(1, CancelCount());
  p.set_value(false);
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kCancelled,
                              HasSubstr("Operation cancelled")));
}

TEST_F(AsyncWaitForConsistencyCancelTest, CancelDuringTimer) {
  std::string const table_name =
      bigtable::TableName("test-project", "test-instance", "test-table");
  std::string const token = "test-token";

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  CompletionQueue cq(fake);
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return SimulateRequest();
      });

  auto actual =
      AsyncWaitForConsistency(cq, client, table_name, token, SlowTestOptions());

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(false);
  // At this point there is a timer in the completion queue, cancel the call and
  // simulate a cancel for the timer.
  EXPECT_EQ(0, CancelCount());
  actual.cancel();
  EXPECT_EQ(0, CancelCount());
  fake->SimulateCompletion(false);
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

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  CompletionQueue cq(fake);
  auto mock = std::make_shared<MockConnection>();
  auto client = BigtableTableAdminClient(mock);

  EXPECT_CALL(*mock, AsyncCheckConsistency)
      .WillOnce([&](btadmin::CheckConsistencyRequest const& request) {
        EXPECT_EQ(request.name(), table_name);
        EXPECT_EQ(request.consistency_token(), token);
        return SimulateRequest();
      });

  auto actual =
      AsyncWaitForConsistency(cq, client, table_name, token, SlowTestOptions());

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(false);
  // At this point there is a timer in the completion queue, simulate a
  // CancelAll() + Shutdown().
  fake->CancelAll();
  fake->Shutdown();
  // the retry loop should exit
  auto value = actual.get();
  EXPECT_THAT(value,
              StatusIs(StatusCode::kCancelled, HasSubstr("timer canceled")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google
