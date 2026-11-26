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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGE_LIMITER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGE_LIMITER_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/clock.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Enforces connection-wide rate, concurrency, and lifetime limits on
 * speculative hedged requests.
 *
 * Values `<= 0` disable the corresponding limit; `capacity` is clamped to at
 * least `1.0` so fractional rate limits (`0.0 < rate_limit < 1.0`) can still
 * issue hedges once refilled.
 */
class HedgeLimiter {
 public:
  using Clock = google::cloud::internal::SteadyClock;

  HedgeLimiter(double rate_limit, double capacity, std::int64_t max_concurrent,
               std::int64_t max_total)
      : HedgeLimiter(rate_limit, capacity, max_concurrent, max_total,
                     std::make_shared<Clock>()) {}

  /// @p clock must not be null. Tests inject `testing_util::FakeSteadyClock`.
  HedgeLimiter(double rate_limit, double capacity, std::int64_t max_concurrent,
               std::int64_t max_total, std::shared_ptr<Clock const> clock)
      : clock_(std::move(clock)),
        rate_limit_(rate_limit),
        tokens_capacity_((std::max)(1.0, capacity)),
        tokens_((std::max)(1.0, capacity)),
        last_refill_(clock_->Now()),
        max_concurrent_hedges_(max_concurrent),
        max_total_hedges_(max_total) {}

  ~HedgeLimiter() = default;

  HedgeLimiter(HedgeLimiter const&) = delete;
  HedgeLimiter& operator=(HedgeLimiter const&) = delete;
  HedgeLimiter(HedgeLimiter&&) = delete;
  HedgeLimiter& operator=(HedgeLimiter&&) = delete;

  /**
   * Reserves capacity for one hedged request.
   *
   * On success the caller *must* eventually call `ReleaseHedgeSlot()`.
   */
  bool TryAcquireHedgeToken() {
    // Once the lifetime budget is spent, deny with a single atomic load.
    if (IsTotalBudgetSpent()) return false;

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

    // Gate 3: the lifetime budget. Checked last so the counter only moves for
    // a hedge that is granted, which keeps `IsTotalBudgetSpent()` exact. If
    // another thread took the last unit since the check above, return what the
    // earlier gates reserved. When max_total_hedges_ <= 0, the lifetime limit
    // is disabled.
    if (max_total_hedges_ > 0) {
      std::int64_t granted =
          total_hedges_granted_.load(std::memory_order_relaxed);
      do {
        if (granted >= max_total_hedges_) {
          ReleaseHedgeSlot();
          ReturnRateToken();
          return false;
        }
      } while (!total_hedges_granted_.compare_exchange_weak(
          granted, granted + 1, std::memory_order_relaxed));
    }

    return true;
  }

  /// Releases the concurrency slot. The lifetime budget is never returned.
  void ReleaseHedgeSlot() {
    if (max_concurrent_hedges_ > 0) {
      active_concurrent_hedges_.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  /// Returns true once the lifetime budget is spent. No hedge is granted after
  /// that, so callers can skip racing altogether.
  bool IsTotalBudgetSpent() const {
    return max_total_hedges_ > 0 &&
           total_hedges_granted_.load(std::memory_order_relaxed) >=
               max_total_hedges_;
  }

 private:
  void ReturnRateToken() {
    if (rate_limit_ <= 0.0) return;
    std::lock_guard<std::mutex> lock(limiter_mutex_);
    tokens_ = (std::min)(tokens_capacity_, tokens_ + 1.0);
  }

  void Refill() {
    Clock::time_point const now = clock_->Now();
    double const elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(now -
                                                                  last_refill_)
            .count();
    last_refill_ = now;
    tokens_ = (std::min)(tokens_capacity_, tokens_ + elapsed * rate_limit_);
  }

  std::shared_ptr<Clock const> const clock_;
  double const rate_limit_;
  double const tokens_capacity_;
  double tokens_;
  Clock::time_point last_refill_;
  std::mutex limiter_mutex_;

  std::int64_t const max_concurrent_hedges_;
  std::atomic<std::int64_t> active_concurrent_hedges_{0};

  std::int64_t const max_total_hedges_;
  std::atomic<std::int64_t> total_hedges_granted_{0};
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_HEDGE_LIMITER_H
