// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/options.h"
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

/**
 * Option for `google::cloud::internal::Options` to set a
 * `spanner::RetryPolicy`.
 */
struct RetryPolicyOption {
  using Type = std::shared_ptr<spanner::RetryPolicy>;
};

/**
 * Option for `google::cloud::internal::Options` to set a
 * `spanner::BackoffPolicy`.
 */
struct BackoffPolicyOption {
  using Type = std::shared_ptr<spanner::BackoffPolicy>;
};

/**
 * Option for `google::cloud::internal::Options` to set a
 * `spanner::PollingPolicy`.
 */
struct PollingPolicyOption {
  using Type = std::shared_ptr<spanner::PollingPolicy>;
};

/**
 * List of all "policy" options.
 */
using PolicyOptionList =
    internal::OptionList<RetryPolicyOption, BackoffPolicyOption,
                         PollingPolicyOption>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H
