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

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::unique_ptr<BackoffPolicy> ExponentialBackoffPolicy::clone() const {
  return std::unique_ptr<BackoffPolicy>(new ExponentialBackoffPolicy(*this));
}

std::chrono::milliseconds ExponentialBackoffPolicy::OnCompletion() {
  using namespace std::chrono;
  std::uniform_int_distribution<microseconds::rep> rng_distribution(
      current_delay_range_.count() / 2, current_delay_range_.count());
  // Randomized sleep period because it is possible that after some time all
  // client have same sleep period if we use only exponential backoff policy.
  auto delay = microseconds(rng_distribution(generator_));
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
