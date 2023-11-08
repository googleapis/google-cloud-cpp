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

#include "google/cloud/bigtable/internal/rate_limiter.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::FakeSteadyClock;

TEST(RateLimiter, NoWaitForInitialAcquire) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::seconds(1));

  auto wait = limiter.acquire(100);
  EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());
}

TEST(RateLimiter, Basic) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::seconds(1));

  for (auto i = 0; i != 10; ++i) {
    auto wait = limiter.acquire(1);
    EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(i));
  }

  clock->AdvanceTime(std::chrono::seconds(10));
  for (auto i = 0; i != 10; ++i) {
    auto wait = limiter.acquire(1);
    EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());
    clock->AdvanceTime(std::chrono::seconds(1));
  }
}

TEST(RateLimiter, WaitsForEachToken) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::seconds(1));

  (void)limiter.acquire(10);

  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(10));
}

TEST(RateLimiter, StoresTokens) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::milliseconds(500),
                      std::chrono::seconds(5));

  // After 2 seconds, we should have 4 tokens banked.
  clock->AdvanceTime(std::chrono::seconds(2));
  auto wait = limiter.acquire(10);
  EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());

  // We requested 10 tokens, with 4 tokens banked. We should have to wait 3
  // seconds to give out the remaining 6 tokens at a rate of 2 tokens per
  // second.
  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(3));
}

TEST(RateLimiter, StoresTokensUpToLimit) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::seconds(1), std::chrono::seconds(10));

  // Wait for 100 seconds. We should be able to use 10 tokens from the last 10
  // seconds of this interval.
  clock->AdvanceTime(std::chrono::seconds(100));
  (void)limiter.acquire(30);

  // We should have to wait for 30 - 10 = 20 tokens.
  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(20));
}

TEST(RateLimiter, PeriodLessThanOneSecond) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::milliseconds(100));

  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());

  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Milliseconds(100));
}

TEST(RateLimiter, PeriodGreaterThanOneSecond) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::seconds(10));

  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());

  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(10));
}

TEST(RateLimiter, SetPeriodEventuallyTakesAffect) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::milliseconds(100));

  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());

  limiter.set_period(std::chrono::milliseconds(200));
  EXPECT_EQ(absl::FromChrono(limiter.period()), absl::Milliseconds(200));

  // The return of this call to `acquire()` has already been determined at the
  // 10 QPS rate.
  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Milliseconds(100));

  // Every subsequent call should add on .2 seconds.
  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Milliseconds(300));

  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Milliseconds(500));
}

TEST(RateLimiter, AbsoluteValueOfPeriod) {
  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, -std::chrono::seconds(10));

  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::ZeroDuration());

  limiter.set_period(-std::chrono::seconds(5));
  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(10));

  wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait), absl::Seconds(15));
}

TEST(RateLimiter, ThreadSafety) {
  // - Set rate to 1 QPS
  // - Spin off N threads
  // - In each thread do M acquires at time now
  //
  // We expect that N * M + 1 acquires yields a wait time of N * M seconds.

  auto constexpr kThreadCount = 8;
  auto constexpr kAcquiresPerThread = 1000;

  auto clock = std::make_shared<FakeSteadyClock>();
  RateLimiter limiter(clock, std::chrono::seconds(1));

  auto work = [&limiter](int acquires) {
    for (auto i = 0; i != acquires; ++i) (void)limiter.acquire(1);
  };
  std::vector<std::thread> v;
  v.reserve(kThreadCount);
  for (auto i = 0; i != kThreadCount; ++i) {
    v.emplace_back(work, kAcquiresPerThread);
  }
  for (auto& t : v) {
    t.join();
  }

  // Make sure we didn't drop any individual acquires
  auto wait = limiter.acquire(1);
  EXPECT_EQ(absl::FromChrono(wait),
            absl::Seconds(kThreadCount * kAcquiresPerThread));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
