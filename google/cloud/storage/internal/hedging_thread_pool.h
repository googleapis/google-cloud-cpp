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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGING_THREAD_POOL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGING_THREAD_POOL_H

#include "google/cloud/storage/version.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A lazy, dynamically-scaling thread pool for asynchronous background tasks.
 *
 * The pool starts with no threads and spawns workers on demand up to
 * @p max_threads. Idle workers wait on a condition variable until tasks arrive
 * or the pool is shut down.
 *
 * @p max_threads is clamped to at least 1. A pool that can never spawn a
 * worker would accept tasks it silently never runs, and callers that block on
 * a task's side effects would wait forever.
 */
class ThreadPool {
 private:
  struct State {
    std::size_t const max_threads;
    std::size_t idle_threads = 0;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

    explicit State(std::size_t mt) : max_threads(mt) {}
  };

 public:
  explicit ThreadPool(std::size_t max_threads)
      : state_(
            std::make_shared<State>((std::max<std::size_t>)(1, max_threads))) {}

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(state_->queue_mutex);
      state_->stop = true;
    }
    state_->cv.notify_all();
    for (auto& worker : workers_) {
      if (worker.joinable()) {
        if (worker.get_id() == std::this_thread::get_id()) {
          worker.detach();
        } else {
          worker.join();
        }
      }
    }
  }

  ThreadPool(ThreadPool const&) = delete;
  ThreadPool& operator=(ThreadPool const&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  /**
   * Schedule @p task to run on a pool thread.
   *
   * Returns false if the pool is shutting down, in which case the task is
   * *not* scheduled. Callers waiting on the task's side effects must handle
   * this case (e.g. by running the task inline), or they would block forever.
   */
  bool Enqueue(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(state_->queue_mutex);
      if (state_->stop) return false;
      state_->tasks.push(std::move(task));
      // Only spawn a new thread if there are no idle threads and the pool has
      // not reached its thread ceiling.
      if (state_->idle_threads == 0 && workers_.size() < state_->max_threads) {
        SpawnWorker();
      }
    }
    state_->cv.notify_one();
    return true;
  }

  std::size_t max_threads() const { return state_->max_threads; }

 private:
  void SpawnWorker() {
    workers_.emplace_back([state = state_]() mutable {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(state->queue_mutex);
          ++state->idle_threads;
          state->cv.wait(
              lock, [&state] { return state->stop || !state->tasks.empty(); });
          --state->idle_threads;
          if (state->stop && state->tasks.empty()) return;
          task = std::move(state->tasks.front());
          state->tasks.pop();
        }
        task();
      }
    });
  }

  std::shared_ptr<State> state_;
  std::vector<std::thread> workers_;
};

/**
 * Coordinates and bounds speculative hedged requests across a storage client.
 *
 * Hedged requests are gated by `TryAcquireHedgeToken()`, which enforces two
 * limits: a maximum number of concurrently active hedges (when
 * `max_concurrent > 0`), and a maximum rate of new hedges per second via a
 * token bucket (when `rate_limit > 0.0`). Setting `rate_limit <= 0.0` disables
 * rate limiting (unlimited hedges per second). Task execution is dispatched
 * onto a dedicated internal `ThreadPool`.
 */
class HedgingThreadPool {
 public:
  /**
   * Constructs a `HedgingThreadPool`.
   *
   * @param max_threads the worker pool thread limit. Clamped to at least 1.
   * @param rate_limit the token bucket refill rate in tokens/sec. When <= 0.0,
   *     rate limiting is disabled (unlimited hedges per second).
   * @param capacity the maximum burst capacity in tokens. Clamped to at
   *     least 1.0.
   * @param max_concurrent the ceiling on concurrently active hedges. When <= 0,
   *     concurrency limiting is disabled.
   */
  HedgingThreadPool(std::size_t max_threads, double rate_limit, double capacity,
                    std::int64_t max_concurrent)
      : rate_limit_(rate_limit),
        tokens_capacity_((std::max)(1.0, capacity)),
        tokens_((std::max)(1.0, capacity)),
        last_refill_(std::chrono::steady_clock::now()),
        max_concurrent_hedges_(max_concurrent),
        pool_(max_threads) {}

  ~HedgingThreadPool() = default;

  HedgingThreadPool(HedgingThreadPool const&) = delete;
  HedgingThreadPool& operator=(HedgingThreadPool const&) = delete;
  HedgingThreadPool(HedgingThreadPool&&) = delete;
  HedgingThreadPool& operator=(HedgingThreadPool&&) = delete;

  /**
   * Schedule @p task to run on a pool thread.
   *
   * Returns false if the pool is shutting down.
   */
  bool Enqueue(std::function<void()> task) {
    return pool_.Enqueue(std::move(task));
  }

  /**
   * Try to reserve capacity for one hedged request.
   *
   * On success the caller *must* eventually call `ReleaseHedgeSlot()`.
   */
  bool TryAcquireHedgeToken() {
    // Gate 1: the ceiling on concurrently active hedges. When
    // max_concurrent_hedges_ <= 0, concurrency limiting is disabled.
    if (max_concurrent_hedges_ > 0) {
      std::int64_t current =
          active_concurrent_hedges_.load(std::memory_order_relaxed);
      do {
        if (current >= max_concurrent_hedges_) return false;
      } while (!active_concurrent_hedges_.compare_exchange_weak(
          current, current + 1, std::memory_order_relaxed));
    }

    // Gate 2: the rate limit on new hedges (token bucket). When
    // rate_limit_ <= 0.0, rate limiting is disabled.
    if (rate_limit_ > 0.0) {
      std::lock_guard<std::mutex> lock(limiter_mutex_);
      Refill();
      if (tokens_ < 1.0) {
        ReleaseHedgeSlot();
        return false;
      }
      tokens_ -= 1.0;
    }

    return true;
  }

  void ReleaseHedgeSlot() {
    if (max_concurrent_hedges_ > 0) {
      active_concurrent_hedges_.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  std::size_t max_threads() const { return pool_.max_threads(); }

 private:
  void Refill() {
    std::chrono::steady_clock::time_point const now =
        std::chrono::steady_clock::now();
    double const elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(now -
                                                                  last_refill_)
            .count();
    last_refill_ = now;
    tokens_ = (std::min)(tokens_capacity_, tokens_ + elapsed * rate_limit_);
  }

  // Token bucket rate limiter. A rate_limit_ <= 0.0 disables rate limiting.
  double rate_limit_;
  double tokens_capacity_;
  double tokens_;
  std::chrono::steady_clock::time_point last_refill_;
  std::mutex limiter_mutex_;

  // Concurrency limiter. A max_concurrent_hedges_ <= 0 disables limit.
  std::int64_t const max_concurrent_hedges_;
  std::atomic<std::int64_t> active_concurrent_hedges_{0};

  // Declared last so the pool (and its worker threads) is destroyed and joined
  // first, before any other member variables are torn down.
  ThreadPool pool_;
};

/**
 * The automatic size for the pool running primary read attempts.
 *
 * These threads block inside a synchronous read, so the pool needs roughly one
 * thread per concurrent application read. That has little to do with the core
 * count; the floor is what serves small hosts.
 *
 * Used by `StorageConnectionImpl` when `ReadThreadPoolSizeOption` is unset (0).
 */
inline std::size_t DefaultReadThreadPoolSize() {
  static std::size_t const kCores = std::thread::hardware_concurrency();
  return (std::max<std::size_t>)(64, 4 * kCores);
}

/**
 * The automatic size for the pool running speculative hedge attempts.
 *
 * When @p max_concurrent_hedges is set it is already a ceiling on how many
 * hedges can run at once, so a larger pool could never use the extra threads.
 *
 * Used by `StorageConnectionImpl` when `HedgingThreadPoolSizeOption` is
 * unset (0).
 */
inline std::size_t DefaultHedgingThreadPoolSize(
    std::int64_t max_concurrent_hedges) {
  if (max_concurrent_hedges > 0) {
    return static_cast<std::size_t>(max_concurrent_hedges);
  }
  static std::size_t const kCores = std::thread::hardware_concurrency();
  return (std::max<std::size_t>)(16, 2 * kCores);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGING_THREAD_POOL_H
