// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
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
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Option for `google::cloud::Options` to set a `spanner::RetryPolicy`.
 *
 * @ingroup spanner-options
 */
struct SpannerRetryPolicyOption {
  using Type = std::shared_ptr<spanner::RetryPolicy>;
};

/**
 * Option for `google::cloud::Options` to set a `spanner::BackoffPolicy`.
 *
 * @ingroup spanner-options
 */
struct SpannerBackoffPolicyOption {
  using Type = std::shared_ptr<spanner::BackoffPolicy>;
};

/**
 * Option for `google::cloud::Options` to set a `spanner::PollingPolicy`.
 *
 * @ingroup spanner-options
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
 * Option for `google::cloud::Options` to set the database role used for
 * session creation.
 *
 * @ingroup spanner-options
 */
struct SessionCreatorRoleOption {
  using Type = std::string;
};

/**
 * Option for `google::cloud::Options` to set the minimum number of sessions to
 * keep in the pool.
 *
 * Values <= 0 are treated as 0.
 * This value will effectively be reduced if it exceeds the overall limit on
 * the number of sessions (`max_sessions_per_channel` * number of channels).
 *
 * @ingroup spanner-options
 */
struct SessionPoolMinSessionsOption {
  using Type = int;
};

/**
 * Option for `google::cloud::Options` to set the maximum number of sessions to
 * create on each channel.
 *
 * Values <= 1 are treated as 1.
 *
 * @ingroup spanner-options
 */
struct SessionPoolMaxSessionsPerChannelOption {
  using Type = int;
};

/**
 * Option for `google::cloud::Options` to set the maximum number of sessions to
 * keep in the pool in an idle state.
 *
 * Values <= 0 are treated as 0.
 *
 * @ingroup spanner-options
 */
struct SessionPoolMaxIdleSessionsOption {
  using Type = int;
};

/// Action to take when the session pool is exhausted.
enum class ActionOnExhaustion { kBlock, kFail };
/**
 * Option for `google::cloud::Options` to set the action to take when
 * attempting to allocate a session when the pool is exhausted.
 *
 * @ingroup spanner-options
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
 *
 * @ingroup spanner-options
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
 *
 * @ingroup spanner-options
 */
struct SessionPoolLabelsOption {
  using Type = std::map<std::string, std::string>;
};

/**
 * List of all SessionPool options. Pass to `spanner::MakeConnection()`.
 */
using SessionPoolOptionList =
    OptionList<SessionCreatorRoleOption, SessionPoolMinSessionsOption,
               SessionPoolMaxSessionsPerChannelOption,
               SessionPoolMaxIdleSessionsOption,
               SessionPoolActionOnExhaustionOption,
               SessionPoolKeepAliveIntervalOption, SessionPoolLabelsOption>;

/**
 * Option for `google::cloud::Options` to set the optimizer version used in an
 * SQL query.
 *
 * @ingroup spanner-options
 */
struct QueryOptimizerVersionOption {
  using Type = std::string;
};

/**
 * Option for `google::cloud::Options` to set the optimizer statistics package
 * used in an SQL query.
 *
 * @ingroup spanner-options
 */
struct QueryOptimizerStatisticsPackageOption {
  using Type = std::string;
};

/**
 * Option for `google::cloud::Options` to set a `spanner::RequestPriority`.
 *
 * @ingroup spanner-options
 */
struct RequestPriorityOption {
  using Type = spanner::RequestPriority;
};

/**
 * Option for `google::cloud::Options` to set a per-request tag.
 *
 * @ingroup spanner-options
 */
struct RequestTagOption {
  using Type = std::string;
};

/**
 * Option for `google::cloud::Options` to set the name of an index on a
 * database table. This index is used instead of the table primary key when
 * interpreting the `KeySet` and sorting result rows.
 *
 * @ingroup spanner-options
 */
struct ReadIndexNameOption {
  using Type = std::string;
};

/**
 * Option for `google::cloud::Options` to set a limit on the number of rows
 * to yield from `Client::Read()`. There is no limit when the option is unset,
 * or when it is set to 0.
 *
 * @ingroup spanner-options
 */
struct ReadRowLimitOption {
  using Type = std::int64_t;
};

/**
 * Option for `google::cloud::Options` to set a limit on how much data will
 * be buffered to guarantee resumability of a streaming read or SQL query.
 * If the limit is exceeded, and the stream is subsequently interrupted before
 * a new resumption point can be established, the read/query will fail.
 *
 * @ingroup spanner-options
 */
struct StreamingResumabilityBufferSizeOption {
  using Type = std::size_t;
};

/**
 * Option for `google::cloud::Options` to set the desired partition size to
 * be generated by `Client::PartitionRead()` or `PartitionQuery()`.
 *
 * The default for this option is currently 1 GiB. This is only a hint. The
 * actual size of each partition may be smaller or larger than this request.
 *
 * @ingroup spanner-options
 */
struct PartitionSizeOption {
  using Type = std::int64_t;
};

/**
 * Option for `google::cloud::Options` to set the desired maximum number of
 * partitions to return from `Client::PartitionRead()` or `PartitionQuery()`.
 *
 * For example, this may be set to the number of workers available. The
 * default for this option is currently 10,000. The maximum value is
 * currently 200,000. This is only a hint. The actual number of partitions
 * returned may be smaller or larger than this request.
 *
 * @ingroup spanner-options
 */
struct PartitionsMaximumOption {
  using Type = std::int64_t;
};

/**
 * Option for `google::cloud::Options` to set a per-transaction tag.
 *
 * @ingroup spanner-options
 */
struct TransactionTagOption {
  using Type = std::string;
};

/**
 * Option for `google::cloud::Options` to return additional statistics
 * about the committed transaction in a `spanner::CommitResult`.
 *
 * @ingroup spanner-options
 */
struct CommitReturnStatsOption {
  using Type = bool;
};

/**
 * List of Request options for `client::ExecuteBatchDml()`.
 */
using RequestOptionList = OptionList<RequestPriorityOption, RequestTagOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OPTIONS_H
