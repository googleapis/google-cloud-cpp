// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/operation_context_factory.h"

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/opentelemetry/internal/monitoring_exporter.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/internal/algorithm.h"
#include <google/api/monitored_resource.pb.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

OperationContextFactory::~OperationContextFactory() = default;

std::shared_ptr<OperationContext> OperationContextFactory::ReadRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> OperationContextFactory::ReadRows(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> OperationContextFactory::MutateRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> OperationContextFactory::MutateRows(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> OperationContextFactory::CheckAndMutateRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> OperationContextFactory::SampleRowKeys(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> OperationContextFactory::ReadModifyWriteRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> SimpleOperationContextFactory::ReadRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> SimpleOperationContextFactory::ReadRows(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> SimpleOperationContextFactory::MutateRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> SimpleOperationContextFactory::MutateRows(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext>
SimpleOperationContextFactory::CheckAndMutateRow(std::string const&,
                                                 std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext> SimpleOperationContextFactory::SampleRowKeys(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext>
SimpleOperationContextFactory::ReadModifyWriteRow(std::string const&,
                                                  std::string const&) {
  return std::make_shared<OperationContext>();
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

MetricsOperationContextFactory::MetricsOperationContextFactory(
    std::string client_uid,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options)  // NOLINT(performance-unnecessary-value-param)
    : client_uid_(std::move(client_uid)) {
  auto constexpr kResourceType = "bigtable_client_raw";
  auto constexpr kBigtableMetricNamePath =
      "bigtable.googleapis.com/internal/client/";
  auto constexpr kProjectLabel = "project_id";
  auto constexpr kInstanceLabel = "instance";
  auto constexpr kTableLabel = "table";
  auto constexpr kClusterLabel = "cluster";
  auto constexpr kZoneLabel = "zone";

  auto dynamic_resource_fn =
      [&](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        google::api::MonitoredResource resource;
        resource.set_type(kResourceType);
        auto& labels = *resource.mutable_labels();
        auto const& attributes = pda.attributes.GetAttributes();
        labels[kProjectLabel] =
            absl::get<std::string>(attributes.find(kProjectLabel)->second);
        labels[kInstanceLabel] =
            absl::get<std::string>(attributes.find(kInstanceLabel)->second);
        labels[kTableLabel] =
            absl::get<std::string>(attributes.find(kTableLabel)->second);
        labels[kClusterLabel] =
            absl::get<std::string>(attributes.find(kClusterLabel)->second);
        labels[kZoneLabel] =
            absl::get<std::string>(attributes.find(kZoneLabel)->second);
        return std::make_pair(labels[kProjectLabel], resource);
      };

  std::set<std::string> s{kProjectLabel, kInstanceLabel, kTableLabel,
                          kClusterLabel, kZoneLabel};
  auto resource_filter_fn = [resource_labels =
                                 std::move(s)](std::string const& key) {
    return internal::Contains(resource_labels, key);
  };

  auto reader_options =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
  reader_options.export_timeout_millis = std::chrono::seconds(30);
  reader_options.export_interval_millis =
      options.get<bigtable::MetricsPeriodOption>();

  options.set<otel::ServiceTimeSeriesOption>(true)
      .set<otel::MetricNameFormatterOption>(
          [=](auto name) { return kBigtableMetricNamePath + name; });

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter;
  if (conn) {
    exporter = otel_internal::MakeMonitoringExporter(
        dynamic_resource_fn, resource_filter_fn, std::move(conn),
        std::move(options));
  } else {
    exporter = otel_internal::MakeMonitoringExporter(
        dynamic_resource_fn, resource_filter_fn, std::move(options));
  }

  auto reader =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
          std::move(exporter), reader_options);

  auto context = opentelemetry::sdk::metrics::MeterContextFactory::Create(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      // NOTE: this skips OTel's built in resource detection which is more
      // confusing than helpful. (The default is {{"service_name",
      // "unknown_service" }}). And after #14930, this gets copied into our
      // resource labels. oh god why.
      opentelemetry::sdk::resource::Resource::GetEmpty());
  context->AddMetricReader(std::move(reader));
  provider_ = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::move(context));
}

MetricsOperationContextFactory::MetricsOperationContextFactory(
    std::string client_uid, Options options)
    : MetricsOperationContextFactory(std::move(client_uid), nullptr,
                                     std::move(options)) {}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::ReadRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::ReadRows(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::MutateRow(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::MutateRows(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::CheckAndMutateRow(std::string const&,
                                                  std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::SampleRowKeys(
    std::string const&, std::string const&) {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::ReadModifyWriteRow(std::string const&,
                                                   std::string const&) {
  return std::make_shared<OperationContext>();
}

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
