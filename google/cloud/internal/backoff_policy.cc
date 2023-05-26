// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/backoff_policy.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::unique_ptr<BackoffPolicy> ExponentialBackoffPolicy::clone() const {
  return std::make_unique<ExponentialBackoffPolicy>(
      minimum_delay_, initial_delay_upper_bound_, maximum_delay_,
      scaling_lower_bound_, scaling_upper_bound_);
}

std::chrono::milliseconds ExponentialBackoffPolicy::OnCompletion() {
  // We do not want to copy the seed in `clone()` because then all operations
  // will have the same sequence of backoffs. Nor do we want to use a shared
  // PRNG because that would require locking and some more complicated lifecycle
  // management of the shared PRNG.
  //
  // Instead we exploit the following observation: most requests never need to
  // backoff, they succeed on the first call.
  //
  // So we delay the initialization of the PRNG until the first call that needs
  // to, that is here:
  if (!generator_) {
    generator_ = google::cloud::internal::MakeDefaultPRNG();
  }

  if (current_delay_start_ >= (maximum_delay_ / scaling_upper_bound_)) {
    current_delay_start_ =
        (std::max)(minimum_delay_, maximum_delay_ / scaling_upper_bound_);
  }
  current_delay_end_ = (std::min)(current_delay_end_, maximum_delay_);

  std::uniform_real_distribution<DoubleMicroseconds::rep> rng_distribution(
      current_delay_start_.count(), current_delay_end_.count());
  // Randomized sleep period because it is possible that after some time all
  // client have same sleep period if we use only exponential backoff policy.
  auto delay = DoubleMicroseconds(rng_distribution(*generator_));

  current_delay_start_ *= scaling_lower_bound_;
  current_delay_end_ *= scaling_upper_bound_;

  return std::chrono::duration_cast<std::chrono::milliseconds>(delay);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
