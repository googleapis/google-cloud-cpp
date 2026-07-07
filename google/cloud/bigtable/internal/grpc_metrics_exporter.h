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
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/api/monitored_resource.pb.h"
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

namespace google {
namespace cloud {
namespace monitoring_v3 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class MetricServiceConnection;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace monitoring_v3

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class GrpcMetricsExporterRegistry {
 public:
  GrpcMetricsExporterRegistry() = default;

  static GrpcMetricsExporterRegistry& Singleton();

  // Returns true if authority is newly registered, false if it was already
  // registered.
  bool Register(std::string authority);

  void Clear();

 private:
  std::set<std::string> known_authority_;
  std::mutex mu_;
};

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
struct MonitoredResourceResult {
  std::string project_id;
  google::api::MonitoredResource resource;
};

MonitoredResourceResult MakeMonitoredResource(
    opentelemetry::sdk::metrics::PointDataAttributes const& pda,
    Options const& options, std::string const& client_uid);
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class GrpcMetricsExporter {
 public:
  GrpcMetricsExporter(
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      Options const& options, std::string const& client_uid);

 private:
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GRPC_METRICS_EXPORTER_H
