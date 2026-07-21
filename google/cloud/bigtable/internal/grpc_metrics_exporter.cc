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

#include "google/cloud/bigtable/internal/grpc_metrics_exporter.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/opentelemetry/internal/monitoring_exporter.h"
#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include <grpcpp/ext/otel_plugin.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/semconv/incubating/cloud_attributes.h>
#include <opentelemetry/semconv/incubating/faas_attributes.h>
#include <opentelemetry/semconv/incubating/host_attributes.h>
#include <opentelemetry/version.h>
#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 10)
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#endif  // OPENTELEMETRY_VERSION_MAJOR > 1 || (OPENTELEMETRY_VERSION_MAJOR == 1
        // && OPENTELEMETRY_VERSION_MINOR >= 10)
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector.h>
#include <opentelemetry/sdk/metrics/view/meter_selector.h>
#include <opentelemetry/sdk/metrics/view/view.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

GrpcMetricsExporterRegistry& GrpcMetricsExporterRegistry::Singleton() {
  static auto* registry = new GrpcMetricsExporterRegistry;
  return *registry;
}

bool GrpcMetricsExporterRegistry::Register(std::string authority) {
  std::unique_lock<std::mutex> lk(mu_);
  return known_authority_.insert(std::move(authority)).second;
}

void GrpcMetricsExporterRegistry::Clear() {
  std::unique_lock<std::mutex> lk(mu_);
  known_authority_.clear();
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

std::vector<double> MakeLatencyHistogramBoundaries() {
  static auto const kBoundaries = [] {
    using dseconds = std::chrono::duration<double, std::ratio<1>>;
    std::vector<double> boundaries;
    auto boundary = std::chrono::milliseconds(0);
    auto increment = std::chrono::milliseconds(2);
    for (int i = 0; i != 50; ++i) {
      boundaries.push_back(
          std::chrono::duration_cast<dseconds>(boundary).count());
      boundary += increment;
    }
    increment = std::chrono::milliseconds(10);
    for (int i = 0; i != 150 && boundary <= std::chrono::minutes(5); ++i) {
      boundaries.push_back(
          std::chrono::duration_cast<dseconds>(boundary).count());
      if (i != 0 && i % 10 == 0) increment *= 2;
      boundary += increment;
    }
    return boundaries;
  }();
  return kBoundaries;
}

namespace {

void AddHistogramView(opentelemetry::sdk::metrics::MeterProvider& provider,
                      std::vector<double> boundaries, std::string const& name,
                      std::string const& unit) {
  auto constexpr kGrpcMeterName = "grpc-c++";
  auto constexpr kGrpcSchema = "";

  auto histogram_aggregation_config = std::make_unique<
      opentelemetry::sdk::metrics::HistogramAggregationConfig>();
  histogram_aggregation_config->boundaries_ = std::move(boundaries);
  auto aggregation_config =
      std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig>(
          std::move(histogram_aggregation_config));

  auto description = absl::StrCat("A view of ", name,
                                  " with histogram boundaries more appropriate "
                                  "for Google Cloud Bigtable RPCs");

#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 23)
  (void)unit;
  provider.AddView(
      opentelemetry::sdk::metrics::InstrumentSelectorFactory::Create(
          opentelemetry::sdk::metrics::InstrumentType::kHistogram, name, unit),
      opentelemetry::sdk::metrics::MeterSelectorFactory::Create(
          kGrpcMeterName, grpc::Version(), kGrpcSchema),
      opentelemetry::sdk::metrics::ViewFactory::Create(
          name, std::move(description),
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          std::move(aggregation_config)));
#elif OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 10)
  provider.AddView(
      opentelemetry::sdk::metrics::InstrumentSelectorFactory::Create(
          opentelemetry::sdk::metrics::InstrumentType::kHistogram, name, unit),
      opentelemetry::sdk::metrics::MeterSelectorFactory::Create(
          kGrpcMeterName, grpc::Version(), kGrpcSchema),
      opentelemetry::sdk::metrics::ViewFactory::Create(
          name, std::move(description), unit,
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          std::move(aggregation_config)));
#else   // OPENTELEMETRY_VERSION_MAJOR > 1 || (OPENTELEMETRY_VERSION_MAJOR == 1
        // && OPENTELEMETRY_VERSION_MINOR >= 10)
  (void)unit;
  provider.AddView(
      std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
          opentelemetry::sdk::metrics::InstrumentType::kHistogram, name),
      std::make_unique<opentelemetry::sdk::metrics::MeterSelector>(
          kGrpcMeterName, grpc::Version(), kGrpcSchema),
      std::make_unique<opentelemetry::sdk::metrics::View>(
          name, std::move(description),
          opentelemetry::sdk::metrics::AggregationType::kHistogram,
          std::move(aggregation_config)));
#endif  // OPENTELEMETRY_VERSION_MAJOR > 1 || (OPENTELEMETRY_VERSION_MAJOR == 1
        // && OPENTELEMETRY_VERSION_MINOR >= 23)
}

}  // namespace

std::shared_ptr<opentelemetry::metrics::MeterProvider> MakeGrpcMeterProvider(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        reader_options) {
#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 10)
  auto provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      opentelemetry::sdk::resource::Resource::Create({}));
#else   // OPENTELEMETRY_VERSION_MAJOR > 1 || (OPENTELEMETRY_VERSION_MAJOR == 1
        // && OPENTELEMETRY_VERSION_MINOR >= 10)
  auto provider = std::make_unique<opentelemetry::sdk::metrics::MeterProvider>(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      opentelemetry::sdk::resource::Resource::Create({}));
#endif  // OPENTELEMETRY_VERSION_MAJOR > 1 || (OPENTELEMETRY_VERSION_MAJOR == 1
        // && OPENTELEMETRY_VERSION_MINOR >= 10)
  auto* p =
      static_cast<opentelemetry::sdk::metrics::MeterProvider*>(provider.get());
  AddHistogramView(*p, MakeLatencyHistogramBoundaries(),
                   "grpc.client.attempt.duration", "s");

#if OPENTELEMETRY_VERSION_MAJOR > 1 || OPENTELEMETRY_VERSION_MINOR >= 10
  p->AddMetricReader(
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
          std::move(exporter), std::move(reader_options)));
#else   // OPENTELEMETRY_VERSION_MAJOR > 1 || OPENTELEMETRY_VERSION_MINOR >= 10
  p->AddMetricReader(
      std::make_unique<
          opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
          std::move(exporter), std::move(reader_options)));
#endif  // OPENTELEMETRY_VERSION_MAJOR > 1 || OPENTELEMETRY_VERSION_MINOR >= 10

  return std::shared_ptr<opentelemetry::metrics::MeterProvider>(
      std::move(provider));
}

MonitoredResourceResult MakeMonitoredResource(
    opentelemetry::sdk::metrics::PointDataAttributes const& pda,
    opentelemetry::sdk::resource::Resource const& detected_resource,
    Options const& options, std::string const& client_uid) {
  namespace sc = ::opentelemetry::semconv;
  google::api::MonitoredResource resource;
  resource.set_type("bigtable.googleapis.com/Client");
  auto& labels = *resource.mutable_labels();
  auto const& attributes = pda.attributes.GetAttributes();
  auto get_attr = [&](std::string const& key) {
    auto it = attributes.find(key);
    if (it == attributes.end()) return std::string{};
    return opentelemetry::nostd::get<std::string>(it->second);
  };
  auto const& detected_attributes = detected_resource.GetAttributes();
  auto by_name = [&](std::string const& name, std::string default_value = {}) {
    auto const l = detected_attributes.find(name);
    if (l == detected_attributes.end()) return default_value;
    return opentelemetry::nostd::get<std::string>(l->second);
  };

  auto project_id = get_attr("project_id");
  // Fall back to resolving the project ID from `InstanceChannelAffinityOption`
  // if the static `ProjectIdOption` is not set on the client (which is common
  // in client configurations with instance routing / channel affinity).
  if (project_id.empty() &&
      options.has<bigtable_internal::InstanceChannelAffinityOption>()) {
    auto const& instances =
        options.get<bigtable_internal::InstanceChannelAffinityOption>();
    if (!instances.empty()) {
      project_id = instances[0].project_id();
    }
  }
  if (project_id.empty()) {
    project_id = by_name(sc::cloud::kCloudAccountId);
  }

  labels["project_id"] = project_id;
  labels["instance"] = get_attr("instance");
  labels["app_profile"] = options.get<bigtable::AppProfileIdOption>();
  labels["client_name"] = "cpp.Bigtable/" + bigtable::version_string();
  labels["uuid"] = client_uid;

  auto client_project = by_name(sc::cloud::kCloudAccountId);
  if (client_project.empty()) {
    client_project = project_id;
  }
  if (!client_project.empty()) {
    labels["client_project"] = client_project;
  }
  labels["location"] = by_name(sc::cloud::kCloudAvailabilityZone,
                               by_name(sc::cloud::kCloudRegion, "global"));
  labels["cloud_platform"] = by_name(sc::cloud::kCloudPlatform, "unknown");
  labels["host_id"] = by_name("faas.id", by_name(sc::host::kHostId, "unknown"));
  auto hostname = by_name(sc::host::kHostName);
  if (!hostname.empty()) {
    labels["hostname"] = hostname;
  }

  return MonitoredResourceResult{std::move(project_id), std::move(resource)};
}

GrpcMetricsExporter::GrpcMetricsExporter(
    std::shared_ptr<monitoring_v3::MetricServiceConnection> const& conn,
    Options const& options, std::string const& client_uid) {
  auto authority = options.get<AuthorityOption>();
  if (!GrpcMetricsExporterRegistry::Singleton().Register(authority)) return;

  auto detector = otel::MakeResourceDetector();
  auto detected_resource = detector->Detect();

  auto dynamic_resource_fn =
      [options, client_uid, detected_resource = std::move(detected_resource)](
          opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        auto res =
            MakeMonitoredResource(pda, detected_resource, options, client_uid);
        return std::make_pair(std::move(res.project_id),
                              std::move(res.resource));
      };

  std::set<std::string> excluded_labels{"project_id", "instance"};
  auto resource_filter_fn =
      [excluded_labels = std::move(excluded_labels)](std::string const& key) {
        return internal::Contains(excluded_labels, key);
      };

  auto exporter = otel_internal::MakeMonitoringExporter(
      dynamic_resource_fn, resource_filter_fn, conn, options);

  auto reader_options =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
  // otel::PeriodicExportingMetricReader enforces that export_timeout_millis <
  // export_interval_millis.
  reader_options.export_interval_millis =
      options.get<bigtable::MetricsPeriodOption>();
  reader_options.export_timeout_millis =
      (std::min)(std::chrono::milliseconds(std::chrono::seconds(30)),
                 reader_options.export_interval_millis / 2);

  provider_ =
      MakeGrpcMeterProvider(std::move(exporter), std::move(reader_options));

  auto const metrics = std::vector<absl::string_view>{
      absl::string_view{"grpc.client.attempt.duration"},
      absl::string_view{"grpc.lb.rls.default_target_picks"},
      absl::string_view{"grpc.lb.rls.target_picks"},
      absl::string_view{"grpc.lb.rls.failed_picks"},
      absl::string_view{"grpc.xds_client.server_failure"},
      absl::string_view{"grpc.xds_client.resource_updates_invalid"},
      absl::string_view{"grpc.subchannel.disconnections"},
      absl::string_view{"grpc.subchannel.connection_attempts_succeeded"},
      absl::string_view{"grpc.subchannel.connection_attempts_failed"},
      absl::string_view{"grpc.subchannel.open_connections"},
  };
  auto scope_filter =
      [authority = std::move(authority)](
          grpc::OpenTelemetryPluginBuilder::ChannelScope const& scope) {
        GCP_LOG(INFO) << "GrpcMetricsExporter: scope filter checking target="
                      << scope.target()
                      << " default_authority=" << scope.default_authority()
                      << " vs expected authority=" << authority;
        return scope.default_authority() == authority;
      };
  auto status =
      grpc::OpenTelemetryPluginBuilder()
          .SetMeterProvider(provider_)
          .EnableMetrics(metrics)
          .SetGenericMethodAttributeFilter([](absl::string_view target) {
            return absl::StartsWith(target, "google.bigtable.v2");
          })
          .SetChannelScopeFilter(std::move(scope_filter))
          .BuildAndRegisterGlobal();
  if (!status.ok()) {
    GCP_LOG(ERROR) << "Cannot register provider status=" << status.ToString();
  }
}

GrpcMetricsExporter::~GrpcMetricsExporter() {
  if (provider_) {
    auto* p = static_cast<opentelemetry::sdk::metrics::MeterProvider*>(
        provider_.get());
    p->Shutdown();
  }
}

#else  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

GrpcMetricsExporter::GrpcMetricsExporter(
    std::shared_ptr<monitoring_v3::MetricServiceConnection> const&,
    Options const&, std::string const&) {}

GrpcMetricsExporter::~GrpcMetricsExporter() = default;

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
