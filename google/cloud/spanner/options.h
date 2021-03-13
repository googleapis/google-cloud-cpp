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
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/options.h"
#include <chrono>
#include <map>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
// TODO(#5738): Move this closer to the option that uses it once those are
// moved to the "spanner" namespace.
// What action to take if the session pool is exhausted.
enum class ActionOnExhaustion { kBlock, kFail };
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

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

/**
 * The minimum number of sessions to keep in the pool.
 * Values <= 0 are treated as 0.
 * This value will effectively be reduced if it exceeds the overall limit on
 * the number of sessions (`max_sessions_per_channel` * number of channels).
 */
struct SessionPoolMinSessionsOption {
  using Type = int;
};

/**
 * The maximum number of sessions to create on each channel.
 * Values <= 1 are treated as 1.
 */
struct SessionPoolMaxSessionsPerChannelOption {
  using Type = int;
};

/**
 * The maximum number of sessions to keep in the pool in an idle state.
 * Values <= 0 are treated as 0.
 */
struct SessionPoolMaxIdleSessionsOption {
  using Type = int;
};

/**
 * The action to take (kBlock or kFail) when attempting to allocate a session
 * when the pool is exhausted.
 */
struct SessionPoolActionOnExhaustionOption {
  using Type = spanner::ActionOnExhaustion;
};

/*
 * The interval at which we refresh sessions so they don't get collected by the
 * backend GC. The GC collects objects older than 60 minutes, so any duration
 * below that (less some slack to allow the calls to be made to refresh the
 * sessions) should suffice.
 */
struct SessionPoolKeepAliveIntervalOption {
  using Type = std::chrono::seconds;
};

/**
 * The labels used when creating sessions within the pool.
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
using SessionPoolOptionList = internal::OptionList<
    SessionPoolMinSessionsOption, SessionPoolMaxSessionsPerChannelOption,
    SessionPoolMaxIdleSessionsOption, SessionPoolActionOnExhaustionOption,
    SessionPoolKeepAliveIntervalOption, SessionPoolLabelsOption>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {}

// An option for the Clock that the session pool will use. This is an injection
// point to facility unit testing.
struct SessionPoolClockOption {
  using Type = std::shared_ptr<Session::Clock>;
};

}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H
