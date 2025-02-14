// Copyright 2020 Google LLC
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

#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Pair;
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
std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return LimitedErrorCountRetryPolicy<TestRetryablePolicy>(kMaxRetries).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

using TimerResult = StatusOr<std::chrono::system_clock::time_point>;

class AsyncRetryLoopCancelTest : public ::testing::Test {
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

TEST(AsyncRetryLoopTest, SuccessWithExplicitOptions) {
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [](google::cloud::CompletionQueue&, auto, ImmutableOptions const& options,
         int request) -> future<StatusOr<int>> {
        EXPECT_EQ(options->get<TestOption>(), "Success");
        return make_ready_future(StatusOr<int>(2 * request));
      },
      MakeImmutableOptions(Options{}.set<TestOption>("Success")),
      /*request=*/42, /*location=*/"error message");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&](google::cloud::CompletionQueue&, auto,
          ImmutableOptions const& options, int request) {
        EXPECT_EQ(options->get<TestOption>(), "TransientThenSuccess");
        if (++counter < 3) {
          return make_ready_future(
              StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
        }
        return make_ready_future(StatusOr<int>(2 * request));
      },
      MakeImmutableOptions(Options{}.set<TestOption>("TransientThenSuccess")),
      42, "error message");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&](google::cloud::CompletionQueue&, auto,
          ImmutableOptions const& options, int) {
        EXPECT_EQ(options->get<TestOption>(), "ReturnJustStatus");
        if (++counter <= 3) {
          return make_ready_future(
              Status(StatusCode::kResourceExhausted, "slow-down"));
        }
        return make_ready_future(Status());
      },
      MakeImmutableOptions(Options{}.set<TestOption>("ReturnJustStatus")), 42,
      "error message");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  Status actual = pending.get();
  ASSERT_THAT(actual, IsOk());
}

class RetryPolicyWithSetup : public RetryPolicy {
 public:
  ~RetryPolicyWithSetup() override = default;
  virtual void Setup(grpc::ClientContext&) const = 0;
};

class MockRetryPolicy : public RetryPolicyWithSetup {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext&), (const, override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
  MOCK_METHOD(std::unique_ptr<google::cloud::RetryPolicy>, clone, (),
              (const, override));
};

/// @test Verify the backoff policy is queried after each failure.
TEST(AsyncRetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion()).Times(3).WillRepeatedly(Return(ms(1)));

  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), std::move(mock), Idempotency::kIdempotent,
      background.cq(),
      [&](google::cloud::CompletionQueue&, auto,
          ImmutableOptions const& options, int request) {
        EXPECT_EQ(options->get<TestOption>(), "UsesBackoffPolicy");
        if (++counter <= 3) {
          return make_ready_future(
              StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
        }
        return make_ready_future(StatusOr<int>(2 * request));
      },
      MakeImmutableOptions(Options{}.set<TestOption>("UsesBackoffPolicy")), 42,
      "error message");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), IsOk());
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, TransientFailureNonIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
      background.cq(),
      [](google::cloud::CompletionQueue&, auto, ImmutableOptions const& options,
         int) {
        EXPECT_EQ(options->get<TestOption>(), "TransientFailureNonIdempotent");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kUnavailable, "test-message-try-again")));
      },
      MakeImmutableOptions(
          Options{}.set<TestOption>("TransientFailureNonIdempotent")),
      42, "test-location");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("test-message-try-again")));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.original-message",
                                      "test-message-try-again")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.reason", "non-idempotent")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST(AsyncRetryLoopTest, PermanentFailureIdempotent) {
  EXPECT_EQ(CurrentOptions().get<TestOption>(), "");
  OptionsSpan span(Options{}.set<TestOption>("PermanentFailureIdempotent"));
  EXPECT_EQ(CurrentOptions().get<TestOption>(), "PermanentFailureIdempotent");
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [](google::cloud::CompletionQueue&, auto, ImmutableOptions const& options,
         int) {
        EXPECT_EQ(options->get<TestOption>(), "PermanentFailureIdempotent");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kPermissionDenied, "test-message-uh-oh")));
      },
      MakeImmutableOptions(
          Options{}.set<TestOption>("PermanentFailureIdempotent")),
      42, "test-location");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("test-message-uh-oh")));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.original-message",
                                      "test-message-uh-oh")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.reason", "permanent-error")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST(AsyncRetryLoopTest, TooManyTransientFailuresIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [](google::cloud::CompletionQueue&, auto, ImmutableOptions const& options,
         int) {
        EXPECT_EQ(options->get<TestOption>(),
                  "TooManyTransientFailuresIdempotent");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kUnavailable, "test-message-try-again")));
      },
      MakeImmutableOptions(
          Options{}.set<TestOption>("TooManyTransientFailuresIdempotent")),
      42, "test-location");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("test-message-try-again")));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.original-message",
                                      "test-message-try-again")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.on-entry", "false")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST(AsyncRetryLoopTest, ExhaustedDuringBackoff) {
  using ms = std::chrono::milliseconds;
  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      LimitedErrorCountRetryPolicy<TestRetryablePolicy>(0).clone(),
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0).clone(),
      Idempotency::kIdempotent, background.cq(),
      [](google::cloud::CompletionQueue&, auto, ImmutableOptions const& options,
         int) {
        EXPECT_EQ(options->get<TestOption>(), "ExhaustedDuringBackoff");
        return make_ready_future(StatusOr<int>(
            Status(StatusCode::kUnavailable, "test-message-try-again")));
      },
      MakeImmutableOptions(Options{}.set<TestOption>("ExhaustedDuringBackoff")),
      42, "test-location");
  StatusOr<int> actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("test-message-try-again")));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.on-entry", "false")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST(AsyncRetryLoopTest, ExhaustedBeforeStart) {
  auto mock = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock, IsExhausted).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, OnFailure).Times(0);
  EXPECT_CALL(*mock, IsPermanentFailure).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock, Setup).Times(0);

  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          std::unique_ptr<RetryPolicyWithSetup>(std::move(mock)),
          TestBackoffPolicy(), Idempotency::kIdempotent, background.cq(),
          [](google::cloud::CompletionQueue&, auto, auto const&, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          MakeImmutableOptions(Options{}), 42, "test-location")
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kDeadlineExceeded));
  auto const& metadata = actual.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.on-entry", "true")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST(AsyncRetryLoopTest, HeedsRetryInfo) {
  MockFunction<future<StatusOr<int>>(CompletionQueue&,
                                     std::shared_ptr<grpc::ClientContext>,
                                     ImmutableOptions, int)>
      mock;
  EXPECT_CALL(mock, Call)
      .WillOnce([] {
        auto status = ResourceExhaustedError("try again");
        SetRetryInfo(status, RetryInfo{std::chrono::seconds(0)});
        return make_ready_future<StatusOr<int>>(status);
      })
      .WillOnce(Return(make_ready_future<StatusOr<int>>(5)));

  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kNonIdempotent,
      background.cq(), mock.AsStdFunction(),
      MakeImmutableOptions(Options{}.set<EnableServerRetriesOption>(true)), 42,
      "test-location");
  EXPECT_THAT(pending.get(), IsOkAndHolds(5));
}

TEST(AsyncRetryLoopTest, SetsTimeout) {
  auto mock = std::make_unique<MockRetryPolicy>();
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

  EXPECT_EQ(CurrentOptions().get<TestOption>(), "");
  OptionsSpan span(Options{}.set<TestOption>("SetsTimeout"));
  EXPECT_EQ(CurrentOptions().get<TestOption>(), "SetsTimeout");
  AutomaticallyCreatedBackgroundThreads background;

  auto pending = AsyncRetryLoop(
      std::unique_ptr<RetryPolicyWithSetup>(std::move(mock)),
      TestBackoffPolicy(), Idempotency::kIdempotent, background.cq(),
      [&](google::cloud::CompletionQueue&, auto, ImmutableOptions const&,
          int /*request*/) {
        EXPECT_EQ(CurrentOptions().get<TestOption>(), "SetsTimeout");
        return make_ready_future(
            StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
      },
      MakeImmutableOptions(Options{}), 42, "error message");
  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  StatusOr<int> actual = pending.get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kUnavailable));
}

TEST(AsyncRetryLoopTest, ConfigureContext) {
  AsyncSequencer<StatusOr<int>> sequencer;

  // The original options should be used in the first attempt and in the retry
  // attempt.
  MockFunction<void(grpc::ClientContext&)> setup;
  EXPECT_CALL(setup, Call).Times(2);

  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&sequencer](auto&, auto, auto const&, auto const&) {
        return sequencer.PushBack();
      },
      MakeImmutableOptions(
          Options{}.set<GrpcSetupOption>(setup.AsStdFunction())),
      42, "error message");

  // Clear the current options before retrying.
  sequencer.PopFront().set_value(Status(StatusCode::kUnavailable, "try again"));
  sequencer.PopFront().set_value(0);
  (void)pending.get();
}

TEST(AsyncRetryLoopTest, CallOptionsDuringCancel) {
  promise<StatusOr<int>> p([] {
    EXPECT_EQ(CurrentOptions().get<TestOption>(), "CallOptionsDuringCancel");
  });

  AutomaticallyCreatedBackgroundThreads background;
  auto pending = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&p](auto&, auto, auto const&, auto const&) { return p.get_future(); },
      MakeImmutableOptions(
          Options{}.set<TestOption>("CallOptionsDuringCancel")),
      42, "error message");

  OptionsSpan overlay(Options{}.set<TestOption>("uh-oh"));
  pending.cancel();
  p.set_value(0);
  (void)pending.get();
}

TEST_F(AsyncRetryLoopCancelTest, CancelAndSuccess) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  google::cloud::CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&, auto, auto const&, int x) {
        return SimulateRequest(x);
      },
      MakeImmutableOptions(Options{}), 42, "test-location");

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

TEST_F(AsyncRetryLoopCancelTest, CancelWithFailure) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  google::cloud::CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&, auto, auto const&, int x) {
        return SimulateRequest(x);
      },
      MakeImmutableOptions(Options{}), 42, "test-location");

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
  EXPECT_THAT(value,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
  auto const& metadata = value.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST_F(AsyncRetryLoopCancelTest, CancelDuringTimer) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  google::cloud::CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&, auto, auto const&, int x) {
        return SimulateRequest(x);
      },
      MakeImmutableOptions(Options{}), 42, "test-location");

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
  EXPECT_THAT(value,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
  auto const& metadata = value.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

TEST_F(AsyncRetryLoopCancelTest, ShutdownDuringTimer) {
  auto const transient = Status(StatusCode::kUnavailable, "try-again");

  auto mock = MakeMockCompletionQueue();
  google::cloud::CompletionQueue cq(mock);
  future<StatusOr<int>> actual = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent, cq,
      [this](google::cloud::CompletionQueue&, auto, auto const&, int x) {
        return SimulateRequest(x);
      },
      MakeImmutableOptions(Options{}), 42, "test-location");

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
  EXPECT_THAT(value, StatusIs(StatusCode::kCancelled));
  auto const& metadata = value.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
  EXPECT_THAT(metadata,
              Contains(Pair("gcloud-cpp.retry.function", "test-location")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::IsActive;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::SizeIs;

TEST(AsyncRetryLoopTest, TracedBackoff) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  AsyncSequencer<bool> sequencer;
  AutomaticallyCreatedBackgroundThreads background;
  auto actual = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&](auto, auto, auto const&, auto) {
        return sequencer.PushBack().then(
            [](auto) { return StatusOr<int>(UnavailableError("try again")); });
      },
      MakeImmutableOptions(EnableTracing(Options{})), 42, "error message");

  OptionsSpan overlay(Options{});
  for (auto i = 0; i != kMaxRetries + 1; ++i) {
    sequencer.PopFront().set_value(true);
  }
  EXPECT_THAT(actual.get(), StatusIs(StatusCode::kUnavailable));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              AllOf(SizeIs(kMaxRetries), Each(SpanNamed("Async Backoff"))));
}

TEST(AsyncRetryLoopTest, CallSpanActiveDuringCancel) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  OTelScope scope(span);

  promise<StatusOr<int>> p([span] { EXPECT_THAT(span, IsActive()); });

  AutomaticallyCreatedBackgroundThreads background;
  future<StatusOr<int>> actual = AsyncRetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      background.cq(),
      [&](auto, auto, auto const&, auto) { return p.get_future(); },
      MakeImmutableOptions(EnableTracing(Options{})), 42, "error message");

  auto overlay = opentelemetry::trace::Scope(MakeSpan("overlay"));
  actual.cancel();
  p.set_value(0);
  (void)actual.get();
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
