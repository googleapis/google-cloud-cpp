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

#include "google/cloud/bigtable/internal/rate_limiter.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Clock = RateLimiter::Clock;

/**
 * Returns the duration it takes to give out @p permits at the given @p rate.
 *
 * Essentially, we are returning `permits / rate`.
 *
 * This function handles rounding, while getting the maximum precision out of
 * the clock.
 */
Clock::duration TimeNeeded(std::int64_t permits, double rate) {
  return std::chrono::duration_cast<Clock::duration>(absl::ToChronoNanoseconds(
      absl::Seconds(static_cast<double>(permits) / rate)));
}

}  // namespace

RateLimiter::Clock::duration RateLimiter::acquire(std::int64_t permits) {
  auto const now = clock_->Now();
  std::lock_guard<std::mutex> lk(mu_);
  // The request can go through immediately. But first, we need to update the
  // time the next permit can be given out.
  if (next_ <= now) {
    // Update the stored permits
    absl::Duration d = absl::FromChrono(now - next_) * rate_;
    auto can_add = absl::ToInt64Seconds(d);
    stored_permits_ = std::min(max_stored_permits_, stored_permits_ + can_add);

    // Try to spend the stored permits
    if (permits < stored_permits_) {
      stored_permits_ -= permits;
      next_ = now;
      return Clock::duration::zero();
    }

    // Drain the stored permits
    permits -= stored_permits_;
    stored_permits_ = 0;
    next_ = now + TimeNeeded(permits, rate_);
    return Clock::duration::zero();
  }

  // The request cannot go through immediately. We must add to the wait time.
  if (stored_permits_ != 0) {
    GCP_LOG(FATAL)
        << "RateLimiter stored permits should be 0. Clock may not be monotonic."
        << std::endl;
  }
  auto current = next_;
  next_ += TimeNeeded(permits, rate_);
  return current - now;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
