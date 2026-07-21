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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS

#include "google/cloud/bigtable/internal/grpc_metrics_exporter.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/data/point_data.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/semconv/incubating/cloud_attributes.h>
#include <opentelemetry/semconv/incubating/host_attributes.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Not;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::VariantWith;

class MockPushMetricExporter
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  // NOLINTBEGIN(bugprone-exception-escape)
  MOCK_METHOD(opentelemetry::sdk::common::ExportResult, Export,
              (opentelemetry::sdk::metrics::ResourceMetrics const&),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::sdk::metrics::AggregationTemporality,
              GetAggregationTemporality,
              (opentelemetry::sdk::metrics::InstrumentType),
              (const, noexcept, override));

  MOCK_METHOD(bool, ForceFlush, (std::chrono::microseconds),
              (noexcept, override));
  MOCK_METHOD(bool, Shutdown, (std::chrono::microseconds),
              (noexcept, override));
  // NOLINTEND(bugprone-exception-escape)
};

auto constexpr kDurationMetric = "grpc.client.attempt.duration";

auto MatchesLatencyBoundaries() {
  return ResultOf(
      "boundaries are latency boundaries",
      [](opentelemetry::sdk::metrics::HistogramPointData const& hpd) {
        return hpd.boundaries_;
      },
      ElementsAreArray(MakeLatencyHistogramBoundaries()));
}

auto ExpectedLatencyHistogram() {
  return ResultOf(
      "data is histogram with right boundaries",
      [](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        return pda.point_data;
      },
      VariantWith<opentelemetry::sdk::metrics::HistogramPointData>(
          MatchesLatencyBoundaries()));
}

template <typename Matcher>
auto WithPointData(Matcher&& matcher) {
  return ResultOf(
      "instrument descriptor name",
      [](opentelemetry::sdk::metrics::MetricData const& md) {
        return md.point_data_attr_;
      },
      std::forward<Matcher>(matcher));
}

template <typename Matcher>
auto WithMetricsData(Matcher&& matcher) {
  return ResultOf(
      "metric_data_",
      [](opentelemetry::sdk::metrics::ScopeMetrics const& sm) {
        return sm.metric_data_;
      },
      std::forward<Matcher>(matcher));
}

auto MatchesInstrumentName(std::string name) {
  return ResultOf(
      "instrument descriptor name",
      [](opentelemetry::sdk::metrics::MetricData const& md) {
        return md.instrument_descriptor.name_;
      },
      std::move(name));
}

auto MetricDataEmpty() {
  return ResultOf(
      "scope_metric_data_",
      [](opentelemetry::sdk::metrics::ResourceMetrics const& data) {
        return data.scope_metric_data_;
      },
      IsEmpty());
}

auto MetricDataNotEmpty() {
  return ResultOf(
      "scope_metric_data_",
      [](opentelemetry::sdk::metrics::ResourceMetrics const& data) {
        return data.scope_metric_data_;
      },
      Not(IsEmpty()));
}

auto constexpr kExportInterval = std::chrono::milliseconds(50);
auto constexpr kExportTimeout = std::chrono::milliseconds(25);

auto TestReaderOptions() {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      reader_options;
  reader_options.export_interval_millis = kExportInterval;
  reader_options.export_timeout_millis = kExportTimeout;
  return reader_options;
}

TEST(GrpcMetricsExporterRegistryTest, SingletonAndClear) {
  auto& registry = GrpcMetricsExporterRegistry::Singleton();
  registry.Clear();

  EXPECT_TRUE(registry.Register("test-authority-1"));
  EXPECT_FALSE(registry.Register("test-authority-1"));
  EXPECT_TRUE(registry.Register("test-authority-2"));

  registry.Clear();
  EXPECT_TRUE(registry.Register("test-authority-1"));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResource) {
  Options options;
  options.set<bigtable::AppProfileIdOption>("test-app-profile");
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({
      {"project_id", "test-project"},
      {"instance", "test-instance"},
  });

  auto result = MakeMonitoredResource(pda, options, client_uid);
  EXPECT_THAT(result.project_id, Eq("test-project"));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.at("project_id"), Eq("test-project"));
  EXPECT_THAT(labels.at("client_project"), Eq("test-project"));
  EXPECT_THAT(labels.at("instance"), Eq("test-instance"));
  EXPECT_THAT(labels.at("app_profile"), Eq("test-app-profile"));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
  EXPECT_THAT(labels.at("location"), Eq("global"));
  EXPECT_THAT(labels.at("cloud_platform"), Eq("unknown"));
  EXPECT_THAT(labels.at("host_id"), Eq("unknown"));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResourceMissingAttributes) {
  Options options;
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({});

  auto result = MakeMonitoredResource(pda, options, client_uid);
  EXPECT_THAT(result.project_id, Eq(""));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.at("project_id"), Eq(""));
  EXPECT_THAT(labels.at("instance"), Eq(""));
  EXPECT_THAT(labels.at("app_profile"), Eq(""));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
  EXPECT_THAT(labels.at("location"), Eq("global"));
  EXPECT_THAT(labels.at("cloud_platform"), Eq("unknown"));
  EXPECT_THAT(labels.at("host_id"), Eq("unknown"));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResourcePartialAttributes) {
  Options options;
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({
      {"project_id", "test-project"},
  });

  auto result = MakeMonitoredResource(pda, options, client_uid);
  EXPECT_THAT(result.project_id, Eq("test-project"));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.at("project_id"), Eq("test-project"));
  EXPECT_THAT(labels.at("client_project"), Eq("test-project"));
  EXPECT_THAT(labels.at("instance"), Eq(""));
  EXPECT_THAT(labels.at("app_profile"), Eq(""));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
  EXPECT_THAT(labels.at("location"), Eq("global"));
  EXPECT_THAT(labels.at("cloud_platform"), Eq("unknown"));
  EXPECT_THAT(labels.at("host_id"), Eq("unknown"));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResourceExtraAttributes) {
  Options options;
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({
      {"project_id", "test-project"},
      {"instance", "test-instance"},
      {"grpc.method", "google.bigtable.v2.Bigtable/ReadRows"},
      {"grpc.status", "OK"},
  });

  auto result = MakeMonitoredResource(pda, options, client_uid);
  EXPECT_THAT(result.project_id, Eq("test-project"));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.size(), Eq(9));
  EXPECT_THAT(labels.at("project_id"), Eq("test-project"));
  EXPECT_THAT(labels.at("client_project"), Eq("test-project"));
  EXPECT_THAT(labels.at("instance"), Eq("test-instance"));
  EXPECT_THAT(labels.at("app_profile"), Eq(""));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
  EXPECT_THAT(labels.at("location"), Eq("global"));
  EXPECT_THAT(labels.at("cloud_platform"), Eq("unknown"));
  EXPECT_THAT(labels.at("host_id"), Eq("unknown"));
  EXPECT_THAT(labels.find("grpc.method"), Eq(labels.end()));
  EXPECT_THAT(labels.find("grpc.status"), Eq(labels.end()));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResourceWithDetectedResource) {
  namespace sc = ::opentelemetry::semconv;
  Options options;
  options.set<bigtable::AppProfileIdOption>("test-app-profile");
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({
      {"project_id", "test-project"},
      {"instance", "test-instance"},
  });

  auto detected_resource = opentelemetry::sdk::resource::Resource::Create({
      {sc::cloud::kCloudAccountId, "detected-vm-project"},
      {sc::cloud::kCloudPlatform, "gcp_compute_engine"},
      {sc::cloud::kCloudAvailabilityZone, "us-central1-a"},
      {sc::host::kHostId, "123456789"},
      {sc::host::kHostName, "test-vm-host"},
  });

  auto result =
      MakeMonitoredResource(pda, detected_resource, options, client_uid);
  EXPECT_THAT(result.project_id, Eq("test-project"));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.at("project_id"), Eq("test-project"));
  EXPECT_THAT(labels.at("client_project"), Eq("detected-vm-project"));
  EXPECT_THAT(labels.at("instance"), Eq("test-instance"));
  EXPECT_THAT(labels.at("app_profile"), Eq("test-app-profile"));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
  EXPECT_THAT(labels.at("location"), Eq("us-central1-a"));
  EXPECT_THAT(labels.at("cloud_platform"), Eq("gcp_compute_engine"));
  EXPECT_THAT(labels.at("host_id"), Eq("123456789"));
  EXPECT_THAT(labels.at("hostname"), Eq("test-vm-host"));
}

TEST(GrpcMetricsExporterTest, MakeLatencyHistogramBoundaries) {
  auto const boundaries = MakeLatencyHistogramBoundaries();
  ASSERT_THAT(boundaries, Not(IsEmpty()));
  ASSERT_THAT(boundaries, SizeIs(Le(200U)));
  auto sorted = boundaries;
  std::sort(sorted.begin(), sorted.end());
  EXPECT_THAT(boundaries, ElementsAreArray(sorted.begin(), sorted.end()));
  std::vector<double> diff;
  std::adjacent_difference(boundaries.begin(), boundaries.end(),
                           std::back_inserter(diff));
  ASSERT_THAT(diff, Not(IsEmpty()));
  EXPECT_THAT(*std::min_element(std::next(diff.begin()), diff.end()),
              Ge(0.001));
  ASSERT_THAT(boundaries.back(), Le(300));
}

TEST(GrpcMetricsExporterTest, ValidateGrpcClientAttemptDuration) {
  std::atomic<int> export_count{0};
  auto mock = std::make_unique<MockPushMetricExporter>();
  EXPECT_CALL(*mock, Shutdown).WillOnce(Return(true));
  EXPECT_CALL(*mock, GetAggregationTemporality)
      .WillRepeatedly(Return(
          opentelemetry::sdk::metrics::AggregationTemporality::kCumulative));
  EXPECT_CALL(*mock, Export(MetricDataEmpty()))
      .WillRepeatedly(
          Return(opentelemetry::sdk::common::ExportResult::kSuccess));
  EXPECT_CALL(*mock, Export(MetricDataNotEmpty()))
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&export_count](
              opentelemetry::sdk::metrics::ResourceMetrics const& data) {
            EXPECT_THAT(
                data.scope_metric_data_,
                Contains(WithMetricsData(Contains(AllOf(
                    MatchesInstrumentName(kDurationMetric),
                    WithPointData(ElementsAre(ExpectedLatencyHistogram())))))));
            ++export_count;
            return opentelemetry::sdk::common::ExportResult::kSuccess;
          });

  {
    auto provider = MakeGrpcMeterProvider(std::move(mock), TestReaderOptions());
    auto meter = provider->GetMeter("grpc-c++", grpc::Version());
    auto histogram = meter->CreateDoubleHistogram(kDurationMetric,
                                                  "test-only-description", "s");
    for (int i = 0; i != 50 && export_count.load() == 0; ++i) {
      histogram->Record(1.0, opentelemetry::context::Context{});
      std::this_thread::sleep_for(kExportInterval);
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_GRPC_OTEL_METRICS
