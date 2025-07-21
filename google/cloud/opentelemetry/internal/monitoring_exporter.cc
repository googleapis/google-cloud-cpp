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

#include "google/cloud/opentelemetry/internal/monitoring_exporter.h"
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/opentelemetry/internal/time_series.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include <memory>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::string FormatProjectFullName(std::string const& project) {
  return absl::StrCat("projects/", project);
}

}  // namespace

MonitoringExporter::MonitoringExporter(
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    otel_internal::MonitoredResourceFromDataFn dynamic_resource_fn,
    otel_internal::ResourceFilterDataFn resource_filter_fn,
    Options const& options)
    : client_(std::move(conn)),
      formatter_(options.get<otel::MetricNameFormatterOption>()),
      use_service_time_series_(options.get<otel::ServiceTimeSeriesOption>()),
      mr_proto_(internal::FetchOption<otel::MonitoredResourceOption>(options)),
      dynamic_resource_fn_(std::move(dynamic_resource_fn)),
      resource_filter_fn_(std::move(resource_filter_fn)) {}

MonitoringExporter::MonitoringExporter(
    Project project,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options const& options)
    : MonitoringExporter(std::move(conn), nullptr, nullptr, options) {
  project_ = std::move(project);
}

opentelemetry::sdk::common::ExportResult MonitoringExporter::Export(
    opentelemetry::sdk::metrics::ResourceMetrics const& data) noexcept {
  auto result =
      internal::NoExceptAction<opentelemetry::sdk::common::ExportResult>(
          [&] { return ExportImpl(data); });
  if (result) return *std::move(result);
  GCP_LOG(WARNING) << "Exception thrown while exporting metrics.";
  return opentelemetry::sdk::common::ExportResult::kFailure;
}

opentelemetry::sdk::common::ExportResult MonitoringExporter::SendRequests(
    std::vector<google::monitoring::v3::CreateTimeSeriesRequest> const&
        requests) {
  auto result = opentelemetry::sdk::common::ExportResult::kSuccess;
  for (auto const& request : requests) {
    auto status = use_service_time_series_
                      ? client_.CreateServiceTimeSeries(request)
                      : client_.CreateTimeSeries(request);
    if (status.ok()) continue;
    GCP_LOG(WARNING) << "Cloud Monitoring Export failed with status=" << status;
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

opentelemetry::sdk::common::ExportResult MonitoringExporter::ExportImpl(
    opentelemetry::sdk::metrics::ResourceMetrics const& data) {
  opentelemetry::sdk::common::ExportResult result =
      opentelemetry::sdk::common::ExportResult::kSuccess;
  if (otel_internal::IsEmptyTimeSeries(data)) {
    GCP_LOG(INFO) << "Cloud Monitoring Export skipped. No data.";
    // Return early to save the littlest bit of processing.
    return result;
  }

  std::vector<google::monitoring::v3::CreateTimeSeriesRequest> requests;
  if (dynamic_resource_fn_) {
    auto tss_map = otel_internal::ToTimeSeriesWithResources(
        data, formatter_, resource_filter_fn_, dynamic_resource_fn_);
    for (auto& tss : tss_map) {
      requests = otel_internal::ToRequests(FormatProjectFullName(tss.first),
                                           std::move(tss.second));
      result = SendRequests(requests);
    }
  } else {
    auto tss = otel_internal::ToTimeSeries(data, formatter_);
    auto mr = otel_internal::ToMonitoredResource(data, mr_proto_);
    requests =
        otel_internal::ToRequests(project_->FullName(), mr, std::move(tss));
    result = SendRequests(requests);
  }
  return result;
}

Options DefaultOptions(Options o) {
  if (!o.has<otel::MetricNameFormatterOption>()) {
    o.set<otel::MetricNameFormatterOption>([](std::string s) {
      return "workload.googleapis.com/" + std::move(s);
    });
  }
  return o;
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(MonitoredResourceFromDataFn dynamic_resource_fn,
                       ResourceFilterDataFn resource_filter_fn,
                       Options options) {
  auto connection = monitoring_v3::MakeMetricServiceConnection(options);
  options = DefaultOptions(std::move(options));
  return std::make_unique<otel_internal::MonitoringExporter>(
      std::move(connection), std::move(dynamic_resource_fn),
      std::move(resource_filter_fn), std::move(options));
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    MonitoredResourceFromDataFn dynamic_resource_fn,
    ResourceFilterDataFn resource_filter_fn,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options) {
  options = DefaultOptions(std::move(options));
  return std::make_unique<otel_internal::MonitoringExporter>(
      std::move(conn), std::move(dynamic_resource_fn),
      std::move(resource_filter_fn), std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
