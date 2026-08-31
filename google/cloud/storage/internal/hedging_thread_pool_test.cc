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

#include "google/cloud/storage/internal/hedging_thread_pool.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(HedgingThreadPoolTest, EnqueueAndExecute) {
  // Declare the promises *before* the pool. `get()` returns as soon as the
  // shared state is ready, which may be before the worker has returned from
  // `set_value()`. Destroying the pool joins the workers, so declaring it last
  // means the workers are done before the promises they reference go away.
  std::promise<void> p1;
  std::promise<void> p2;
  auto f1 = p1.get_future();
  auto f2 = p2.get_future();

  HedgingThreadPool pool(2, 0.0, 0.0, 0);
  EXPECT_TRUE(pool.Enqueue([&p1] { p1.set_value(); }));
  EXPECT_TRUE(pool.Enqueue([&p2] { p2.set_value(); }));

  f1.get();
  f2.get();
}

TEST(HedgingThreadPoolTest, MaxConcurrentHedgesLimit) {
  // Only one concurrent hedge allowed.
  HedgingThreadPool pool(5, 0.0, 0.0, 1);

  EXPECT_TRUE(pool.TryAcquireHedgeToken());
  // Fails because one hedge is active.
  EXPECT_FALSE(pool.TryAcquireHedgeToken());

  pool.ReleaseHedgeSlot();

  EXPECT_TRUE(pool.TryAcquireHedgeToken());
}

TEST(HedgingThreadPoolTest, RateLimiter) {
  // A rate limit of 5.0 tokens per second (one token per 200ms), and a burst
  // capacity of 2 tokens.
  HedgingThreadPool pool(5, 5.0, 2.0, 0);

  EXPECT_TRUE(pool.TryAcquireHedgeToken());
  EXPECT_TRUE(pool.TryAcquireHedgeToken());
  // The burst capacity is exhausted.
  EXPECT_FALSE(pool.TryAcquireHedgeToken());

  // The refill is time-based, there is no way to inject a fake clock. Wait
  // longer than one token's refill period, with margin for slow machines.
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  EXPECT_TRUE(pool.TryAcquireHedgeToken());
}

TEST(HedgingThreadPoolTest, FractionalRateLimiter) {
  // A rate limit of 0.5 tokens per second (one token per 2 seconds).
  // The capacity is set to 0.5, which the pool must clamp to a floor of 1.0.
  HedgingThreadPool pool(5, 0.5, 0.5, 0);

  // Since capacity is clamped to 1.0, we must be able to acquire at least one
  // token.
  EXPECT_TRUE(pool.TryAcquireHedgeToken());
  EXPECT_FALSE(pool.TryAcquireHedgeToken());
}

TEST(HedgingThreadPoolTest, SafeDestructionOnWorkerThread) {
  // Dropping the last reference to the pool from inside a task runs the pool
  // destructor on one of its own worker threads. It must detach that thread
  // rather than join itself.
  //
  // Every synchronization object is shared and captured by value: this worker
  // is detached, so it can still be running after this function returns and
  // must not reference anything on the test's stack.
  auto started = std::make_shared<std::promise<void>>();
  auto destroyed = std::make_shared<std::promise<void>>();
  auto release = std::make_shared<std::promise<void>>();
  auto started_future = started->get_future();
  auto destroyed_future = destroyed->get_future();
  auto release_future = release->get_future().share();

  auto pool = std::make_shared<HedgingThreadPool>(1, 0.0, 0.0, 0);
  ASSERT_TRUE(pool->Enqueue(
      [pool_copy = pool, started, destroyed, release_future]() mutable {
        started->set_value();
        release_future.wait();
        // The test thread has dropped its reference by now, so this is the
        // last one: the pool destructor runs on this worker thread.
        pool_copy.reset();
        destroyed->set_value();
      }));

  started_future.get();
  pool.reset();
  release->set_value();
  // Wait for the destructor to finish on the worker thread. This replaces a
  // timing-based sleep: the test cannot return while the pool is still being
  // destroyed.
  destroyed_future.get();
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
