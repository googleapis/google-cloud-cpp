// Copyright 2023 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RATE_LIMITER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RATE_LIMITER_H

#include "google/cloud/internal/clock.h"
#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include <chrono>
#include <cstdint>
#include <mutex>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A threadsafe interface for rate limiting.
 *
 * The caller tells the `RateLimiter` how many permits it wants to acquire. The
 * `RateLimiter` tells the caller when those permits will be available.
 *
 * The `RateLimiter` does not sleep. It is the responsibility of the caller to
 * sleep. For example:
 *
 * @code
 * auto clock = std::make_shared<internal::SteadyClock>();
 * auto initial_period = std::chrono::milliseconds(100);
 * RateLimiter limiter(clock, initial_period);
 * while (MoreThingsToDo()) {
 *   auto wait = limiter.acquire(1);
 *   std::this_thread_sleep_for(wait);
 *   DoOneThing();
 * }
 * @endcode
 *
 * To allow for some smoothing, this class can bank permits. For example, if the
 * limiter is set to 2 permits/s, and goes unused for 10 seconds, it can bank up
 * to 20 permits. The maximum stored permits is configurable.
 *
 * Throttling does not start until after the first call to `acquire()`. Consider
 * a caller asking for 100 permits at 1 permit/s. We do not want to wait 100s
 * for this initial request. Instead, it goes through immediately, and the next
 * request is scheduled for 100s from now.
 */
class RateLimiter {
 public:
  // Absolute time does not matter, so use a steady_clock which is guaranteed to
  // increase with time.
  using Clock = ::google::cloud::internal::SteadyClock;

  template <typename Rep, typename Period>
  explicit RateLimiter(std::shared_ptr<Clock> clock,
                       std::chrono::duration<Rep, Period> period,
                       std::int64_t max_stored_permits = 0)
      : clock_(std::move(clock)),
        period_(std::chrono::duration_cast<Clock::duration>(period)),
        next_(clock_->Now()),
        stored_permits_(max_stored_permits),
        max_stored_permits_(max_stored_permits) {
    if (period_ < Clock::duration::zero()) {
      GCP_LOG(FATAL) << "RateLimiter period must be > 0.";
    }
  }

  /**
   * Returns the time to wait before performing the operation associated with
   * this call.
   *
   * The caller can ask for multiple @p permits, as a way to "weight" the
   * operation. For example, instead of acquiring one permit per request, you
   * might choose to acquire one permit per repeated field in a request.
   */
  Clock::duration acquire(std::int64_t permits);

  /**
   * Set the period.
   *
   * Note that the current next_ has already been calculated. This new rate will
   * not apply to it. The new rate will apply to every `acquire()` after next.
   */
  void set_period(Clock::duration period) {
    if (period < Clock::duration::zero()) {
      GCP_LOG(FATAL) << "RateLimiter period must be >= 0.";
    }
    std::lock_guard<std::mutex> lk(mu_);
    period_ = period;
  }
  Clock::duration period() const { return period_; }

 private:
  std::mutex mu_;
  std::shared_ptr<Clock> clock_;
  Clock::duration period_;           // ABSL_GUARDED_BY(mu_)
  Clock::time_point next_;           // ABSL_GUARDED_BY(mu_)
  std::int64_t stored_permits_ = 0;  // ABSL_GUARDED_BY(mu_)
  std::int64_t max_stored_permits_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RATE_LIMITER_H
