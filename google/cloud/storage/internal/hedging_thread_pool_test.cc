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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::Eq;
using ::testing::Ge;

TEST(ThreadPoolTest, EnqueueAndExecute) {
  std::promise<void> p1;
  std::promise<void> p2;
  std::future<void> f1 = p1.get_future();
  std::future<void> f2 = p2.get_future();

  ThreadPool pool(2);
  EXPECT_TRUE(pool.Enqueue([&p1] { p1.set_value(); }));
  EXPECT_TRUE(pool.Enqueue([&p2] { p2.set_value(); }));

  f1.get();
  f2.get();
}

TEST(ThreadPoolTest, ConcurrentTasks) {
  std::size_t const task_count = 50;
  std::atomic<int> completed{0};
  std::vector<std::promise<void>> promises(task_count);
  std::vector<std::future<void>> futures;
  futures.reserve(task_count);
  // Declare the pool after the promises so workers are joined before the
  // promises and atomic counter they reference go out of scope.
  ThreadPool pool(8);
  for (std::size_t i = 0; i != task_count; ++i) {
    futures.push_back(promises[i].get_future());
    EXPECT_TRUE(pool.Enqueue([&promises, &completed, i] {
      ++completed;
      promises[i].set_value();
    }));
  }

  for (auto& f : futures) {
    f.get();
  }
  EXPECT_THAT(completed.load(), Eq(static_cast<int>(task_count)));
}

TEST(ThreadPoolTest, ZeroThreadsStillRunsTasks) {
  // A pool that could never spawn a worker would queue the task, report
  // success, and leave anyone waiting on the task's side effects blocked
  // forever. The size is clamped to 1 instead.
  std::promise<void> p;
  std::future<void> f = p.get_future();
  ThreadPool pool(0);
  EXPECT_THAT(pool.max_threads(), Eq(std::size_t{1}));
  EXPECT_TRUE(pool.Enqueue([&p] { p.set_value(); }));
  f.get();
}

TEST(ThreadPoolTest, DefaultSizes) {
  // `StorageConnectionImpl` sizes its pools with these when the options are
  // unset (0), so the floors are the contract.
  EXPECT_THAT(DefaultReadThreadPoolSize(), Ge(std::size_t{64}));
  EXPECT_THAT(DefaultHedgingThreadPoolSize(0), Ge(std::size_t{16}));
  // An explicit hedge ceiling already bounds concurrency, so the pool matches
  // it rather than the (larger) automatic size.
  EXPECT_THAT(DefaultHedgingThreadPoolSize(3), Eq(std::size_t{3}));
  EXPECT_THAT(DefaultHedgingThreadPoolSize(1), Eq(std::size_t{1}));
}

template <typename Pool>
void TestSafeDestructionOnWorkerThread(std::shared_ptr<Pool> pool) {
  auto started = std::make_shared<std::promise<void>>();
  auto destroyed = std::make_shared<std::promise<void>>();
  auto release = std::make_shared<std::promise<void>>();
  std::future<void> started_future = started->get_future();
  std::future<void> destroyed_future = destroyed->get_future();
  std::shared_future<void> release_future = release->get_future().share();

  ASSERT_TRUE(pool->Enqueue(
      [pool_copy = pool, started, destroyed, release_future]() mutable {
        started->set_value();
        release_future.wait();
        pool_copy.reset();
        destroyed->set_value();
      }));

  started_future.get();
  pool.reset();
  release->set_value();
  destroyed_future.get();
}

TEST(ThreadPoolTest, SafeDestructionOnWorkerThread) {
  TestSafeDestructionOnWorkerThread(std::make_shared<ThreadPool>(1));
}

TEST(HedgingThreadPoolTest, EnqueueAndExecute) {
  // Declare the promises *before* the pool. `get()` returns as soon as the
  // shared state is ready, which may be before the worker has returned from
  // `set_value()`. Destroying the pool joins the workers, so declaring it last
  // means the workers are done before the promises they reference go away.
  std::promise<void> p1;
  std::promise<void> p2;
  std::future<void> f1 = p1.get_future();
  std::future<void> f2 = p2.get_future();

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

TEST(HedgingThreadPoolTest, ZeroRateLimitDisablesRateLimiting) {
  // A rate limit of 0.0 disables rate limiting, allowing unlimited
  // acquisitions.
  HedgingThreadPool pool(5, 0.0, 0.0, 0);

  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(pool.TryAcquireHedgeToken());
  }
}

TEST(HedgingThreadPoolTest, SafeDestructionOnWorkerThread) {
  TestSafeDestructionOnWorkerThread(
      std::make_shared<HedgingThreadPool>(1, 0.0, 0.0, 0));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
