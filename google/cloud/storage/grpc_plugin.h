// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
/**
 * Contains experimental features for the GCS C++ Client Library.
 *
 * @warning The types, functions, aliases, and objects in this namespace are
 *   subject to change without notice.
 */
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configure the GCS+gRPC plugin.
 *
 * @deprecated use `google::cloud::storage::Client()` to create JSON-based
 *     clients and `google::cloud::storage::MakeGrpcClient()` to create
 *     gRPC-based clients. If you need to pick one dynamically a simple
 *     `if()` statement or ternary expression can do the job.
 */
struct [[deprecated(
    "use storage::Client() or storage::MakeGrpcClient()")]] GrpcPluginOption {
  using Type = std::string;
};

/**
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @param opts the configuration parameters for the Client.
 *
 * @warning At present, GCS gRPC is GA with Allowlist. To access this API,
 *   kindly contact the Google Cloud Storage gRPC team at
 *   gcs-grpc-contact@google.com with a list of GCS buckets you would like to
 *   Allowlist. Please note that while the **service** is GA (with Allowlist),
 *   the client library features remain experimental and subject to change
 *   without notice.
 *
 * @par Example
 * @snippet storage_grpc_samples.cc grpc-read-write
 */
google::cloud::storage::Client DefaultGrpcClient(Options opts = {});

/**
 * Enable gRPC telemetry for GCS RPCs.
 *
 * Troubleshooting problems with GCS over gRPC is difficult without some
 * telemetry indicating how the client is configured, and what load balancing
 * information was available to the gRPC library.
 *
 * When this option is enabled (the default), the GCS client will export the
 * gRPC telemetry discussed in [gRFC/66] and [gRFC/78] to
 * [Google Cloud Monitoring]. Google Cloud Support can use this information to
 * more quickly diagnose problems related to GCS and gRPC.
 *
 * Sending this data does not incur any billing charges, and requires minimal
 * CPU (a single RPC every few minutes) or memory (a few KiB to batch the
 * telemetry).
 *
 * [gRFC/66]: https://github.com/grpc/proposal/blob/master/A66-otel-stats.md
 * [gRFC/78]:
 * https://github.com/grpc/proposal/blob/master/A78-grpc-metrics-wrr-pf-xds.md
 * [Google Cloud Monitoring]: https://cloud.google.com/monitoring/docs
 */
struct EnableGrpcMetrics {
  using Type = bool;
};

/**
 * gRPC telemetry export period.
 *
 * When `EnableGrpcMetrics` is enabled, this option controls the frequency at
 * which metrics are exported to [Google Cloud Monitoring]. The default is 60
 * seconds. Values below 60 seconds are ignored.
 *
 * [Google Cloud Monitoring]: https://cloud.google.com/monitoring/docs
 */
struct GrpcMetricsPeriod {
  using Type = std::chrono::seconds;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H
