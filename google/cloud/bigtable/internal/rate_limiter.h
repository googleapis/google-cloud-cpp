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
 * The caller needs to acquire a "token" to perform the operation under rate
 * limits. This class limits the number of tokens issued per period of time,
 * effectively limiting the operation rate.
 *
 * The caller may acquire more than one token at a time, if it needs to perform
 * a burst of the operation under rate limits. More tokens become available as
 * time passes, with some maximum to limit the size of bursts.
 *
 * The allocation of resources must be a "prior reservation". That is, the
 * caller must tell the `RateLimiter` how many tokens it wants to acquire
 * *before* performing the operation. The `RateLimiter` will tell the caller
 * when to perform the operation.
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
 * Rate limiting does not start until after the first call to `acquire()`.
 * Consider a caller asking for 100 tokens at 1 token/s. We do not want to wait
 * 100s for this initial request. Instead, it goes through immediately, and the
 * next request is scheduled for 100s from now.
 *
 * @see https://en.wikipedia.org/wiki/Flow_control_(data)#Open-loop_flow_control
 */
class RateLimiter {
 public:
  // Absolute time does not matter, so use a steady_clock which is guaranteed to
  // increase with time.
  using Clock = ::google::cloud::internal::SteadyClock;

  template <typename Rep, typename Period>
  explicit RateLimiter(std::shared_ptr<Clock> clock,
                       std::chrono::duration<Rep, Period> period,
                       std::int64_t max_stored_tokens = 0)
      : clock_(std::move(clock)),
        // Note that std::chrono::abs() is not available until C++17.
        period_(std::chrono::duration_cast<Clock::duration>(
            period >= Clock::duration::zero() ? period : -period)),
        // Start with a full set of tokens.
        next_(clock_->Now() - max_stored_tokens * period_),
        max_stored_tokens_(max_stored_tokens) {}

  /**
   * Returns the time to wait before performing the operation associated with
   * this call.
   *
   * The caller can ask for multiple @p tokens, as a way to "weight" the
   * operation. For example, instead of acquiring one token per request, you
   * might choose to acquire one token per repeated field in a request.
   */
  Clock::duration acquire(std::int64_t tokens);

  /**
   * Set the period.
   *
   * Note that the current next_ has already been calculated. This new period
   * will not apply to it. The new period will apply to every `acquire()` after
   * next.
   */
  template <typename Rep, typename Period>
  void set_period(std::chrono::duration<Rep, Period> period) {
    // Note that std::chrono::abs() is not available until C++17.
    if (period < Clock::duration::zero()) period = -period;
    std::lock_guard<std::mutex> lk(mu_);
    period_ = std::chrono::duration_cast<Clock::duration>(period);
  }
  Clock::duration period() const { return period_; }

 private:
  std::mutex mu_;
  std::shared_ptr<Clock> clock_;
  Clock::duration period_;  // ABSL_GUARDED_BY(mu_)
  Clock::time_point next_;  // ABSL_GUARDED_BY(mu_)
  std::int64_t max_stored_tokens_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RATE_LIMITER_H
