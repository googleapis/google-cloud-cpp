// Copyright 2023 Google LLC
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

#include "google/cloud/internal/async_rest_retry_loop.h"
#include "google/cloud/internal/rest_background_threads_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Return;

struct TestOption {
  using Type = std::string;
};

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

auto constexpr kMaxRetries = 5;
std::unique_ptr<internal::RetryPolicy> TestRetryPolicy() {
  return internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>(
             kMaxRetries)
      .clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

using TimerResult = StatusOr<std::chrono::system_clock::time_point>;

class AsyncRestRetryLoopCancelTest : public ::testing::Test {
 protected:
  std::size_t RequestCancelCount() { return sequencer_.CancelCount("Request"); }
  std::size_t TimerCancelCount() { return sequencer_.CancelCount("Timer"); }

  future<StatusOr<int>> SimulateRequest(int x) {
    return sequencer_.PushBack("Request").then(
        [x](future<Status> g) -> StatusOr<int> {
          auto status = g.get();
          if (!status.ok()) return status;
          return 2 * x;
        });
  }

  future<TimerResult> SimulateRelativeTimer(std::chrono::nanoseconds d) {
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
        .WillRepeatedly([this](std::chrono::nanoseconds d) {
          return SimulateRelativeTimer(d);
        });
    return mock;
  }

 private:
  AsyncSequencer<Status> sequencer_;
};

TEST(AsyncRestRetryLoopTest, Success) {
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(Options{}.set<TestOption>("Success"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "Success");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [](CompletionQueue&, RestContext&, int request) -> future<StatusOr<int>> {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "Success");
        return make_ready_future(StatusOr<int>(2 * request));
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRestRetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(Options{}.set<TestOption>("TransientThenSuccess"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
            "TransientThenSuccess");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&](CompletionQueue&, RestContext&, int request) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "TransientThenSuccess");
        if (++counter < 3) {
          return make_ready_future(
              StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
        }
        return make_ready_future(StatusOr<int>(2 * request));
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRestRetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(Options{}.set<TestOption>("ReturnJustStatus"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "ReturnJustStatus");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&](CompletionQueue&, RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "ReturnJustStatus");
        if (++counter <= 3) {
          return make_ready_future(
              Status(StatusCode::kResourceExhausted, "slow-down"));
        }
        return make_ready_future(Status());
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  Status actual = pending.get();
  ASSERT_THAT(actual, IsOk());
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

/// @test Verify the backoff policy is queried after each failure.
TEST(AsyncRestRetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion).Times(3).WillRepeatedly([&] {
    return ms(1);
  });

  int counter = 0;
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(Options{}.set<TestOption>("UsesBackoffPolicy"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "UsesBackoffPolicy");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), std::move(mock), Idempotency::kIdempotent,
      background.cq(),
      [&](CompletionQueue&, RestContext&, int request) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "UsesBackoffPolicy");
        if (++counter <= 3) {
          return make_ready_future(
              StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
        }
        return make_ready_future(StatusOr<int>(2 * request));
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRestRetryLoopTest, TransientFailureNonIdempotent) {
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(
      Options{}.set<TestOption>("TransientFailureNonIdempotent"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
            "TransientFailureNonIdempotent");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
      background.cq(),
      [](CompletionQueue&, RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "TransientFailureNonIdempotent");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kUnavailable, "test-message-try-again")));
      },
      42, "test-location");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               AllOf(HasSubstr("test-message-try-again"),
                                     HasSubstr("Error in non-idempotent"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRestRetryLoopTest, PermanentFailureIdempotent) {
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(
      Options{}.set<TestOption>("PermanentFailureIdempotent"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
            "PermanentFailureIdempotent");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [](CompletionQueue&, RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "PermanentFailureIdempotent");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kPermissionDenied, "test-message-uh-oh")));
      },
      42, "test-location");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied,
                               AllOf(HasSubstr("test-message-uh-oh"),
                                     HasSubstr("Permanent error in"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRestRetryLoopTest, TooManyTransientFailuresIdempotent) {
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(
      Options{}.set<TestOption>("TooManyTransientFailuresIdempotent"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
            "TooManyTransientFailuresIdempotent");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [](CompletionQueue&, RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "TooManyTransientFailuresIdempotent");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kUnavailable, "test-message-try-again")));
      },
      42, "test-location");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               AllOf(HasSubstr("test-message-try-again"),
                                     HasSubstr("Retry policy exhausted"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRestRetryLoopTest, ExhaustedDuringBackoff) {
  using ms = std::chrono::milliseconds;
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(
      Options{}.set<TestOption>("ExhaustedDuringBackoff"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
            "ExhaustedDuringBackoff");
  AutomaticallyCreatedRestBackgroundThreads background;
  auto pending = AsyncRestRetryLoop(
      internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>(0).clone(),
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0).clone(),
      Idempotency::kIdempotent, background.cq(),
      [](CompletionQueue&, RestContext&, int) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(),
                  "ExhaustedDuringBackoff");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kUnavailable, "test-message-try-again")));
      },
      42, "test-location");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               AllOf(HasSubstr("test-message-try-again"),
                                     HasSubstr("Retry policy exhausted"),
                                     HasSubstr("test-location"))));
}

TEST(AsyncRestRetryLoopTest, ExhaustedBeforeStart) {
  auto mock = absl::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock, IsExhausted)
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, OnFailure).WillOnce(Return(true));
  EXPECT_CALL(*mock, IsPermanentFailure).WillRepeatedly(Return(false));

  AutomaticallyCreatedRestBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRestRetryLoop(
          std::unique_ptr<RetryPolicyWithSetup>(std::move(mock)),
          TestBackoffPolicy(), Idempotency::kIdempotent, background.cq(),
          [](CompletionQueue&, RestContext&, int) {
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

TEST(AsyncRestRetryLoopTest, SetsTimeout) {
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

  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "");
  internal::OptionsSpan span(Options{}.set<TestOption>("SetsTimeout"));
  EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "SetsTimeout");
  AutomaticallyCreatedRestBackgroundThreads background;

  auto pending = AsyncRestRetryLoop(
      std::unique_ptr<RetryPolicyWithSetup>(std::move(mock)),
      TestBackoffPolicy(), Idempotency::kIdempotent, background.cq(),
      [&](CompletionQueue&, RestContext&, int /*request*/) {
        EXPECT_EQ(internal::CurrentOptions().get<TestOption>(), "SetsTimeout");
        return make_ready_future(
            StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
      },
      42, "error message");
  internal::OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(AsyncRestRetryLoopCancelTest, CancelAndSuccess) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  google::cloud::CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](CompletionQueue&, RestContext&, int x) {
        return SimulateRequest(x);
      },
      42, "test-location");

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
  ASSERT_THAT(value, IsOk());
  EXPECT_EQ(84, *value);
}

TEST_F(AsyncRestRetryLoopCancelTest, CancelWithFailure) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](CompletionQueue&, RestContext&, int x) {
        return SimulateRequest(x);
      },
      42, "test-location");

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
  EXPECT_THAT(value, StatusIs(StatusCode::kUnavailable,
                              AllOf(HasSubstr("try-again"),
                                    HasSubstr("Retry loop cancelled"),
                                    HasSubstr("test-location"))));
}

TEST_F(AsyncRestRetryLoopCancelTest, CancelDuringTimer) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](CompletionQueue&, RestContext&, int x) {
        return SimulateRequest(x);
      },
      42, "test-location");

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
  EXPECT_THAT(value, StatusIs(StatusCode::kUnavailable,
                              AllOf(HasSubstr("try-again"),
                                    HasSubstr("Retry loop cancelled"),
                                    HasSubstr("test-location"))));
}

TEST_F(AsyncRestRetryLoopCancelTest, ShutdownDuringTimer) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRestRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](CompletionQueue&, RestContext&, int x) {
        return SimulateRequest(x);
      },
      42, "test-location");

  // First simulate a regular request.
  auto p = WaitForRequest();
  p.set_value(transient);

  // Wait for the timer to be set
  p = WaitForTimer();

  // At this point there is a timer in the completion queue, simulate a
  // CancelAll() + Shutdown().
  EXPECT_CALL(*mock, CancelAll).Times(1);
  EXPECT_CALL(*mock, Shutdown).Times(1);
  cq.CancelAll();
  cq.Shutdown();
  p.set_value(Status(StatusCode::kCancelled, "timer cancelled"));

  // the retry loop should exit
  auto value = actual.get();
  EXPECT_THAT(value, StatusIs(StatusCode::kCancelled,
                              AllOf(HasSubstr("Timer failure in"),
                                    HasSubstr("test-location"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
