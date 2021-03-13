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
 * *Internal-only* options for `google::cloud::internal::Options` to allow
 * passing a `spanner::RetryPolicy`. Customers will not use these, but they
 * will allow us internally to stop passing this policy separately from the
 * other `Options.`
 */
struct SpannerRetryPolicyOption {
  using Type = std::shared_ptr<spanner::RetryPolicy>;
};

/**
 * *Internal-only* options for `google::cloud::internal::Options` to allow
 * passing a `spanner::BackoffPolicy`. Customers will not use these, but they
 * will allow us internally to stop passing this policy separately from the
 * other `Options.`
 */
struct SpannerBackoffPolicyOption {
  using Type = std::shared_ptr<spanner::BackoffPolicy>;
};

/**
 * *Internal-only* option for `google::cloud::internal::Options` to allow
 * passing a `spanner::PollingPolicy`.
 */
struct SpannerPollingPolicyOption {
  using Type = std::shared_ptr<spanner::PollingPolicy>;
};

/**
 * List of internal-only options.
 */
using SpannerInternalOptionList =
    internal::OptionList<SpannerRetryPolicyOption, SpannerBackoffPolicyOption,
                         SpannerPollingPolicyOption>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H
