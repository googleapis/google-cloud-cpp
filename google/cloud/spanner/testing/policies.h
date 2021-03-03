// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_POLICIES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_POLICIES_H

#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/backoff_policy.h"
#include <chrono>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {
// For some tests, use 15 minutes as the maximum polling and retry periods. The
// default is longer, but we need to timeout earlier in the CI builds.
auto constexpr kMaximumWaitTimeMinutes = 15;
auto constexpr kBackoffScaling = 2.0;

inline std::unique_ptr<spanner::RetryPolicy> TestRetryPolicy() {
  return spanner::LimitedTimeRetryPolicy(
             std::chrono::minutes(kMaximumWaitTimeMinutes))
      .clone();
}

inline std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::seconds(1),
                                  std::chrono::minutes(1), kBackoffScaling)
      .clone();
}

inline std::unique_ptr<PollingPolicy> TestPollingPolicy() {
  return spanner::GenericPollingPolicy<>(
             spanner::LimitedTimeRetryPolicy(
                 std::chrono::minutes(kMaximumWaitTimeMinutes)),
             ExponentialBackoffPolicy(std::chrono::seconds(1),
                                      std::chrono::minutes(1), kBackoffScaling))
      .clone();
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_POLICIES_H
