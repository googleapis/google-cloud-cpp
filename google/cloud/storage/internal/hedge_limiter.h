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
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Enforces connection-wide rate, concurrency, and lifetime limits on
 * speculative hedged requests. Values `<= 0` disable the corresponding limit;
 * `capacity` is clamped to at least `1.0`.
 */
class HedgeLimiter {
 public:
  HedgeLimiter(double rate_limit, double capacity, std::int64_t max_concurrent,
               std::int64_t max_total)
      : rate_limit_(rate_limit),
        tokens_capacity_((std::max)(1.0, capacity)),
        tokens_((std::max)(1.0, capacity)),
        last_refill_(std::chrono::steady_clock::now()),
        max_concurrent_hedges_(max_concurrent),
        max_total_hedges_(max_total) {}

  ~HedgeLimiter() = default;

  HedgeLimiter(HedgeLimiter const&) = delete;
  HedgeLimiter& operator=(HedgeLimiter const&) = delete;
  HedgeLimiter(HedgeLimiter&&) = delete;
  HedgeLimiter& operator=(HedgeLimiter&&) = delete;

  /// Reserves capacity for one hedge; callers *must* call `ReleaseHedgeSlot()`.
  bool TryAcquireHedgeToken() {
    if (IsTotalBudgetSpent()) return false;

    if (max_concurrent_hedges_ > 0) {
      std::int64_t current =
          active_concurrent_hedges_.load(std::memory_order_relaxed);
      do {
        if (current >= max_concurrent_hedges_) return false;
      } while (!active_concurrent_hedges_.compare_exchange_weak(
          current, current + 1, std::memory_order_relaxed));
    }

    if (rate_limit_ > 0.0) {
      std::lock_guard<std::mutex> lock(limiter_mutex_);
      Refill();
      if (tokens_ < 1.0) {
        ReleaseHedgeSlot();
        return false;
      }
      tokens_ -= 1.0;
    }

    // Checked last so `total_hedges_granted_` moves only for granted hedges.
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

  /// Returns true once the lifetime budget is permanently exhausted.
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
    auto const now = std::chrono::steady_clock::now();
    double const elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(now -
                                                                  last_refill_)
            .count();
    last_refill_ = now;
    tokens_ = (std::min)(tokens_capacity_, tokens_ + elapsed * rate_limit_);
  }

  double rate_limit_;
  double tokens_capacity_;
  double tokens_;
  std::chrono::steady_clock::time_point last_refill_;
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
