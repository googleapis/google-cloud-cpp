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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/grpc/metrics_exporter_impl.h"
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/metrics_exporter_options.h"
#include "google/cloud/storage/internal/grpc/metrics_meter_provider.h"
#include "google/cloud/storage/internal/grpc/monitoring_project.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include <opentelemetry/sdk/resource/resource.h>
#include <chrono>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto MakeReaderOptions(Options const& options) {
  auto reader_options =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
  reader_options.export_interval_millis = std::chrono::milliseconds(
      options.get<storage_experimental::GrpcMetricsPeriodOption>());
  reader_options.export_timeout_millis =
      std::chrono::milliseconds(std::chrono::seconds(30));
  return reader_options;
}

}  // namespace

absl::optional<ExporterConfig> MakeMeterProviderConfig(
    opentelemetry::sdk::resource::Resource const& resource,
    Options const& options) {
  if (!options.get<storage_experimental::EnableGrpcMetricsOption>()) {
    return absl::nullopt;
  }
  auto project = MonitoringProject(resource, options);
  if (!project) return absl::nullopt;

  auto exporter_options = MetricsExporterOptions(*project, resource);
  auto exporter_connection_options = MetricsExporterConnectionOptions(options);
  return ExporterConfig{std::move(*project), std::move(exporter_options),
                        std::move(exporter_connection_options),
                        MakeReaderOptions(options),
                        options.get<AuthorityOption>()};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
