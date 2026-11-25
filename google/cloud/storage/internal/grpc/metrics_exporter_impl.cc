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
#include "google/cloud/opentelemetry/internal/monitoring_exporter.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/metrics_exporter_options.h"
#include "google/cloud/storage/internal/grpc/metrics_meter_provider.h"
#include "google/cloud/storage/internal/grpc/monitoring_project.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include <grpcpp/ext/otel_plugin.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
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
  reader_options.export_timeout_millis = std::chrono::milliseconds(
      options.get<storage_experimental::GrpcMetricsExportTimeoutOption>());
  return reader_options;
}

class ExporterRegistry {
 public:
  ExporterRegistry() = default;

  static ExporterRegistry& Singleton() {
    static auto* exporters = new ExporterRegistry;
    return *exporters;
  }

  // Registers a meter provider for @p authority if none exists yet, and runs
  // @p on_grpc_plugin once when @p enable_grpc_plugin is true.
  //
  // Holding `mu_` across both callbacks ensures that a concurrent caller for
  // the same authority waits until the provider is stored and (when requested)
  // registered with gRPC, rather than observing an uninitialized entry.
  template <typename Factory, typename OnGrpcPlugin>
  void Register(std::string const& authority, bool enable_grpc_plugin,
                Factory&& factory, OnGrpcPlugin&& on_grpc_plugin) {
    std::unique_lock<std::mutex> lk(mu_);
    auto& entry = known_authority_[authority];
    if (!entry.provider) {
      entry.provider = std::forward<Factory>(factory)();
    }
    if (enable_grpc_plugin && !entry.grpc_plugin_registered) {
      std::forward<OnGrpcPlugin>(on_grpc_plugin)(entry.provider);
      entry.grpc_plugin_registered = true;
    }
  }

  // Returns the provider stored for @p authority, or `nullptr` if
  // @p authority is unknown.
  std::shared_ptr<opentelemetry::metrics::MeterProvider> Find(
      std::string const& authority) {
    std::unique_lock<std::mutex> lk(mu_);
    auto const l = known_authority_.find(authority);
    if (l == known_authority_.end()) return nullptr;
    return l->second.provider;
  }

  void Clear() {
    std::unique_lock<std::mutex> lk(mu_);
    known_authority_.clear();
  }

  struct Entry {
    std::shared_ptr<opentelemetry::metrics::MeterProvider> provider;
    bool grpc_plugin_registered = false;
  };
  std::map<std::string, Entry> known_authority_;
  std::mutex mu_;
};

void RegisterGrpcPlugin(
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider,
    std::string const& authority) {
  auto const metrics = std::vector<std::string_view>{
      "grpc.lb.wrr.rr_fallback",
      "grpc.lb.wrr.endpoint_weight_not_yet_usable",
      "grpc.lb.wrr.endpoint_weight_stale",
      "grpc.lb.wrr.endpoint_weights",
      "grpc.xds_client.connected",
      "grpc.xds_client.server_failure",
      "grpc.xds_client.resource_updates_valid",
      "grpc.xds_client.resource_updates_invalid",
      "grpc.xds_client.resources",
      "grpc.lb.rls.cache_size",
      "grpc.lb.rls.cache_entries",
      "grpc.lb.rls.default_target_picks",
      "grpc.lb.rls.target_picks",
      "grpc.lb.rls.failed_picks",
  };
  auto scope_filter =
      [authority](grpc::OpenTelemetryPluginBuilder::ChannelScope const& scope) {
        return scope.default_authority() == authority;
      };
  auto status =
      grpc::OpenTelemetryPluginBuilder()
          .SetMeterProvider(provider)
          .EnableMetrics(metrics)
          .AddOptionalLabel("grpc.lb.locality")
          .SetGenericMethodAttributeFilter([](std::string_view target) {
            return absl::StartsWith(target, "google.storage.v2");
          })
          .SetChannelScopeFilter(std::move(scope_filter))
          .BuildAndRegisterGlobal();
  if (!status.ok()) {
    GCP_LOG(ERROR) << "Cannot register provider status=" << status.ToString();
  }
}

}  // namespace

bool AnyMetricsEnabled(Options const& options) {
  return options.get<storage_experimental::EnableGrpcMetricsOption>() ||
         options.get<storage_experimental::EnableOTelMetricsOption>() ||
         options.get<storage_experimental::EnableOTelDebugMetricsOption>();
}

std::optional<ExporterConfig> MakeMeterProviderConfig(
    opentelemetry::sdk::resource::Resource const& resource,
    Options const& options) {
  if (!AnyMetricsEnabled(options)) {
    return std::nullopt;
  }
  MetricsExportMode const mode = DetectMetricsExportMode(options);
  if (mode == MetricsExportMode::kDisabled) return std::nullopt;
  bool const enable_grpc =
      options.get<storage_experimental::EnableGrpcMetricsOption>();
  // Attach host resource attributes only when the client authenticates via the
  // host's metadata server; otherwise use default client resource labels.
  opentelemetry::sdk::resource::Resource const& client_resource =
      mode == MetricsExportMode::kWithHostResource
          ? resource
          : opentelemetry::sdk::resource::Resource::GetEmpty();
  std::optional<Project> project = MonitoringProject(client_resource, options);
  if (!project) return std::nullopt;

  Options exporter_options = MetricsExporterOptions(*project, client_resource);
  if (options.has<storage_experimental::GrpcMetricsExcludedLabelsOption>()) {
    exporter_options.set<otel_internal::ResourceFilterDataFnOption>(
        options.get<storage_experimental::GrpcMetricsExcludedLabelsOption>());
  }

  auto exporter_connection_options = MetricsExporterConnectionOptions(options);
  return ExporterConfig{std::move(*project),
                        std::move(exporter_options),
                        std::move(exporter_connection_options),
                        MakeReaderOptions(options),
                        options.get<AuthorityOption>(),
                        enable_grpc};
}

void EnableGrpcMetricsImpl(ExporterConfig config) {
  EnableGrpcMetricsImpl(std::move(config), RegisterGrpcPlugin);
}

void EnableGrpcMetricsImpl(
    ExporterConfig config,
    std::function<
        void(std::shared_ptr<opentelemetry::metrics::MeterProvider> const&,
             std::string const&)>
        register_grpc_plugin) {
  ExporterRegistry::Singleton().Register(
      config.authority, config.enable_grpc_plugin,
      [&] {
        auto exporter = otel::MakeMonitoringExporter(
            std::move(config.project),
            monitoring_v3::MakeMetricServiceConnection(
                std::move(config.exporter_connection_options)),
            std::move(config.exporter_options));
        return MakeGrpcMeterProvider(std::move(exporter),
                                     std::move(config.reader_options));
      },
      [&](std::shared_ptr<opentelemetry::metrics::MeterProvider> const&
              provider) { register_grpc_plugin(provider, config.authority); });
}

std::shared_ptr<opentelemetry::metrics::MeterProvider> FindMeterProvider(
    std::string const& authority) {
  return ExporterRegistry::Singleton().Find(authority);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
