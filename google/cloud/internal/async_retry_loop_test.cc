// Copyright 2020 Google LLC
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

#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Return;

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

auto constexpr kMaxRetries = 5;
std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return LimitedErrorCountRetryPolicy<TestRetryablePolicy>(kMaxRetries).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

class AsyncRetryLoopCancelTest : public ::testing::Test {
 protected:
  int CancelCount() {
    std::lock_guard<std::mutex> lk(mu_);
    int n = cancel_count_;
    cancel_count_ = 0;
    return n;
  }

  future<StatusOr<int>> SimulateRequest(int x) {
    promise<Status> p([this] {
      std::lock_guard<std::mutex> lk(mu_);
      ++cancel_count_;
    });
    auto f = p.get_future().then([x](future<Status> g) -> StatusOr<int> {
      auto status = g.get();
      if (!status.ok()) return status;
      return 2 * x;
    });
    std::lock_guard<std::mutex> lk(mu_);
    requests_.push_back(std::move(p));
    cv_.notify_one();
    return f;
  }

  promise<Status> WaitForRequest() {
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
  std::deque<promise<Status>> requests_;
  int cancel_count_ = 0;
};

TEST(AsyncRetryLoopTest, Success) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             int request) -> future<StatusOr<int>> {
            return make_ready_future(StatusOr<int>(2 * request));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          background.cq(),
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, int request) {
            if (++counter < 3) {
              return make_ready_future(
                  StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
            }
            return make_ready_future(StatusOr<int>(2 * request));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  Status actual = AsyncRetryLoop(
                      TestRetryPolicy(), TestBackoffPolicy(),
                      Idempotency::kIdempotent, background.cq(),
                      [&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>, int) {
                        if (++counter <= 3) {
                          return make_ready_future(Status(
                              StatusCode::kResourceExhausted, "slow-down"));
                        }
                        return make_ready_future(Status());
                      },
                      42, "error message")
                      .get();
  ASSERT_THAT(actual, IsOk());
}

class MockBackoffPolicy : public BackoffPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<BackoffPolicy>, clone, (), (const, override));
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion, (), (override));
};

/// @test Verify the backoff policy is queried after each failure.
TEST(AsyncRetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion()).Times(3).WillRepeatedly(Return(ms(1)));

  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), std::move(mock), Idempotency::kIdempotent,
          background.cq(),
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, int request) {
            if (++counter <= 3) {
              return make_ready_future(
                  StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
            }
            return make_ready_future(StatusOr<int>(2 * request));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, TransientFailureNonIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
          background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               AllOf(HasSubstr("test-message-try-again"),
                                     HasSubstr("Error in non-idempotent"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRetryLoopTest, PermanentFailureIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kPermissionDenied, "test-message-uh-oh")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied,
                               AllOf(HasSubstr("test-message-uh-oh"),
                                     HasSubstr("Permanent error in"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRetryLoopTest, TooManyTransientFailuresIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               AllOf(HasSubstr("test-message-try-again"),
                                     HasSubstr("Retry policy exhausted"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRetryLoopTest, ExhaustedDuringBackoff) {
  using ms = std::chrono::milliseconds;
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          LimitedTimeRetryPolicy<TestRetryablePolicy>(ms(10)).clone(),
          ExponentialBackoffPolicy(ms(20), ms(20), 2.0).clone(),
          Idempotency::kIdempotent, background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               AllOf(HasSubstr("test-message-try-again"),
                                     HasSubstr("Retry policy exhausted"),
                                     HasSubstr("test-location"))));
}

class RetryPolicyWithSetup {
 public:
  virtual ~RetryPolicyWithSetup() = default;
  virtual bool OnFailure(Status const&) = 0;
  virtual void Setup(grpc::ClientContext&) const = 0;
  virtual bool IsExhausted() const = 0;
  virtual bool IsPermanentFailure(Status const&) const = 0;
};

class MockRetryPolicy : public RetryPolicyWithSetup {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext&), (const, override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
};

TEST(AsyncRetryLoopTest, SetsTimeout) {
  auto mock = absl::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, IsExhausted)
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, IsPermanentFailure).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock, Setup).Times(3);

  AutomaticallyCreatedBackgroundThreads background;

  StatusOr<int> actual =
      AsyncRetryLoop(
          std::unique_ptr<RetryPolicyWithSetup>(std::move(mock)),
          TestBackoffPolicy(), Idempotency::kIdempotent, background.cq(),
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, int /*request*/) {
            return make_ready_future(
                StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(AsyncRetryLoopCancelTest, CancelAndSuccess) {
  using ms = std::chrono::milliseconds;

  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  google::cloud::CompletionQueue cq(fake);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone(),
      ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone(),
      Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             int x) { return SimulateRequest(x); },
      42, "test-location");

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);
  // Then simulate the backoff timer expiring.
  fake->SimulateCompletion(/*ok=*/true);
  // Then another request that gets cancelled.
  p = WaitForRequest();
  EXPECT_EQ(0, CancelCount());
  actual.cancel();
  EXPECT_EQ(1, CancelCount());
  p.set_value({});
  auto value = actual.get();
  ASSERT_THAT(value, IsOk());
  EXPECT_EQ(84, *value);
}

TEST_F(AsyncRetryLoopCancelTest, CancelWithFailure) {
  using ms = std::chrono::milliseconds;

  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  google::cloud::CompletionQueue cq(fake);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone(),
      ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone(),
      Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             int x) { return SimulateRequest(x); },
      42, "test-location");

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);
  // Then simulate the backoff timer expiring.
  fake->SimulateCompletion(/*ok=*/true);
  p = WaitForRequest();
  EXPECT_EQ(0, CancelCount());
  actual.cancel();
  EXPECT_EQ(1, CancelCount());
  p.set_value(transient);
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kUnavailable,
                              AllOf(HasSubstr("try-again"),
                                    HasSubstr("Retry loop cancelled"),
                                    HasSubstr("test-location"))));
}

TEST_F(AsyncRetryLoopCancelTest, CancelDuringTimer) {
  using ms = std::chrono::milliseconds;

  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  google::cloud::CompletionQueue cq(fake);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone(),
      ExponentialBackoffPolicy(ms(2000), ms(8000), 2.0).clone(),
      Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             int x) { return SimulateRequest(x); },
      42, "test-location");

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);
  // At this point there is a timer in the completion queue, cancel the call and
  // simulate a cancel for the timer.
  EXPECT_EQ(0, CancelCount());
  actual.cancel();
  EXPECT_EQ(0, CancelCount());
  fake->SimulateCompletion(/*ok=*/false);
  // the retry loop should *not* create any more calls, the value should be
  // available immediately.
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kUnavailable,
                              AllOf(HasSubstr("try-again"),
                                    HasSubstr("Retry loop cancelled"),
                                    HasSubstr("test-location"))));
}

TEST_F(AsyncRetryLoopCancelTest, ShutdownDuringTimer) {
  using ms = std::chrono::milliseconds;

  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto fake = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  google::cloud::CompletionQueue cq(fake);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone(),
      ExponentialBackoffPolicy(ms(2000), ms(8000), 2.0).clone(),
      Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             int x) { return SimulateRequest(x); },
      42, "test-location");

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);
  // At this point there is a timer in the completion queue, simulate a
  // CancelAll() + Shutdown().
  fake->CancelAll();
  fake->Shutdown();
  // the retry loop should exit
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kCancelled,
                              AllOf(HasSubstr("Timer failure in"),
                                    HasSubstr("test-location"))));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
