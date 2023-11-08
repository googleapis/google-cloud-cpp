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

#include "google/cloud/bigtable/internal/rate_limiter.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RateLimiter::Clock::duration RateLimiter::acquire(std::int64_t tokens) {
  auto const now = clock_->Now();
  std::lock_guard<std::mutex> lk(mu_);
  auto const wait = (std::max)(next_ - now, Clock::duration::zero());
  // We can potentially give out tokens from the last `smoothing_interval_`.
  next_ = (std::max)(next_, now - smoothing_interval_);
  next_ += tokens * period_;
  return wait;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
