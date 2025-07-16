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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_MONITORING_EXPORTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_MONITORING_EXPORTER_H

#include "google/cloud/monitoring/v3/metric_client.h"
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using MonitoredResourceFromDataFn =
    std::function<std::pair<std::string, google::api::MonitoredResource>(
        opentelemetry::sdk::metrics::PointDataAttributes const&)>;

using ResourceFilterDataFn = std::function<bool(std::string const&)>;

class MonitoringExporter final
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  MonitoringExporter(
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      otel_internal::MonitoredResourceFromDataFn resource_fn,
      otel_internal::ResourceFilterDataFn filter_fn, Options const& options);

  MonitoringExporter(
      Project project,
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      Options const& options);

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) const noexcept override {
    return opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  }

  opentelemetry::sdk::common::ExportResult Export(
      opentelemetry::sdk::metrics::ResourceMetrics const& data) noexcept
      override;

  bool ForceFlush(std::chrono::microseconds) noexcept override { return false; }

  bool Shutdown(std::chrono::microseconds) noexcept override { return true; }

 private:
  opentelemetry::sdk::common::ExportResult ExportImpl(
      opentelemetry::sdk::metrics::ResourceMetrics const& data);
  opentelemetry::sdk::common::ExportResult SendRequests(
      std::vector<google::monitoring::v3::CreateTimeSeriesRequest> const&
          requests);

  absl::optional<Project> project_;
  monitoring_v3::MetricServiceClient client_;
  otel::MetricNameFormatterOption::Type formatter_;
  bool use_service_time_series_;
  absl::optional<google::api::MonitoredResource> mr_proto_;
  otel_internal::MonitoredResourceFromDataFn resource_fn_;
  otel_internal::ResourceFilterDataFn filter_fn_;
};

Options DefaultOptions(Options o);

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(MonitoredResourceFromDataFn resource_fn,
                       ResourceFilterDataFn filter_fn, Options options = {});

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    MonitoredResourceFromDataFn resource_fn, ResourceFilterDataFn filter_fn,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_MONITORING_EXPORTER_H
