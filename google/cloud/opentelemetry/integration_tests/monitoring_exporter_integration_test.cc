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
#include <gmock/gmock.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/aggregation/default_aggregation.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
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

auto constexpr kSchemaVersion = "1.2.0";
auto constexpr kSchema = "https://opentelemetry.io/schemas/1.2.0";
auto constexpr kMeterName =
    "gl-cpp/testing/monitoring_exporter_integration_test";

std::string RandomId() {
  auto generator = internal::MakeDefaultPRNG();
  return internal::Sample(generator, 8, "0123456789");
}

// If this code is unfamiliar, see the "simple" metrics example:
// https://github.com/open-telemetry/opentelemetry-cpp/tree/2d077f8ec5315e0979a236554c81f621eb61f5b3/examples/metrics_simple
void InstallExporter(
    std::unique_ptr<metrics_sdk::PushMetricExporter> exporter) {
  // GCM requires that metrics be tied to a Monitored Resource. Instead of using
  // a GCP Resource Detector, which requires that the integration test be run
  // from GCP, we set attributes that we know will map to a `generic_node`.
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{sc::kServiceNamespace, "gl-cpp"},
       // Host ID is meant to uniquely identify a VM. We don't care, though.
       {sc::kHostId, "monitoring_exporter_integration_test"}},
      kSchemaVersion);

  // Initialize and set the global MeterProvider
  metrics_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis = std::chrono::milliseconds(5000);
  options.export_timeout_millis = std::chrono::milliseconds(500);

  auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
      std::move(exporter), options);

  auto u_provider = metrics_sdk::MeterProviderFactory::Create(
      std::make_unique<opentelemetry::sdk::metrics::ViewRegistry>(), resource);
  auto* p = static_cast<metrics_sdk::MeterProvider*>(u_provider.get());

  p->AddMetricReader(std::move(reader));

  // counter view
  std::string unit = "1";

  auto instrument_selector = metrics_sdk::InstrumentSelectorFactory::Create(
      metrics_sdk::InstrumentType::kCounter, kMeterName, unit);

  auto meter_selector = metrics_sdk::MeterSelectorFactory::Create(
      kMeterName, kSchemaVersion, kSchema);

  auto sum_view = metrics_sdk::ViewFactory::Create(
      kMeterName, "description", unit, metrics_sdk::AggregationType::kSum);

  p->AddView(std::move(instrument_selector), std::move(meter_selector),
             std::move(sum_view));

  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider(
      std::move(u_provider));
  metrics_api::Provider::SetMeterProvider(provider);
}

void DoWork(std::string const& run_id) {
  auto provider = metrics_api::Provider::GetMeterProvider();
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter =
      provider->GetMeter(kMeterName, "1.2.0");
  auto double_counter = meter->CreateDoubleCounter(kMeterName);

  // This takes 10s to run. That is unfortunate, but necessary because GCM has a
  // minimum update period of 5s.
  for (uint32_t i = 0; i < 20; ++i) {
    double_counter->Add(i, {{"run_id", run_id}});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

TEST(MonitoringExporter, Basic) {
  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));

  auto project = Project(project_id);
  auto conn = monitoring_v3::MakeMetricServiceConnection();
  auto client = monitoring_v3::MetricServiceClient(conn);
  auto exporter =
      otel_internal::MakeMonitoringExporter(project, std::move(conn));
  InstallExporter(std::move(exporter));

  // Uniquely identify the telemetry produced by this run of the test.
  auto const run_id = RandomId();
  // Perform work which creates telemetry. An export should happen.
  DoWork(run_id);

  // Verify that the metrics were exported to GCM, by retrieving TimeSeries.
  auto const now = std::chrono::system_clock::now();
  auto const then = now - std::chrono::minutes(10);
  google::monitoring::v3::TimeInterval interval;
  *interval.mutable_end_time() = internal::ToProtoTimestamp(now);
  *interval.mutable_start_time() = internal::ToProtoTimestamp(then);
  auto filter = absl::StrFormat(R"(metric.type = "workload.googleapis.com/%s"
      AND metric.labels.run_id = "%s")",
                                kMeterName, run_id);
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
