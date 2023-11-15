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
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "absl/types/variant.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Clock = ThrottlingMutateRowsLimiter::Clock;
using ::google::cloud::testing_util::FakeSteadyClock;
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

auto noop = [](auto) {};

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

  ThrottlingMutateRowsLimiter limiter(clock, mock.AsStdFunction(),
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
      clock, noop, std::chrono::milliseconds(100), kMinPeriod, kMaxPeriod,
      kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));

  limiter.Update(google::bigtable::v2::MutateRowsResponse{});
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(100));
}

TEST(MutateRowsLimiter, NoRateIncreaseIfNoThrottlingSinceLastUpdate) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, std::chrono::milliseconds(100), kMinPeriod, kMaxPeriod,
      kMinFactor, kMaxFactor);
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
  ThrottlingMutateRowsLimiter limiter(clock, noop,
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
  ThrottlingMutateRowsLimiter limiter(clock, noop, std::chrono::milliseconds(7),
                                      kMinPeriod, kMaxPeriod, /*min_factor=*/.7,
                                      kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(7));

  limiter.Update(MakeResponse(.5, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(10));
}

TEST(MutateRowsLimiter, UpdateClampsMaxFactor) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, std::chrono::milliseconds(13), kMinPeriod, kMaxPeriod,
      kMinFactor, /*max_factor=*/1.3);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(13));

  InduceThrottling(limiter);
  limiter.Update(MakeResponse(2.0, std::chrono::milliseconds(0)));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(10));
}

TEST(MutateRowsLimiter, UpdateClampsResultToMinPeriod) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(
      clock, noop, std::chrono::microseconds(11),
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
      clock, noop, std::chrono::seconds(9), kMinPeriod,
      /*max_period=*/std::chrono::seconds(10), kMinFactor, kMaxFactor);
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Seconds(9));

  limiter.Update(MakeResponse(0.5, std::chrono::milliseconds(0)));
  EXPECT_LE(absl::FromChrono(limiter.period()), absl::Seconds(10));
}

TEST(MutateRowsLimiter, UpdateRespectsResponsePeriod) {
  auto clock = std::make_shared<FakeSteadyClock>();
  ThrottlingMutateRowsLimiter limiter(clock, noop,
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

TEST(MutateRowsLimiter, MakeMutateRowsLimiter) {
  using ::google::cloud::bigtable::experimental::BulkApplyThrottlingOption;
  auto noop = MakeMutateRowsLimiter(Options{});
  EXPECT_THAT(dynamic_cast<NoopMutateRowsLimiter*>(noop.get()), NotNull());

  auto throttling = MakeMutateRowsLimiter(
      Options{}.set<BulkApplyThrottlingOption>(absl::monostate{}));
  EXPECT_THAT(dynamic_cast<ThrottlingMutateRowsLimiter*>(throttling.get()),
              NotNull());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
