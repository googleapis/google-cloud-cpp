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

/**
 * @file
 *
 * This file defines options to be used with instances of
 * `google::cloud::Options`. By convention options are named with an "Option"
 * suffix. As the name would imply, all options are optional, and leaving them
 * unset will result in a reasonable default being chosen.
 *
 * Not all options are meaningful to all functions that accept a
 * `google::cloud::Options` instance. Each function that accepts a
 * `google::cloud::Options` should document which options it expects. This is
 * typically done by indicating lists of options using "OptionList" aliases.
 * For example, a function may indicate that users may set any option in
 * `SpannerPolicyOptionList`.
 *
 * @note Unrecognized options are allowed and will be ignored. To debug issues
 *     with options set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment
 *     and unexpected options will be logged.
 *
 * @see `google::cloud::CommonOptionList`
 * @see `google::cloud::GrpcOptionList`
 */

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/request_priority.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/options.h"
#include <chrono>
#include <map>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Option for `google::cloud::Options` to set a `spanner::RetryPolicy`.
 */
struct SpannerRetryPolicyOption {
  using Type = std::shared_ptr<spanner::RetryPolicy>;
};

/**
 * Option for `google::cloud::Options` to set a `spanner::BackoffPolicy`.
 */
struct SpannerBackoffPolicyOption {
  using Type = std::shared_ptr<spanner::BackoffPolicy>;
};

/**
 * Option for `google::cloud::Options` to set a `spanner::PollingPolicy`.
 */
struct SpannerPollingPolicyOption {
  using Type = std::shared_ptr<spanner::PollingPolicy>;
};

/**
 * List of all "policy" options.
 */
using SpannerPolicyOptionList =
    OptionList<spanner::SpannerRetryPolicyOption, SpannerBackoffPolicyOption,
               SpannerPollingPolicyOption>;

/**
 * Option for `google::cloud::Options` to set the minimum number of sessions to
 * keep in the pool.
 *
 * Values <= 0 are treated as 0.
 * This value will effectively be reduced if it exceeds the overall limit on
 * the number of sessions (`max_sessions_per_channel` * number of channels).
 */
struct SessionPoolMinSessionsOption {
  using Type = int;
};

/**
 * Option for `google::cloud::Options` to set the maximum number of sessions to
 * create on each channel.
 *
 * Values <= 1 are treated as 1.
 */
struct SessionPoolMaxSessionsPerChannelOption {
  using Type = int;
};

/**
 * Option for `google::cloud::Options` to set the maximum number of sessions to
 * keep in the pool in an idle state.
 *
 * Values <= 0 are treated as 0.
 */
struct SessionPoolMaxIdleSessionsOption {
  using Type = int;
};

/// Action to take when the session pool is exhausted.
enum class ActionOnExhaustion { kBlock, kFail };
/**
 * Option for `google::cloud::Options` to set the action to take when
 * attempting to allocate a session when the pool is exhausted.
 */
struct SessionPoolActionOnExhaustionOption {
  using Type = spanner::ActionOnExhaustion;
};

/**
 * Option for `google::cloud::Options` to set the interval at which we refresh
 * sessions so they don't get collected by the backend GC.
 *
 * The GC collects objects older than 60 minutes, so any duration
 * below that (less some slack to allow the calls to be made to refresh the
 * sessions) should suffice.
 */
struct SessionPoolKeepAliveIntervalOption {
  using Type = std::chrono::seconds;
};

/**
 * Option for `google::cloud::Options` to set the labels used when creating
 * sessions within the pool.
 *
 *  * Label keys must match `[a-z]([-a-z0-9]{0,61}[a-z0-9])?`.
 *  * Label values must match `([a-z]([-a-z0-9]{0,61}[a-z0-9])?)?`.
 *  * The maximum number of labels is 64.
 */
struct SessionPoolLabelsOption {
  using Type = std::map<std::string, std::string>;
};

/**
 * List of all SessionPool options.
 */
using SessionPoolOptionList = OptionList<
    SessionPoolMinSessionsOption, SessionPoolMaxSessionsPerChannelOption,
    SessionPoolMaxIdleSessionsOption, SessionPoolActionOnExhaustionOption,
    SessionPoolKeepAliveIntervalOption, SessionPoolLabelsOption>;

/**
 * Option for `google::cloud::Options` to set a `spanner::RequestPriority`.
 */
struct RequestPriorityOption {
  using Type = spanner::RequestPriority;
};

/**
 * List of all Request options.
 */
using RequestOptionList = OptionList<RequestPriorityOption>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H
