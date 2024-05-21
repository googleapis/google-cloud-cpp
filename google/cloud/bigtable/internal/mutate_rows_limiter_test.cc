// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Clock = ThrottlingMutateRowsLimiter::Clock;
using ::google::cloud::bigtable::experimental::BulkApplyThrottlingOption;
using ::google::cloud::testing_util::FakeSteadyClock;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::NotNull;

auto constexpr kMinFactor = .7;
auto constexpr kMaxFactor = 1.3;
auto constexpr kMinPeriod = std::chrono::microseconds(10);
auto constexpr kMaxPeriod = std::chrono::seconds(10);

std::function<void(Clock::duration)> ExpectWait(absl::Duration expected_wait) {
  return [expected_wait](auto actual_wait) {
    EXPECT_EQ(absl::FromChrono(actual_wait), expected_wait);
  };
}

std::function<future<void>(Clock::duration)> ExpectWaitAsync(
    absl::Duration expected_wait) {
  return [expected_wait](auto actual_wait) {
    EXPECT_EQ(absl::FromChrono(actual_wait), expected_wait);
    return make_ready_future();
  };
}

auto noop = [](auto) {};
auto async_noop = [](auto) { return make_ready_future(); };

void InduceThrottling(MutateRowsLimiter& limiter) {
  limiter.Acquire();
  limiter.Acquire();
}

template <typename Rep, typename Period>
google::bigtable::v2::MutateRowsResponse MakeResponse(
    double factor, std::chrono::duration<Rep, Period> period) {
  google::bigtable::v2::MutateRowsResponse response;
  auto& info = *response.mutable_rate_limit_info();
  info.set_factor(factor);
  *info.mutable_period() = internal::ToDurationProto(period);
  return response;
}

TEST(MutateRowsLimiter, BasicRateLimiting) {
  auto clock = std::make_shared<FakeSteadyClock>();
  MockFunction<void(FakeSteadyClock::duration)> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(ExpectWait(absl::ZeroDuration()))
      .WillOnce(ExpectWait(absl::Seconds(1)))
      .WillOnce(ExpectWait(absl::Seconds(2)))
      .WillOnce(ExpectWait(absl::ZeroDuration()));

  ThrottlingMutateRowsLimiter limiter(clock, mock.AsStdFunction(), async_noop,
                                      std::chrono::seconds(1), kMinPeriod,
                                      kMaxPeriod, kMinFactor, kMaxFactor);
  limiter.Acquire();
  limiter.Acquire();
  limiter.Acquire();

  clock->AdvanceTime(std::chrono::seconds(3));
  limiter.Acquire();
}

TEST(MutateRowsLimiter, ResponseWithoutRateLimitInfo) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, async_noop, std::chrono::milliseconds(100), kMinPeriod,
      kMaxPeriod, kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));

  limiter.Update(google::bigtable::v2::MutateRowsResponse{});
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));
}

TEST(MutateRowsLimiter, NoRateIncreaseIfNoThrottlingSinceLastUpdate) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, async_noop, std::chrono::milliseconds(100), kMinPeriod,
      kMaxPeriod, kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));

  limiter.Update(MakeResponse(1.25, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));

  InduceThrottling(limiter);
  limiter.Update(MakeResponse(1.25, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(80));

  limiter.Update(MakeResponse(1.25, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(80));
}

TEST(MutateRowsLimiter, RateCanDecreaseIfNoThrottlingSinceLastUpdate) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(clock, noop, async_noop,
                                      std::chrono::milliseconds(64), kMinPeriod,
                                      kMaxPeriod, kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(64));

  limiter.Update(MakeResponse(.8, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(80));

  InduceThrottling(limiter);
  limiter.Update(MakeResponse(.8, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));

  limiter.Update(MakeResponse(.8, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(125));
}

TEST(MutateRowsLimiter, UpdateClampsMinFactor) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, async_noop, std::chrono::milliseconds(7), kMinPeriod,
      kMaxPeriod, /*min_factor=*/.7, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(7));

  limiter.Update(MakeResponse(.5, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(10));
}

TEST(MutateRowsLimiter, UpdateClampsMaxFactor) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, async_noop, std::chrono::milliseconds(13), kMinPeriod,
      kMaxPeriod, kMinFactor, /*max_factor=*/1.3);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(13));

  InduceThrottling(limiter);
  limiter.Update(MakeResponse(2.0, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(10));
}

TEST(MutateRowsLimiter, UpdateClampsResultToMinPeriod) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, async_noop, std::chrono::microseconds(11),
      /*min_period=*/std::chrono::microseconds(10), kMaxPeriod, kMinFactor,
      kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Microseconds(11));

  InduceThrottling(limiter);
  limiter.Update(MakeResponse(2.0, std::chrono::milliseconds(0)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Microseconds(10));
}

TEST(MutateRowsLimiter, UpdateClampsResultToMaxPeriod) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, async_noop, std::chrono::seconds(9), kMinPeriod,
      /*max_period=*/std::chrono::seconds(10), kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Seconds(9));

  limiter.Update(MakeResponse(0.5, std::chrono::milliseconds(0)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Seconds(10));
}

TEST(MutateRowsLimiter, UpdateRespectsResponsePeriod) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(clock, noop, async_noop,
                                      std::chrono::milliseconds(64), kMinPeriod,
                                      kMaxPeriod, kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(64));

  limiter.Update(MakeResponse(0.8, std::chrono::seconds(2)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Milliseconds(80));

  // We were told to wait 2 seconds until updating again. These calls to
  // `Update` should be no-ops.
  limiter.Update(MakeResponse(0.8, std::chrono::seconds(2)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Milliseconds(80));
  clock->AdvanceTime(std::chrono::seconds(1));
  limiter.Update(MakeResponse(0.8, std::chrono::seconds(2)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Milliseconds(80));
  clock->AdvanceTime(std::chrono::seconds(1));

  // The previous update period has expired. The next call to `Update` should
  // modify the underlying rate limiter.
  limiter.Update(MakeResponse(0.8, std::chrono::seconds(2)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Milliseconds(100));
}

TEST(MutateRowsLimiter, AsyncBasicRateLimiting) {
  auto clock = std::make_shared<FakeSteadyClock>();
  MockFunction<future<void>(FakeSteadyClock::duration)> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(ExpectWaitAsync(absl::ZeroDuration()))
      .WillOnce(ExpectWaitAsync(absl::Seconds(1)))
      .WillOnce(ExpectWaitAsync(absl::Seconds(2)))
      .WillOnce(ExpectWaitAsync(absl::ZeroDuration()));

  ThrottlingMutateRowsLimiter limiter(clock, noop, mock.AsStdFunction(),
                                      std::chrono::seconds(1), kMinPeriod,
                                      kMaxPeriod, kMinFactor, kMaxFactor);
  limiter.AsyncAcquire().get();
  limiter.AsyncAcquire().get();
  limiter.AsyncAcquire().get();

  clock->AdvanceTime(std::chrono::seconds(3));
  limiter.AsyncAcquire();
}

TEST(MutateRowsLimiter, MakeMutateRowsLimiter) {
  auto noop = MakeMutateRowsLimiter(
      CompletionQueue{}, Options{}.set<BulkApplyThrottlingOption>(false));
  EXPECT_THAT(dynamic_cast<NoopMutateRowsLimiter*>(noop.get()), NotNull());

  auto throttling = MakeMutateRowsLimiter(
      CompletionQueue{}, Options{}.set<BulkApplyThrottlingOption>(true));
  EXPECT_THAT(dynamic_cast<ThrottlingMutateRowsLimiter*>(throttling.get()),
              NotNull());
}

TEST(MutateRowsLimiter, LoggingEnabled) {
  testing_util::ScopedLog log;
  std::vector<std::string> lines;

  auto limiter = MakeMutateRowsLimiter(
      CompletionQueue{}, Options{}
                             .set<BulkApplyThrottlingOption>(true)
                             .set<TracingComponentsOption>({"rpc"}));
  // With the default settings, we should expect throttling by the second
  // request. Still, go up to 100 in case instructions are slow.
  for (auto i = 0; i != 100 && lines.empty(); ++i) {
    limiter->Acquire();
    lines = log.ExtractLines();
  }

  EXPECT_THAT(lines, Contains(HasSubstr("Throttling BulkApply for ")));
}

TEST(MakeMutateRowsLimiter, LoggingDisabled) {
  testing_util::ScopedLog log;

  auto limiter = MakeMutateRowsLimiter(CompletionQueue{},
                                       Options{}
                                           .set<BulkApplyThrottlingOption>(true)
                                           .set<TracingComponentsOption>({}));
  // We generally expect throttling by the second response. Still, go up to 5 to
  // be a little bit more conclusive.
  for (auto i = 0; i != 5; ++i) {
    limiter->Acquire();
    EXPECT_THAT(log.ExtractLines(), IsEmpty());
  }
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::SpanNamed;

TEST(MakeMutateRowsLimiter, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> spans;

  auto limiter = MakeMutateRowsLimiter(
      CompletionQueue{},
      EnableTracing(Options{}.set<BulkApplyThrottlingOption>(true)));
  // With the default settings, we should expect throttling by the second
  // request. Still, go up to 100 in case instructions are slow.
  for (auto i = 0; i != 100 && spans.empty(); ++i) {
    limiter->Acquire();
    spans = span_catcher->GetSpans();
  }
  EXPECT_THAT(spans,
              Contains(SpanNamed("gl-cpp.bigtable.bulk_apply_throttling")));
}

TEST(MakeMutateRowsLimiter, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto limiter = MakeMutateRowsLimiter(
      CompletionQueue{},
      DisableTracing(Options{}.set<BulkApplyThrottlingOption>(true)));
  // We generally expect throttling by the second response. Still, go up to 5 to
  // be a little bit more conclusive.
  for (auto i = 0; i != 5; ++i) {
    limiter->Acquire();
    EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
  }
}

TEST(MakeMutateRowsLimiter, TracingEnabledAsync) {
  auto span_catcher = testing_util::InstallSpanCatcher();
  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> spans;

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillRepeatedly([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);
  auto limiter = MakeMutateRowsLimiter(
      cq, EnableTracing(Options{}.set<BulkApplyThrottlingOption>(true)));
  // With the default settings, we should expect throttling by the second
  // request. Still, go up to 100 in case instructions are slow.
  for (auto i = 0; i != 100 && spans.empty(); ++i) {
    limiter->AsyncAcquire().get();
    spans = span_catcher->GetSpans();
  }
  EXPECT_THAT(spans,
              Contains(SpanNamed("gl-cpp.bigtable.bulk_apply_throttling")));
}

TEST(MakeMutateRowsLimiter, TracingDisabledAsync) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillRepeatedly([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);
  auto limiter = MakeMutateRowsLimiter(
      cq, DisableTracing(Options{}.set<BulkApplyThrottlingOption>(true)));
  // We generally expect throttling by the second response. Still, go up to 5 to
  // be a little bit more conclusive.
  for (auto i = 0; i != 5; ++i) {
    limiter->AsyncAcquire().get();
    EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
  }
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
