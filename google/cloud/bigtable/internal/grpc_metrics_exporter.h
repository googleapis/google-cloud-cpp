// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GRPC_METRICS_EXPORTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GRPC_METRICS_EXPORTER_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/api/monitored_resource.pb.h"
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <vector>
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

namespace google {
namespace cloud {
namespace monitoring_v3 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class MetricServiceConnection;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace monitoring_v3

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class GrpcMetricsExporter;

/**
 * Thread-safe registry managing the lifetime and deduplication of
 * `GrpcMetricsExporter` instances.
 *
 * gRPC OpenTelemetry plugin registration (`BuildAndRegisterGlobal`) is
 * process-global per target channel authority. If an exporter's lifetime were
 * bound to an individual `DataConnectionImpl` without sharing, closing a single
 * connection would destroy the `MeterProvider` and call `Shutdown()`,
 * prematurely halting metric export globally for other active connections in
 * the same process.
 *
 * Conversely, keeping static `std::shared_ptr` singletons permanently violates
 * Google C++ Style Guide rules against static non-trivially destructible
 * objects and prevents `MeterProvider::Shutdown()` from ever flushing metrics
 * upon connection shutdown.
 *
 * `GrpcMetricsExporterRegistry` resolves both requirements:
 * 1. **Deduplication / Sharing**: Tracks active `GrpcMetricsExporter` instances
 *    via `std::weak_ptr` keyed by channel authority. Multiple connection
 *    instances targeting the same authority share ownership of the same
 *    exporter `std::shared_ptr`.
 * 2. **Clean Shutdown**: When all `DataConnectionImpl` instances sharing an
 *    exporter are closed, the last `std::shared_ptr` is destroyed, invoking
 *    `~GrpcMetricsExporter()` which cleanly calls `MeterProvider::Shutdown()`
 *    and unregisters the authority from the registry.
 */
class GrpcMetricsExporterRegistry {
 public:
  static GrpcMetricsExporterRegistry& Singleton();

  std::shared_ptr<GrpcMetricsExporter> GetOrCreate(
      std::shared_ptr<monitoring_v3::MetricServiceConnection> const& conn,
      Options const& options, std::string const& client_uid);

  void Unregister(std::string const& authority);

  void Clear();

 private:
  GrpcMetricsExporterRegistry() = default;

  std::map<std::string, std::weak_ptr<GrpcMetricsExporter>> exporters_;
  std::mutex mu_;
};

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
struct MonitoredResourceResult {
  std::string project_id;
  google::api::MonitoredResource resource;
};

MonitoredResourceResult MakeMonitoredResource(
    opentelemetry::sdk::metrics::PointDataAttributes const& pda,
    opentelemetry::sdk::resource::Resource const& detected_resource,
    Options const& options, std::string const& client_uid);

inline MonitoredResourceResult MakeMonitoredResource(
    opentelemetry::sdk::metrics::PointDataAttributes const& pda,
    Options const& options, std::string const& client_uid) {
  return MakeMonitoredResource(
      pda, opentelemetry::sdk::resource::Resource::Create({}), options,
      client_uid);
}

std::vector<double> MakeLatencyHistogramBoundaries();

std::shared_ptr<opentelemetry::metrics::MeterProvider> MakeGrpcMeterProvider(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        reader_options);
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

class GrpcMetricsExporter {
 public:
  GrpcMetricsExporter(
      std::shared_ptr<monitoring_v3::MetricServiceConnection> const& conn,
      Options const& options, std::string const& client_uid);
  ~GrpcMetricsExporter();

 private:
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
  std::string authority_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GRPC_METRICS_EXPORTER_H
