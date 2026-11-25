// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_EXPORTER_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_EXPORTER_OPTIONS_H

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include <opentelemetry/sdk/resource/resource.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Instruments whose name starts with this prefix are published under the
 * Google-owned internal namespace.
 *
 * Customer-visible SDK and gRPC instruments are published under
 * `storage.googleapis.com/client/`. Internal-only instruments created by this
 * library opt into `storage.googleapis.com/internal/client/` by using this
 * prefix, mirroring `bigtable.googleapis.com/internal/client/`.
 */
auto constexpr kInternalMetricPrefix = "internal/client/";

/// Returns the monitoring exporter options given the project and resource.
Options MetricsExporterOptions(
    Project const& project,
    opentelemetry::sdk::resource::Resource const& resource);

/// Returns the monitoring exporter connection options given the fully populated
/// storage options.
///
/// Propagates the client's credentials, CA certificates file, and proxy
/// options to the Cloud Monitoring connection.
///
/// This function does not check whether metrics export is enabled. Callers
/// check `DetectMetricsExportMode()` before connecting with the result.
Options MetricsExporterConnectionOptions(Options const& options);

/// How metrics export should be configured for a Storage client.
enum class MetricsExportMode {
  /// Skip metrics export.
  kDisabled,
  /// Export metrics using default monitored resource attributes.
  kWithoutHostResource,
  /// Export metrics using the detected host resource attributes.
  kWithHostResource,
};

/**
 * Determines the `MetricsExportMode` for the resolved @p options.
 *
 * Cloud Monitoring export targets the Monitoring service in the client's
 * universe domain using the client's credentials, so export is enabled only
 * when @p options are compatible with that target:
 *
 * - The client's endpoint must be a `google-c2p` target or `host[:port]` whose
 *   host is a subdomain of the client's universe domain.
 * - The client must not use `MakeInsecureCredentials()`.
 * - If the credentials specify a universe domain, it must match the client's
 *   universe domain; if it cannot be determined, export is disabled.
 *
 * Returns `kWithHostResource` when the client uses metadata-server credentials,
 * and `kWithoutHostResource` for other supported credentials. Impersonated
 * credentials follow their base credentials. Logs the reason at `INFO` when
 * returning `kDisabled`.
 */
MetricsExportMode DetectMetricsExportMode(Options const& options);

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_EXPORTER_OPTIONS_H
