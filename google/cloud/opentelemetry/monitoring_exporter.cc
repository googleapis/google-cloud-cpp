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
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/opentelemetry/internal/time_series.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include <memory>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

Options DefaultOptions(Options o) {
  if (!o.has<MetricNameFormatterOption>()) {
    o.set<MetricNameFormatterOption>([](std::string s) {
      return "workload.googleapis.com/" + std::move(s);
    });
  }
  return o;
}

class MonitoringExporter final
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  MonitoringExporter(
      Project project,
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      otel_internal::MonitoredResourceFromDataFn resource_fn,
      otel_internal::ResourceFilterDataFn filter_fn, Options const& options)
      : project_(std::move(project)),
        client_(std::move(conn)),
        formatter_(options.get<MetricNameFormatterOption>()),
        use_service_time_series_(options.get<ServiceTimeSeriesOption>()),
        mr_proto_(internal::FetchOption<MonitoredResourceOption>(options)),
        resource_fn_(std::move(resource_fn)),
        filter_fn_(std::move(filter_fn)) {}

  MonitoringExporter(
      Project project,
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      Options const& options)
      : MonitoringExporter(std::move(project), std::move(conn), nullptr,
                           nullptr, options) {}

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
    auto result = opentelemetry::sdk::common::ExportResult::kSuccess;

    auto tss = otel_internal::ToTimeSeries(data, formatter_, filter_fn_);
    if (tss.empty()) {
      GCP_LOG(INFO) << "Cloud Monitoring Export skipped. No data.";
      // Return early to save the littlest bit of processing.
      return result;
    }

    google::api::MonitoredResource mr;
    if (resource_fn_) {
      mr = resource_fn_(data);
    } else {
      mr = otel_internal::ToMonitoredResource(data, mr_proto_);
    }
    auto requests =
        otel_internal::ToRequests(project_.FullName(), mr, std::move(tss));
    for (auto& request : requests) {
      auto status = use_service_time_series_
                        ? client_.CreateServiceTimeSeries(request)
                        : client_.CreateTimeSeries(request);
      if (status.ok()) continue;
      GCP_LOG(WARNING) << "Cloud Monitoring Export failed with status="
                       << status;
      // Ultimately, we can only report one error, even though we may send
      // multiple RPCs. If *all* failures are `kInvalidArgument` we should
      // report that. Otherwise, we will report a generic failure.
      if (status.code() == StatusCode::kInvalidArgument &&
          result == opentelemetry::sdk::common::ExportResult::kSuccess) {
        result =
            opentelemetry::sdk::common::ExportResult::kFailureInvalidArgument;
      } else if (status.code() != StatusCode::kInvalidArgument) {
        result = opentelemetry::sdk::common::ExportResult::kFailure;
      }
    }
    return result;
  }

  Project project_;
  monitoring_v3::MetricServiceClient client_;
  MetricNameFormatterOption::Type formatter_;
  bool use_service_time_series_;
  absl::optional<google::api::MonitoredResource> mr_proto_;
  otel_internal::MonitoredResourceFromDataFn resource_fn_;
  otel_internal::ResourceFilterDataFn filter_fn_;
};
}  // namespace

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    Project project,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options) {
  options = DefaultOptions(std::move(options));
  return std::make_unique<MonitoringExporter>(std::move(project),
                                              std::move(conn), options);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel

// TODO: move this to a file in google/cloud/opentelemetry/internal
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(Project project, MonitoredResourceFromDataFn resource_fn,
                       ResourceFilterDataFn filter_fn, Options options) {
  auto connection = monitoring_v3::MakeMetricServiceConnection();
  options = otel::DefaultOptions(std::move(options));
  return std::make_unique<otel::MonitoringExporter>(
      std::move(project), std::move(connection), std::move(resource_fn),
      std::move(filter_fn), std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
