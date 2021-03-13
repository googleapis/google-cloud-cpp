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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DEFAULTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DEFAULTS_H

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

/**
 * Returns an `Options` with the appropriate defaults for Spanner.
 *
 * Environment variables and the optional @p opts argument may be consulted to
 * determine the correct `Options` to set. It's up to the implementation as to
 * what overrides what. For example, it may be that a user-provided value for
 * `EndpointOption` via @p opts takes precedence, OR it may be that an
 * environment variable overrides that, and these rules may differ for each
 * setting.
 *
 * Option values that this implementation doesn't know about will be passed
 * along unmodified.
 */
internal::Options DefaultOptions(internal::Options opts = {});

/**
 * Returns an `Options` with the appropriate defaults for Spanner Admin
 * connections.
 *
 * Environment variables and the optional @p opts argument may be consulted to
 * determine the correct `Options` to set. It's up to the implementation as to
 * what overrides what. For example, it may be that a user-provided value for
 * `EndpointOption` via @p opts takes precedence, OR it may be that an
 * environment variable overrides that, and these rules may differ for each
 * setting.
 *
 * Option values that this implementation doesn't know about will be passed
 * along unmodified.
 */
internal::Options DefaultAdminOptions(internal::Options opts = {});

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DEFAULTS_H
