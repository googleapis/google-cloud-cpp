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

#include "google/cloud/bigtable/internal/operation_context_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/algorithm.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <google/api/monitored_resource.pb.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

ResourceLabels ResourceLabelsFromTableName(std::string const& table_name) {
  // parse table_name into component pieces
  // projects/<project>/instances/<instance>/tables/<table>
  std::vector<absl::string_view> name_parts = absl::StrSplit(table_name, '/');
  ResourceLabels resource_labels = {
      std::string(name_parts[1]), std::string(name_parts[3]),
      std::string(name_parts[5]), "" /*=cluster*/, "" /*=zone*/};
  return resource_labels;
}
}  // namespace

OperationContextFactory::~OperationContextFactory() = default;

opentelemetry::sdk::common::OrderedAttributeMap GrabMap(
    opentelemetry::sdk::metrics::ResourceMetrics const& data) {
  opentelemetry::sdk::common::OrderedAttributeMap attr_map;
  for (auto const& scope_metric : data.scope_metric_data_) {
    for (auto const& metric : scope_metric.metric_data_) {
      for (auto const& point : metric.point_data_attr_) {
        std::cout << __func__ << " data.scope_metric_data_.size()="
                  << data.scope_metric_data_.size()
                  << "; scope_metric.metric_data_.size()="
                  << scope_metric.metric_data_.size()
                  << "; metric.point_data_attr_.size()="
                  << metric.point_data_attr_.size() << std::endl;
        return point.attributes;
      }
    }
  }
  return attr_map;
}

MetricsOperationContextFactory::MetricsOperationContextFactory(
    Project project, std::string client_uid)
    : client_uid_(std::move(client_uid)) {
  std::cout << __func__ << std::endl;
  auto resource_fn =
      [](opentelemetry::sdk::metrics::ResourceMetrics const& data) {
        google::api::MonitoredResource resource;
        std::cout << __func__ << ": build MonitoredResource from attr_map"
                  << std::endl;
        auto attr_map = GrabMap(data);
        for (auto const& p : attr_map) {
          std::cout << p.first << ": "
                    << (absl::holds_alternative<std::string>(p.second)
                            ? absl::get<std::string>(p.second)
                            : "not a string")
                    << std::endl;
        }

        resource.set_type("bigtable_client_raw");
        auto& labels = *resource.mutable_labels();
        labels["project_id"] =
            absl::get<std::string>(attr_map.find("project_id")->second);
        labels["instance"] =
            absl::get<std::string>(attr_map.find("instance")->second);
        labels["cluster"] =
            absl::get<std::string>(attr_map.find("cluster")->second);
        labels["table"] =
            absl::get<std::string>(attr_map.find("table")->second);
        labels["zone"] = absl::get<std::string>(attr_map.find("zone")->second);

        return resource;
      };

  auto filter_fn = [resource_keys = std::set<std::string>{
                        "project_id", "instance", "cluster", "table",
                        "zone"}](std::string const& key) {
    return internal::Contains(resource_keys, key);
  };

  auto o = Options{}
               .set<LoggingComponentsOption>({"rpc"})
               .set<otel::ServiceTimeSeriesOption>(true)
               .set<otel::MetricNameFormatterOption>([](auto name) {
                 return "bigtable.googleapis.com/internal/client/" + name;
               });
  auto exporter = otel_internal::MakeMonitoringExporter(
      project, resource_fn, filter_fn, std::move(o));
  auto options =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
  // Empirically, it seems like 30s is the minimum.
  options.export_interval_millis = std::chrono::seconds(30);
  options.export_timeout_millis = std::chrono::seconds(1);

  auto reader =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
          std::move(exporter), options);

  auto context = opentelemetry::sdk::metrics::MeterContextFactory::Create(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
      // NOTE : this skips OTel's built in resource detection which is more
      // confusing than helpful. (The default is {{"service_name",
      // "unknown_service" }}). And after #14930, this gets copied into our
      // resource labels. oh god why.
      opentelemetry::sdk::resource::Resource::GetEmpty());
  context->AddMetricReader(std::move(reader));
  std::cout << __func__
            << ": opentelemetry::sdk::metrics::MeterProviderFactory::Create"
            << std::endl;
  provider_ = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::move(context));

  auto meter = provider_->GetMeter("bigtable", "");
}

// ReadRow is a synthetic RPC and should appear in metrics as if it's a
// different RPC than ReadRows with row_limit=1.
std::shared_ptr<OperationContext> MetricsOperationContextFactory::ReadRow() {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::ReadRows() {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext>
MetricsOperationContextFactory::AsyncReadRows() {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::MutateRow(
    std::string const& table_name, std::string const& app_profile_id) {
  static bool const kMetricsInitialized = [this, &table_name,
                                           &app_profile_id]() {
    std::vector<std::shared_ptr<Metric>> v;
    auto resource_labels = ResourceLabelsFromTableName(table_name);
    DataLabels data_labels = {"MutateRow",
                              "false",  // streaming
                              "cpp.Bigtable/" + version_string(),
                              client_uid_,
                              app_profile_id,
                              "" /*=status*/};
    v.push_back(std::make_shared<AttemptLatency>(resource_labels, data_labels,
                                                 "bigtable", "", provider_));
    v.push_back(std::make_shared<OperationLatency>(resource_labels, data_labels,
                                                   "bigtable", "", provider_));
    std::lock_guard<std::mutex> lock(mu_);
    if (mutate_row_metrics_.empty()) swap(mutate_row_metrics_, v);
    return true;
  }();

  // this creates a copy, we may not want to make a copy
  if (kMetricsInitialized) {
    return std::make_shared<OperationContext>(mutate_row_metrics_);
  }
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::AsyncMutateRow(
    std::string const& table_name,
    std::string const& app_profile_id) {  // not currently used
  static bool const kMetricsInitialized = [this, &table_name,
                                           &app_profile_id]() {
    std::vector<std::shared_ptr<Metric>> v;
    auto resource_labels = ResourceLabelsFromTableName(table_name);
    DataLabels data_labels = {"MutateRow",
                              "false",  // streaming
                              "cpp.Bigtable/" + version_string(),
                              client_uid_,
                              app_profile_id,
                              "" /*=status*/};
    v.push_back(std::make_shared<AttemptLatency>(resource_labels, data_labels,
                                                 "bigtable", "", provider_));
    v.push_back(std::make_shared<OperationLatency>(resource_labels, data_labels,
                                                   "bigtable", "", provider_));
    std::lock_guard<std::mutex> lock(mu_);
    if (mutate_row_metrics_.empty()) swap(mutate_row_metrics_, v);
    return true;
  }();

  // this creates a copy, we may not want to make a copy
  if (kMetricsInitialized) {
    return std::make_shared<OperationContext>(mutate_row_metrics_);
  }
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext> MetricsOperationContextFactory::MutateRows(
    std::string const& table_name, std::string const& app_profile_id) {
  static bool const kMetricsInitialized = [this, &table_name,
                                           &app_profile_id]() {
    std::vector<std::shared_ptr<Metric>> v;
    auto resource_labels = ResourceLabelsFromTableName(table_name);
    DataLabels data_labels = {"MutateRows",
                              "false",  // streaming
                              "cpp.Bigtable/" + version_string(),
                              client_uid_,
                              app_profile_id,
                              "" /*=status*/};
    v.push_back(std::make_shared<AttemptLatency>(resource_labels, data_labels,
                                                 "bigtable", "", provider_));
    v.push_back(std::make_shared<OperationLatency>(resource_labels, data_labels,
                                                   "bigtable", "", provider_));
    std::lock_guard<std::mutex> lock(mu_);
    if (mutate_rows_metrics_.empty()) swap(mutate_rows_metrics_, v);
    return true;
  }();

  // this creates a copy, we may not want to make a copy
  if (kMetricsInitialized) {
    return std::make_shared<OperationContext>(mutate_rows_metrics_);
  }
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::AsyncMutateRows(
    std::string const& table_name, std::string const& app_profile_id) {
  static bool const kMetricsInitialized = [this, &table_name,
                                           &app_profile_id]() {
    std::vector<std::shared_ptr<Metric>> v;
    auto resource_labels = ResourceLabelsFromTableName(table_name);
    DataLabels data_labels = {"MutateRows",
                              "false",  // streaming
                              "cpp.Bigtable/" + version_string(),
                              client_uid_,
                              app_profile_id,
                              "" /*=status*/};
    v.push_back(std::make_shared<AttemptLatency>(resource_labels, data_labels,
                                                 "bigtable", "", provider_));
    v.push_back(std::make_shared<OperationLatency>(resource_labels, data_labels,
                                                   "bigtable", "", provider_));
    std::lock_guard<std::mutex> lock(mu_);
    if (mutate_rows_metrics_.empty()) swap(mutate_rows_metrics_, v);
    return true;
  }();

  // this creates a copy, we may not want to make a copy
  if (kMetricsInitialized) {
    return std::make_shared<OperationContext>(mutate_rows_metrics_);
  }
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::CheckandMutateRow() {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext>
MetricsOperationContextFactory::AsyncCheckandMutateRow() {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::SampleRowKeys() {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext>
MetricsOperationContextFactory::AsyncSampleRowKeys() {
  return std::make_shared<OperationContext>();
}

std::shared_ptr<OperationContext>
MetricsOperationContextFactory::ReadModifyWriteRow() {
  return std::make_shared<OperationContext>();
}
std::shared_ptr<OperationContext>
MetricsOperationContextFactory::AsyncReadModifyWriteRow() {
  return std::make_shared<OperationContext>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
