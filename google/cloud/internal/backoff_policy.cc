// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::unique_ptr<BackoffPolicy> ExponentialBackoffPolicy::clone() const {
  auto tmp =
      google::cloud::internal::make_unique<ExponentialBackoffPolicy>(*this);
  // Force OnCompletion() to reseed the generator.
  tmp->generator_.reset();
  // Older versions of GCC (4.9) and Clang (Apple Xcode 7.3) need this
  // explicit move-constructor.
  return std::move(tmp);
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
  if (not generator_) {
    generator_ = google::cloud::internal::MakeDefaultPRNG();
  }
  using namespace std::chrono;
  std::uniform_int_distribution<microseconds::rep> rng_distribution(
      current_delay_range_.count() / 2, current_delay_range_.count());
  // Randomized sleep period because it is possible that after some time all
  // client have same sleep period if we use only exponential backoff policy.
  auto delay = microseconds(rng_distribution(*generator_));
  current_delay_range_ = microseconds(
      static_cast<microseconds::rep>(current_delay_range_.count() * scaling_));
  if (current_delay_range_ >= maximum_delay_) {
    current_delay_range_ = maximum_delay_;
  }
  return duration_cast<milliseconds>(delay);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
