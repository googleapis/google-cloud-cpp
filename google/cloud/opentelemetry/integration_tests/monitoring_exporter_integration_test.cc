// Copyright 2024 Google LLC
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

#include "google/cloud/monitoring/v3/metric_client.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_format.h"
#include <gmock/gmock.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/aggregation/default_aggregation.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/view/view_registry.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;
using ::testing::Not;
namespace metrics_api = ::opentelemetry::metrics;
namespace metrics_sdk = ::opentelemetry::sdk::metrics;
namespace sc = opentelemetry::sdk::resource::SemanticConventions;

auto constexpr kJobName = "monitoring_exporter_integration_test";
auto constexpr kMeterName =
    "gl-cpp/testing/monitoring_exporter_integration_test";

std::string RandomId() {
  auto generator = internal::MakeDefaultPRNG();
  return internal::Sample(generator, 8, "0123456789");
}

// If this code is unfamiliar, see the "simple" metrics example:
// https://github.com/open-telemetry/opentelemetry-cpp/tree/2d077f8ec5315e0979a236554c81f621eb61f5b3/examples/metrics_simple
void InstallExporter(std::unique_ptr<metrics_sdk::PushMetricExporter> exporter,
                     std::string const& task_id) {
  // GCM requires that metrics be tied to a Monitored Resource. We set
  // attributes which will map to a `generic_task`, which seems apt for this
  // workflow.
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{sc::kServiceNamespace, "gl-cpp"},
       {sc::kServiceName, kJobName},
       {sc::kServiceInstanceId, task_id}});

  // Initialize and set the global MeterProvider
  metrics_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis = std::chrono::milliseconds(5000);
  options.export_timeout_millis = std::chrono::milliseconds(500);

  // `PeriodicExportingMetricReaderFactory::Create(...)` was added in
  // opentelemetry-cpp v1.10.0. We support >= 1.9.1.
  std::unique_ptr<metrics_sdk::MetricReader> reader{
      new metrics_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                     options)};

  // `MetricProviderFactory::Create(...)` was added in opentelemetry-cpp
  // v1.10.0. We support >= 1.9.1.
  std::unique_ptr<opentelemetry::metrics::MeterProvider> u_provider(
      new metrics_sdk::MeterProvider(
          std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(),
          resource));
  auto* p = static_cast<metrics_sdk::MeterProvider*>(u_provider.get());

  p->AddMetricReader(std::move(reader));

  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider(
      std::move(u_provider));
  metrics_api::Provider::SetMeterProvider(provider);
}

void DoWork() {
  auto provider = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter =
      provider->GetMeter(kMeterName);
  auto double_counter = meter->CreateDoubleCounter(kMeterName);

  // This takes 10s to run. That is unfortunate, but necessary because GCM has a
  // minimum update period of 5s.
  for (std::uint32_t i = 0; i < 20; ++i) {
    double_counter->Add(i);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

TEST(MonitoringExporter, Basic) {
  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));

  // Uniquely identify the telemetry produced by this run of the test.
  auto const task_id = RandomId();

  // Create and install the GCM exporter.
  auto project = Project(project_id);
  auto conn = monitoring_v3::MakeMetricServiceConnection();
  auto client = monitoring_v3::MetricServiceClient(conn);
  auto exporter =
      otel_internal::MakeMonitoringExporter(project, std::move(conn));
  InstallExporter(std::move(exporter), task_id);

  // Perform work which creates telemetry. An export should happen.
  DoWork();

  // Verify that the metrics were exported to GCM, by retrieving TimeSeries.
  auto const now = std::chrono::system_clock::now();
  auto const then = now - std::chrono::minutes(10);
  google::monitoring::v3::TimeInterval interval;
  *interval.mutable_end_time() = internal::ToProtoTimestamp(now);
  *interval.mutable_start_time() = internal::ToProtoTimestamp(then);
  auto filter = absl::StrFormat(R"(metric.type = "workload.googleapis.com/%s"
      AND resource.labels.job = "%s"
      AND resource.labels.task_id = "%s")",
                                kMeterName, kJobName, task_id);
  auto sr = client.ListTimeSeries(
      project.FullName(), filter, interval,
      google::monitoring::v3::ListTimeSeriesRequest_TimeSeriesView_HEADERS);
  std::vector<google::monitoring::v3::TimeSeries> results;
  for (auto& ts : sr) {
    ASSERT_STATUS_OK(ts);
    results.push_back(*std::move(ts));
  }
  EXPECT_THAT(results, Not(IsEmpty()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
