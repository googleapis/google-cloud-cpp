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
#include "absl/types/variant.h"
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
  explicit MonitoringExporter(
      Project project,
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      Options const& options)
      : project_(std::move(project)),
        client_(std::move(conn)),
        formatter_(options.get<MetricNameFormatterOption>()),
        use_service_time_series_(options.get<ServiceTimeSeriesOption>()),
        mr_proto_(internal::FetchOption<MonitoredResourceOption>(options)),
        mr_factory_(options.has<MonitoredResourceFactoryOption>()
                        ? options.get<MonitoredResourceFactoryOption>()
                        : nullptr) {}

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
  opentelemetry::sdk::common::OrderedAttributeMap GrabMap(
      opentelemetry::sdk::metrics::ResourceMetrics const& data) {
    opentelemetry::sdk::common::OrderedAttributeMap attr_map;
    for (auto const& scope_metric : data.scope_metric_data_) {
      for (auto const& metric : scope_metric.metric_data_) {
        for (auto const& point : metric.point_data_attr_) {
          return point.attributes;
        }
      }
    }
    return attr_map;
  }

  opentelemetry::sdk::common::ExportResult ExportImpl(
      opentelemetry::sdk::metrics::ResourceMetrics const& data) {
    std::cout << __func__ << std::endl;
    auto result = opentelemetry::sdk::common::ExportResult::kSuccess;

    //    auto attr_map = GrabMap(data);
    //    for (auto const& p : attr_map) {
    //      std::cout << p.first << ": "
    //                << (absl::holds_alternative<std::string>(p.second) ?
    //                    absl::get<std::string>(p.second) : "not a string")
    //                << std::endl;
    //    }

    auto tss = otel_internal::ToTimeSeries(data, formatter_);
    if (tss.empty()) {
      GCP_LOG(INFO) << "Cloud Monitoring Export skipped. No data.";
      // Return early to save the littlest bit of processing.
      return result;
    }

    absl::optional<google::api::MonitoredResource> mr_proto = mr_proto_;

    if (mr_factory_) {
      std::cout << __func__ << ": calling lambda" << std::endl;
      std::lock_guard<std::mutex> lock(mu_);
      opentelemetry::sdk::common::AttributeMap attributes =
          data.resource_->GetAttributes();
      mr_proto = mr_factory_(attributes);
    } else {
      std::cout << __func__ << ": NOT calling lambda" << std::endl;
    }

    auto mr = otel_internal::ToMonitoredResource(data, mr_proto);
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

  std::mutex mu_;
  Project project_;
  monitoring_v3::MetricServiceClient client_;
  MetricNameFormatterOption::Type formatter_;
  bool use_service_time_series_;
  absl::optional<google::api::MonitoredResource> mr_proto_;
  std::function<google::api::MonitoredResource(
      opentelemetry::sdk::common::AttributeMap const& attr_map)>
      mr_factory_;
};
}  // namespace

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(Project project, Options options) {
  std::cout << __func__ << std::endl;
  return MakeMonitoringExporter(
      std::move(project), monitoring_v3::MakeMetricServiceConnection(options),
      std::move(options));
}

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
}  // namespace cloud
}  // namespace google
