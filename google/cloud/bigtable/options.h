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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_OPTIONS_H

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
 * `ClientOptionList`.
 *
 * @note Unrecognized options are allowed and will be ignored. To debug issues
 *     with options set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment
 *     and unexpected options will be logged.
 *
 * @see `google::cloud::CommonOptionList`
 * @see `google::cloud::GrpcOptionList`
 */

#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/retry_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/options.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The application profile id.
 *
 * An application profile, or app profile, stores settings that tell your Cloud
 * Bigtable instance how to handle incoming requests from an application. When
 * an applications connects to a Bigtable instance, it can specify an app
 * profile, and Bigtable uses that app profile for requests that the application
 * sends over that connection.
 *
 * This option is always used in conjunction with a `bigtable::Table`. The app
 * profile belongs to the table's instance, with an id given by the value of
 * this option.
 *
 * @see https://cloud.google.com/bigtable/docs/app-profiles for an overview of
 *     app profiles.
 *
 * @see https://cloud.google.com/bigtable/docs/replication-overview#app-profiles
 *     for how app profiles are used to achieve replication.
 *
 * @ingroup google-cloud-bigtable-options
 */
struct AppProfileIdOption {
  using Type = std::string;
};

/**
 * Read rows in reverse order.
 *
 * The rows will be streamed in reverse lexicographic order of the keys. This is
 * particularly useful to get the last N records before a key.
 *
 * This option does not affect the contents of the rows, just the order that
 * the rows are returned.
 *
 * @note When using this option, the order of row keys in a `bigtable::RowRange`
 * does not change. The row keys still must be supplied in lexicographic order.
 *
 * @snippet read_snippets.cc reverse scan
 *
 * @see https://cloud.google.com/bigtable/docs/reads#reverse-scan
 *
 * @ingroup google-cloud-bigtable-options
 */
struct ReverseScanOption {
  using Type = bool;
};

/**
 * The endpoint for data operations.
 *
 * @deprecated Please use `google::cloud::EndpointOption` instead.
 */
struct DataEndpointOption {
  using Type = std::string;
};

/**
 * The endpoint for table admin operations.
 *
 * @deprecated Please use `google::cloud::EndpointOption` instead.
 */
struct AdminEndpointOption {
  using Type = std::string;
};

/**
 * The endpoint for instance admin operations.
 *
 * In most scenarios this should have the same value as `AdminEndpointOption`.
 * The most common exception is testing, where the emulator for instance admin
 * operations may be different than the emulator for admin and data operations.
 *
 * @deprecated Please use `google::cloud::EndpointOption` instead.
 */
struct InstanceAdminEndpointOption {
  using Type = std::string;
};

/**
 * Minimum time in ms to refresh connections.
 *
 * The server will not disconnect idle connections before this time.
 *
 * @ingroup google-cloud-bigtable-options
 */
struct MinConnectionRefreshOption {
  using Type = std::chrono::milliseconds;
};

/**
 * Maximum time in ms to refresh connections.
 *
 * The server will disconnect idle connections before this time. The
 * connections will not be automatically refreshed in the background if this
 * value is set to `0`.
 *
 * @note If this value is less than the value of `MinConnectionRefreshOption`,
 * it will be set to the value of `MinConnectionRefreshOption`.
 *
 * @ingroup google-cloud-bigtable-options
 */
struct MaxConnectionRefreshOption {
  using Type = std::chrono::milliseconds;
};

namespace experimental {

/**
 * If set, the client will throttle mutations in batch write jobs.
 *
 * This option is for batch write jobs where the goal is to avoid cluster
 * overload and prevent job failure more than it is to minimize latency or
 * maximize throughput.
 *
 * With this option set, the server rate-limits traffic to avoid overloading
 * your Bigtable cluster, while ensuring the cluster is under enough load to
 * trigger Bigtable [autoscaling] (if enabled).
 *
 * The [app profile] associated with these requests must be configured for
 * [single-cluster routing]. See #google::cloud::bigtable::AppProfileIdOption.
 *
 * @note This option must be supplied to `MakeDataConnection()` in order to take
 * effect.
 *
 * @see https://cloud.google.com/bigtable/docs/writes#flow-control
 *
 * [autoscaling]: https://cloud.google.com/bigtable/docs/autoscaling
 * [app profile]: https://cloud.google.com/bigtable/docs/app-profiles
 * [single-cluster routing]:
 * https://cloud.google.com/bigtable/docs/routing#single-cluster
 */
struct BulkApplyThrottlingOption {
  using Type = bool;
};

}  // namespace experimental

/// The complete list of options accepted by `bigtable::*Client`
using ClientOptionList =
    OptionList<DataEndpointOption, AdminEndpointOption,
               InstanceAdminEndpointOption, MinConnectionRefreshOption,
               MaxConnectionRefreshOption>;

/**
 * Option to configure the retry policy used by `Table`.
 *
 * @ingroup google-cloud-bigtable-options
 */
struct DataRetryPolicyOption {
  using Type = std::shared_ptr<DataRetryPolicy>;
};

/**
 * Option to configure the backoff policy used by `Table`.
 *
 * @ingroup google-cloud-bigtable-options
 */
struct DataBackoffPolicyOption {
  using Type = std::shared_ptr<BackoffPolicy>;
};

/**
 * Option to configure the idempotency policy used by `Table`.
 *
 * @ingroup google-cloud-bigtable-options
 */
struct IdempotentMutationPolicyOption {
  using Type = std::shared_ptr<bigtable::IdempotentMutationPolicy>;
};

/**
 * Enable [client-side metrics]
 *
 * When this option is enabled (the default), the client will export telemetry
 * to [Google Cloud Monitoring]. This information can help identify performance
 * bottlenecks, and is generally useful for monitoring and troubleshooting
 * applications.
 *
 * Sending this data does not incur any billing charges, and requires minimal
 * CPU (a single RPC every few minutes) or memory (a few KiB to batch the
 * telemetry).
 *
 * [client-side metrics]:
 * https://cloud.google.com/bigtable/docs/client-side-metrics [Google Cloud
 * Monitoring]: https://cloud.google.com/monitoring/docs
 */
struct EnableMetricsOption {
  using Type = bool;
};

/**
 * Metrics export period.
 *
 * When `EnableMetricsOption` is enabled, this option controls the frequency at
 * which metrics are exported to [Google Cloud Monitoring]. The default is 60
 * seconds. Values below 5 seconds are ignored.
 *
 * [Google Cloud Monitoring]: https://cloud.google.com/monitoring/docs
 */
struct MetricsPeriodOption {
  using Type = std::chrono::seconds;
};

using DataPolicyOptionList =
    OptionList<DataRetryPolicyOption, DataBackoffPolicyOption,
               IdempotentMutationPolicyOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_OPTIONS_H
