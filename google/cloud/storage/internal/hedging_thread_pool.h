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
 * A lazy, dynamically-scaling thread pool with integrated hedge throttling.
 *
 * The pool starts with no threads and spawns workers on demand, up to
 * @p max_threads. Hedged requests are gated by `TryAcquireHedgeToken()`,
 * which enforces two limits: a maximum number of concurrently active hedges,
 * and a maximum rate of new hedges per second (a token bucket).
 */
class HedgingThreadPool {
 private:
  struct State {
    std::size_t const max_threads;
    std::size_t idle_threads = 0;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

    // Concurrency limiter.
    std::int64_t const max_concurrent_hedges;
    std::atomic<std::int64_t> active_concurrent_hedges{0};

    explicit State(std::size_t mt, std::int64_t mc)
        : max_threads(mt), max_concurrent_hedges(mc) {}
  };

 public:
  HedgingThreadPool(std::size_t max_threads, double rate_limit, double capacity,
                    std::int64_t max_concurrent)
      : state_(std::make_shared<State>(max_threads, max_concurrent)),
        rate_limit_(rate_limit),
        tokens_capacity_((std::max)(1.0, capacity)),
        tokens_((std::max)(1.0, capacity)),
        last_refill_(std::chrono::steady_clock::now()) {}

  ~HedgingThreadPool() {
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

  /**
   * Try to reserve capacity for one hedged request.
   *
   * On success the caller *must* eventually call `ReleaseHedgeSlot()`.
   */
  bool TryAcquireHedgeToken() {
    // Gate 1: the ceiling on concurrently active hedges.
    if (state_->max_concurrent_hedges > 0) {
      auto current =
          state_->active_concurrent_hedges.load(std::memory_order_relaxed);
      do {
        if (current >= state_->max_concurrent_hedges) return false;
      } while (!state_->active_concurrent_hedges.compare_exchange_weak(
          current, current + 1, std::memory_order_relaxed));
    }

    // Gate 2: the rate limit on new hedges (token bucket).
    if (rate_limit_ > 0.0) {
      std::lock_guard<std::mutex> lock(limiter_mutex_);
      Refill();
      if (tokens_ < 1.0) {
        if (state_->max_concurrent_hedges > 0) {
          state_->active_concurrent_hedges.fetch_sub(1,
                                                     std::memory_order_relaxed);
        }
        return false;
      }
      tokens_ -= 1.0;
    }

    return true;
  }

  void ReleaseHedgeSlot() {
    if (state_->max_concurrent_hedges > 0) {
      state_->active_concurrent_hedges.fetch_sub(1, std::memory_order_relaxed);
    }
  }

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

  void Refill() {
    auto now = std::chrono::steady_clock::now();
    auto const elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(now -
                                                                  last_refill_)
            .count();
    last_refill_ = now;
    tokens_ = (std::min)(tokens_capacity_, tokens_ + elapsed * rate_limit_);
  }

  std::shared_ptr<State> state_;
  std::vector<std::thread> workers_;

  // Token bucket rate limiter.
  double rate_limit_;
  double tokens_capacity_;
  double tokens_;
  std::chrono::steady_clock::time_point last_refill_;
  std::mutex limiter_mutex_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGING_THREAD_POOL_H
