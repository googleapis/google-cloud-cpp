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

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include <grpcpp/ext/otel_plugin.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <chrono>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

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

class ExporterRegistry {
 public:
  ExporterRegistry() = default;

  static ExporterRegistry& Singleton() {
    static auto* exporters = new ExporterRegistry;
    return *exporters;
  }

  // Returns `true` if @p authority is newly registered, `false` if @p authority
  // was already registered.
  bool Register(std::string authority) {
    std::unique_lock<std::mutex> lk(mu_);
    return known_authority_.insert(std::move(authority)).second;
  }

  void Clear() {
    std::unique_lock<std::mutex> lk(mu_);
    known_authority_.clear();
  }

  std::set<std::string> known_authority_;
  std::mutex mu_;
};

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

void EnableGrpcMetricsImpl(ExporterConfig config) {
  if (!ExporterRegistry::Singleton().Register(config.authority)) return;

  auto exporter = otel_internal::MakeMonitoringExporter(
      std::move(config.project),
      monitoring_v3::MakeMetricServiceConnection(
          std::move(config.exporter_connection_options)),
      config.exporter_options);

  auto provider = MakeGrpcMeterProvider(std::move(exporter),
                                        std::move(config.reader_options));

  auto const metrics = std::vector<absl::string_view>{
      absl::string_view{"grpc.lb.wrr.rr_fallback"},
      absl::string_view{"grpc.lb.wrr.endpoint_weight_not_yet_usable"},
      absl::string_view{"grpc.lb.wrr.endpoint_weight_stale"},
      absl::string_view{"grpc.lb.wrr.endpoint_weights"},
      absl::string_view{"grpc.xds_client.connected"},
      absl::string_view{"grpc.xds_client.server_failure"},
      absl::string_view{"grpc.xds_client.resource_updates_valid"},
      absl::string_view{"grpc.xds_client.resource_updates_invalid"},
      absl::string_view{"grpc.xds_client.resources"},
      absl::string_view{"grpc.lb.rls.cache_size"},
      absl::string_view{"grpc.lb.rls.cache_entries"},
      absl::string_view{"grpc.lb.rls.default_target_picks"},
      absl::string_view{"grpc.lb.rls.target_picks"},
      absl::string_view{"grpc.lb.rls.failed_picks"},
  };
  auto scope_filter =
      [authority = std::move(config.authority)](
          grpc::OpenTelemetryPluginBuilder::ChannelScope const& scope) {
        return scope.default_authority() == authority;
      };
  auto status =
      grpc::OpenTelemetryPluginBuilder()
          .SetMeterProvider(provider)
          .EnableMetrics(metrics)
          .AddOptionalLabel(absl::string_view("grpc.lb.locality"))
          .SetGenericMethodAttributeFilter([](absl::string_view target) {
            return absl::StartsWith(target, "google.storage.v2");
          })
          .SetChannelScopeFilter(std::move(scope_filter))
          .BuildAndRegisterGlobal();
  if (!status.ok()) {
    GCP_LOG(ERROR) << "Cannot register provider status=" << status.ToString();
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
