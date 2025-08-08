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
#include "google/cloud/monitoring/v3/mocks/mock_metric_connection.h"
#include "google/cloud/opentelemetry/internal/time_series.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/resource/resource.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Each;
using ::testing::Eq;
using ::testing::ResultOf;
using ::testing::SizeIs;
using ::testing::StartsWith;

auto MetricTypePrefix(std::string const& type) {
  return ResultOf(
      "metric.type",
      [](google::monitoring::v3::TimeSeries const& ts) {
        return ts.metric().type();
      },
      StartsWith(type));
}

auto RequestProject(std::string const& project) {
  return ResultOf(
      "resource.project",
      [](google::monitoring::v3::CreateTimeSeriesRequest const& request) {
        return request.name();
      },
      Eq(project));
}

auto ResourceLabelsAre(
    std::unordered_map<std::string, std::string> const& resource_labels) {
  return ResultOf(
      "resource.labels",
      [](google::monitoring::v3::TimeSeries const& ts) {
        auto const& labels = ts.resource().labels();
        return std::unordered_map<std::string, std::string>(labels.begin(),
                                                            labels.end());
      },
      Eq(resource_labels));
}

opentelemetry::sdk::metrics::ResourceMetrics MakeResourceMetrics(
    std::vector<std::unordered_map<std::string, std::string>> const&
        resource_labels) {
  opentelemetry::sdk::metrics::ScopeMetrics sm;
  for (auto const& r : resource_labels) {
    opentelemetry::sdk::metrics::PointDataAttributes pda;
    pda.point_data = opentelemetry::sdk::metrics::SumPointData{};
    for (auto const& l : r) {
      pda.attributes.SetAttribute(l.first, l.second);
    }

    opentelemetry::sdk::metrics::MetricData md;
    md.instrument_descriptor.type_ = {};
    md.instrument_descriptor.value_type_ = {};
    md.point_data_attr_.push_back(pda);

    sm.metric_data_.push_back(md);
  }

  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.resource_ = nullptr;
  rm.scope_metric_data_.push_back(std::move(sm));

  return rm;
}

TEST(MonitoringExporter, ExportSuccess) {
  std::unordered_map<std::string, std::string> resource_labels_1{
      {"project_id", "my-project-1"},
      {"instance", "my-instance-1"},
      {"cluster", "my-cluster-1"},
      {"table", "my-table-1"},
      {"zone", "my-zone-1"}};

  std::unordered_map<std::string, std::string> resource_labels_2{
      {"project_id", "my-project-2"},
      {"instance", "my-instance-2"},
      {"cluster", "my-cluster-2"},
      {"table", "my-table-2"},
      {"zone", "my-zone-2"}};

  auto mock =
      std::make_shared<monitoring_v3_mocks::MockMetricServiceConnection>();
  EXPECT_CALL(*mock, CreateTimeSeries(RequestProject("projects/my-project-1")))
      .WillOnce(
          [&](google::monitoring::v3::CreateTimeSeriesRequest const& request) {
            EXPECT_THAT(request.time_series(), SizeIs(1));
            EXPECT_THAT(request.time_series(),
                        Each(MetricTypePrefix("workload.googleapis.com/")));
            EXPECT_THAT(request.time_series(0),
                        ResourceLabelsAre(resource_labels_1));
            return Status();
          });
  EXPECT_CALL(*mock, CreateTimeSeries(RequestProject("projects/my-project-2")))
      .WillOnce(
          [&](google::monitoring::v3::CreateTimeSeriesRequest const& request) {
            EXPECT_THAT(request.time_series(), SizeIs(1));
            EXPECT_THAT(request.time_series(),
                        Each(MetricTypePrefix("workload.googleapis.com/")));
            EXPECT_THAT(request.time_series(0),
                        ResourceLabelsAre(resource_labels_2));
            return Status();
          });

  auto resource_fn =
      [&](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        google::api::MonitoredResource resource;
        resource.set_type("bigtable_client_raw");
        auto& labels = *resource.mutable_labels();
        auto const& attributes = pda.attributes.GetAttributes();
        labels["project_id"] =
            absl::get<std::string>(attributes.find("project_id")->second);
        labels["instance"] =
            absl::get<std::string>(attributes.find("instance")->second);
        labels["cluster"] =
            absl::get<std::string>(attributes.find("cluster")->second);
        labels["table"] =
            absl::get<std::string>(attributes.find("table")->second);
        labels["zone"] =
            absl::get<std::string>(attributes.find("zone")->second);
        return std::make_pair(labels["project_id"], resource);
      };

  auto filter_fn = [resources = std::set<std::string>{
                        "project_id", "instance", "cluster", "table",
                        "zone"}](std::string const& l) {
    return internal::Contains(resources, l);
  };

  auto exporter =
      MakeMonitoringExporter(resource_fn, filter_fn, std::move(mock), {});
  auto data = MakeResourceMetrics({resource_labels_1, resource_labels_2});
  auto result = exporter->Export(data);
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kSuccess);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
