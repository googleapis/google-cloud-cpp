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
 * This class does not sleep. It is the responsibility of the caller to sleep.
 * For example:
 *
 * @code
 * auto clock = std::make_shared<internal::SteadyClock>();
 * RateLimiter limiter(clock, rate);
 * while (MoreThingsToDo()) {
 *   auto wait = limiter.acquire(1);
 *   std::this_thread_sleep_for(wait);
 *   DoOneThing();
 * }
 * @endcode
 *
 * To allow for some smoothing, this class can bank permits. For example, if the
 * limiter is set to 2 QPS, and goes unused for 10 seconds, it can bank up to 20
 * permits.
 *
 * Throttling does not start until after the first call to `acquire()`. The
 * `RateLimiter` internally stores when the *next* permit can be given out.
 */
class RateLimiter {
 public:
  // Absolute time does not matter, so use a steady_clock which is guaranteed to
  // increase with time.
  using Clock = ::google::cloud::internal::SteadyClock;

  explicit RateLimiter(std::shared_ptr<Clock> clock, double rate,
                       int max_stored_permits = 0)
      : clock_(std::move(clock)),
        next_(clock_->Now()),
        stored_permits_(max_stored_permits),
        rate_(rate),
        max_stored_permits_(max_stored_permits) {
    if (rate <= 0) GCP_LOG(FATAL) << "RateLimiter rate must be > 0.";
  }

  /**
   * Returns how long the caller should wait before sending the query associated
   * with this call.
   */
  Clock::duration acquire(int permits);

  /**
   * Set the rate (Hz).
   *
   * Note that the current next_ has already been calculated. This new rate will
   * not apply to it. The new rate will apply to every `acquire()` after next.
   */
  void set_rate(double rate) {
    if (rate <= 0) GCP_LOG(FATAL) << "RateLimiter rate must be > 0.";
    std::lock_guard<std::mutex> lk(mu_);
    rate_ = rate;
  }
  double rate() const { return rate_; }

 private:
  std::mutex mu_;
  std::shared_ptr<Clock> clock_;
  Clock::time_point next_;
  std::int64_t stored_permits_ = 0;
  double rate_ = 10.0;  // queries per second
  std::int64_t max_stored_permits_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RATE_LIMITER_H
