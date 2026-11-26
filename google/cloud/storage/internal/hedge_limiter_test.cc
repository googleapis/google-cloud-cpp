// Copyright 2026 Google LLC
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

#include "google/cloud/storage/internal/hedge_limiter.h"
#include "google/cloud/testing_util/fake_clock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::FakeSteadyClock;
using ::testing::Eq;

TEST(HedgeLimiterTest, MaxConcurrentHedgesLimit) {
  HedgeLimiter limiter(0.0, 0.0, /*max_concurrent=*/1, /*max_total=*/0);

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());

  limiter.ReleaseHedgeSlot();

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
}

TEST(HedgeLimiterTest, RateLimiter) {
  // 5 tokens/second (one token per 200ms), burst capacity of 2 tokens.
  auto clock = std::make_shared<FakeSteadyClock>();
  HedgeLimiter limiter(5.0, 2.0, 0, 0, clock);

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());

  // Half-token step margins avoid IEEE-754 boundary rounding sensitivity.
  clock->AdvanceTime(std::chrono::milliseconds(100));  // +0.5 token
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
  clock->AdvanceTime(std::chrono::milliseconds(200));  // +1.0 token => 1.5
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());

  // Refill is clamped at burst capacity (2 tokens).
  clock->AdvanceTime(std::chrono::seconds(10));
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
}

TEST(HedgeLimiterTest, FractionalRateLimiter) {
  // 0.5 tokens/second (one token per 2s); capacity 0.5 clamps to floor 1.0.
  auto clock = std::make_shared<FakeSteadyClock>();
  HedgeLimiter limiter(0.5, 0.5, 0, 0, clock);

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());

  clock->AdvanceTime(std::chrono::seconds(1));  // +0.5 token
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
  clock->AdvanceTime(std::chrono::seconds(2));  // +1.0 token => clamped to 1.0
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
}

TEST(HedgeLimiterTest, ZeroRateLimitDisablesRateLimiting) {
  HedgeLimiter limiter(0.0, 0.0, 0, 0);

  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  }
}

TEST(HedgeLimiterTest, MaxTotalHedgesLimit) {
  HedgeLimiter limiter(0.0, 0.0, 0, /*max_total=*/3);

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.IsTotalBudgetSpent());
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_TRUE(limiter.IsTotalBudgetSpent());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());

  // Releasing a hedge frees its concurrency slot, never its lifetime budget.
  limiter.ReleaseHedgeSlot();
  limiter.ReleaseHedgeSlot();
  limiter.ReleaseHedgeSlot();
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
}

TEST(HedgeLimiterTest, ConcurrencyDenialDoesNotSpendTotalBudget) {
  HedgeLimiter limiter(0.0, 0.0, /*max_concurrent=*/1, /*max_total=*/2);

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.IsTotalBudgetSpent());
  limiter.ReleaseHedgeSlot();

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  limiter.ReleaseHedgeSlot();
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
}

TEST(HedgeLimiterTest, RateLimitDenialDoesNotSpendTotalBudget) {
  auto clock = std::make_shared<FakeSteadyClock>();
  HedgeLimiter limiter(0.001, 1.0, 0, /*max_total=*/2, clock);

  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.TryAcquireHedgeToken());
  EXPECT_FALSE(limiter.IsTotalBudgetSpent());

  clock->AdvanceTime(std::chrono::seconds(1500));
  EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  EXPECT_TRUE(limiter.IsTotalBudgetSpent());
}

TEST(HedgeLimiterTest, ZeroMaxTotalDisablesLifetimeLimit) {
  HedgeLimiter limiter(0.0, 0.0, 0, /*max_total=*/0);

  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(limiter.TryAcquireHedgeToken());
  }
  EXPECT_FALSE(limiter.IsTotalBudgetSpent());
}

TEST(HedgeLimiterTest, MaxTotalHedgesUnderContention) {
  std::int64_t const max_total = 100;
  // A rate limit that never runs out, so threads racing for the last unit of
  // budget also exercise returning the rate token.
  HedgeLimiter limiter(1e9, 1e9, /*max_concurrent=*/4, max_total);

  std::atomic<std::int64_t> granted{0};
  std::vector<std::thread> threads;
  threads.reserve(8);
  for (int t = 0; t != 8; ++t) {
    threads.emplace_back([&limiter, &granted] {
      for (int i = 0; i != 1000; ++i) {
        if (!limiter.TryAcquireHedgeToken()) continue;
        ++granted;
        limiter.ReleaseHedgeSlot();
      }
    });
  }
  for (std::thread& t : threads) t.join();

  while (limiter.TryAcquireHedgeToken()) {
    ++granted;
    limiter.ReleaseHedgeSlot();
  }
  EXPECT_THAT(granted.load(), Eq(max_total));
  EXPECT_TRUE(limiter.IsTotalBudgetSpent());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
