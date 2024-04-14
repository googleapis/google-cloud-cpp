// Copyright 2024 Google LLC
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

#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/monitoring/v3/metric_client.h"
#include "google/cloud/opentelemetry/internal/time_series.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include <memory>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class MonitoringExporter final
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  explicit MonitoringExporter(
      Project project,
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn)
      : project_(std::move(project)), client_(std::move(conn)) {}

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) const noexcept override {
    return opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  }

  opentelemetry::sdk::common::ExportResult Export(
      opentelemetry::sdk::metrics::ResourceMetrics const& data) noexcept
      override {
    auto result =
        internal::NoExceptAction<opentelemetry::sdk::common::ExportResult>(
            [&] { return ExportImpl(data); });
    if (result) return *std::move(result);
    GCP_LOG(WARNING) << "Exception thrown while exporting metrics.";
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  bool ForceFlush(std::chrono::microseconds) noexcept override { return false; }

  bool Shutdown(std::chrono::microseconds) noexcept override { return true; }

 private:
  opentelemetry::sdk::common::ExportResult ExportImpl(
      opentelemetry::sdk::metrics::ResourceMetrics const& data) {
    auto request = ToRequest(data);
    request.set_name(project_.FullName());
    if (request.time_series().empty()) {
      GCP_LOG(WARNING) << "Cloud Monitoring Export skipped. No TimeSeries "
                          "added to request.";
      return opentelemetry::sdk::common::ExportResult::kSuccess;
    }

    // TODO(#13869) - Exporters of Google-defined metrics will need to use
    // `CreateServiceTimeSeries` instead of `CreateTimeSeries`. We will add that
    // complexity later.
    auto status = client_.CreateTimeSeries(request);
    if (status.ok()) return opentelemetry::sdk::common::ExportResult::kSuccess;
    GCP_LOG(WARNING) << "Cloud Monitoring Export failed with status=" << status;
    if (status.code() == StatusCode::kInvalidArgument) {
      return opentelemetry::sdk::common::ExportResult::kFailureInvalidArgument;
    }
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  Project project_;
  monitoring_v3::MetricServiceClient client_;
};
}  // namespace

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    Project project,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn) {
  return std::make_unique<MonitoringExporter>(std::move(project),
                                              std::move(conn));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
