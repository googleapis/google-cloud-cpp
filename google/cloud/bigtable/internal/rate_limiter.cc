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

RateLimiter::Clock::duration RateLimiter::acquire(std::int64_t permits) {
  auto const now = clock_->Now();
  std::lock_guard<std::mutex> lk(mu_);
  if (next_ <= now) {
    // The request can go through immediately. But first, we need to update the
    // time the next permit can be given out.

    // Update the stored permits
    if (period_ == Clock::duration::zero()) {
      stored_permits_ = max_stored_permits_;
    } else {
      auto can_add = (now - next_) / period_;
      stored_permits_ =
          std::min(max_stored_permits_, stored_permits_ + can_add);
    }

    // Spend the stored permits
    if (permits < stored_permits_) {
      stored_permits_ -= permits;
      permits = 0;
    } else {
      permits -= stored_permits_;
      stored_permits_ = 0;
    }
    next_ = now + permits * period_;
    return Clock::duration::zero();
  }

  auto wait = next_ - now;
  next_ += permits * period_;
  return wait;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
