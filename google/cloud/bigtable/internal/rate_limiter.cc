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

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RateLimiter::Clock::duration RateLimiter::acquire(std::int64_t tokens) {
  auto const now = clock_->Now();
  std::unique_lock<std::mutex> lk(mu_);
  auto const wait = (std::max)(next_ - now, Clock::duration::zero());
  // We can only keep up to M stored tokens.
  next_ = (std::max)(next_, now - max_stored_tokens_ * period_);
  next_ += tokens * period_;
  lk.unlock();
  return wait;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
