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

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/internal/grpc/metrics_exporter_impl.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;

auto EmptyResource() {
  return opentelemetry::sdk::resource::Resource::Create({});
}

auto FullResource() {
  return opentelemetry::sdk::resource::Resource::Create(
      {{"cloud.provider", "gcp"}, {"cloud.account.id", "project-id-resource"}});
}

auto TestOptions(Options opts = {}) {
  // In CI builds this environment variable may be set and affect the behavior
  // in ways we do not need to (re)test here.
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  opts.set<storage_experimental::EnableGrpcMetricsOption>(true)
      .set<UnifiedCredentialsOption>(MakeAccessTokenCredentials(
          "unused",
          std::chrono::system_clock::now() + std::chrono::minutes(15)));
  return DefaultOptionsGrpc(std::move(opts));
}

TEST(GrpcMetricsExporter, DisabledBecauseOption) {
  auto config = MakeMeterProviderConfig(
      FullResource(),
      TestOptions().set<storage_experimental::EnableGrpcMetricsOption>(false));
  EXPECT_FALSE(config.has_value());
}

TEST(GrpcMetricsExporter, DisabledBecauseNoProject) {
  auto config = MakeMeterProviderConfig(EmptyResource(), TestOptions());
  EXPECT_FALSE(config.has_value());
}

TEST(GrpcMetricsExporter, EnabledResourceProject) {
  auto config = MakeMeterProviderConfig(FullResource(), TestOptions());
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project("project-id-resource"));
  EXPECT_TRUE(config->exporter_options.has<otel::MonitoredResourceOption>());
  EXPECT_TRUE(config->exporter_options.has<otel::MetricNameFormatterOption>());
  EXPECT_TRUE(config->exporter_options.get<otel::ServiceTimeSeriesOption>());
  EXPECT_GT(config->reader_options.export_interval_millis,
            std::chrono::milliseconds(0));
  EXPECT_GT(config->reader_options.export_timeout_millis,
            std::chrono::milliseconds(0));
}

TEST(GrpcMetricsExporter, EnabledOptionsProject) {
  auto config = MakeMeterProviderConfig(
      EmptyResource(),
      TestOptions().set<storage::ProjectIdOption>("project-id-option"));
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project("project-id-option"));
  EXPECT_TRUE(config->exporter_options.has<otel::MonitoredResourceOption>());
  EXPECT_TRUE(config->exporter_options.has<otel::MetricNameFormatterOption>());
  EXPECT_TRUE(config->exporter_options.get<otel::ServiceTimeSeriesOption>());
  EXPECT_GT(config->reader_options.export_interval_millis,
            std::chrono::milliseconds(0));
  EXPECT_GT(config->reader_options.export_timeout_millis,
            std::chrono::milliseconds(0));
}

TEST(GrpcMetricsExporter, EnabledWithTimeout) {
  auto config = MakeMeterProviderConfig(
      FullResource(),
      TestOptions().set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(45)));
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project("project-id-resource"));
  EXPECT_TRUE(config->exporter_options.has<otel::MonitoredResourceOption>());
  EXPECT_TRUE(config->exporter_options.has<otel::MetricNameFormatterOption>());
  EXPECT_TRUE(config->exporter_options.get<otel::ServiceTimeSeriesOption>());
  EXPECT_EQ(config->reader_options.export_interval_millis,
            std::chrono::seconds(45));
  EXPECT_GT(config->reader_options.export_timeout_millis,
            std::chrono::milliseconds(0));
}

TEST(GrpcMetricsExporter, DefaultExportTime) {
  auto config = MakeMeterProviderConfig(FullResource(), TestOptions());
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project("project-id-resource"));
  EXPECT_EQ(config->reader_options.export_interval_millis,
            std::chrono::seconds(60));
  EXPECT_EQ(config->reader_options.export_timeout_millis,
            std::chrono::seconds(30));
}

TEST(GrpcMetricsExporter, CustomExportTime) {
  auto config = MakeMeterProviderConfig(
      FullResource(),
      TestOptions()
          .set<storage_experimental::GrpcMetricsPeriodOption>(
              std::chrono::seconds(10))
          .set<storage_experimental::GrpcMetricsExportTimeoutOption>(
              std::chrono::seconds(2)));
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project("project-id-resource"));
  EXPECT_EQ(config->reader_options.export_interval_millis,
            std::chrono::seconds(10));
  EXPECT_EQ(config->reader_options.export_timeout_millis,
            std::chrono::seconds(2));
}

TEST(GrpcMetricsExporter, ReaderOptionsAreSetFromConfig) {
  auto const expected_interval = std::chrono::seconds(45);
  auto const expected_timeout = std::chrono::seconds(25);

  auto config = MakeMeterProviderConfig(
      FullResource(),
      TestOptions()
          .set<storage_experimental::GrpcMetricsPeriodOption>(expected_interval)
          .set<storage_experimental::GrpcMetricsExportTimeoutOption>(
              expected_timeout));

  ASSERT_TRUE(config.has_value());

  // Verify the conversion from seconds to milliseconds happens correctly.
  EXPECT_EQ(config->reader_options.export_interval_millis,
            std::chrono::milliseconds(expected_interval));
  EXPECT_EQ(config->reader_options.export_timeout_millis,
            std::chrono::milliseconds(expected_timeout));
}

TEST(GrpcMetricsExporter, MakeFilterWithServiceName) {
  auto excluded_labels = std::set<std::string>{"service_name"};
  auto options =
      TestOptions().set<storage_experimental::GrpcMetricsExcludedLabelsOption>(
          excluded_labels);

  auto config = MakeMeterProviderConfig(FullResource(), options);
  ASSERT_TRUE(config.has_value());

  // Test that filter option is set.
  ASSERT_TRUE(config->exporter_options.has<otel::ResourceFilterDataFnOption>());
  auto filter_fn =
      config->exporter_options.get<otel::ResourceFilterDataFnOption>();
  ASSERT_NE(filter_fn, nullptr);

  // Test filtering behavior.
  EXPECT_TRUE(filter_fn("service_name"));
  EXPECT_FALSE(filter_fn("service_version"));
}

TEST(GrpcMetricsExporter, MakeFilterWithExplicitFilterFunction) {
  auto explicit_filter = [](std::string const& key) {
    return key == "explicit_test";
  };
  auto excluded_labels = std::set<std::string>{"service_name"};

  // Test when explicit filter function takes precedence.
  auto options =
      TestOptions()
          .set<otel::ResourceFilterDataFnOption>(explicit_filter)
          .set<storage_experimental::GrpcMetricsExcludedLabelsOption>(
              excluded_labels);
  auto config = MakeMeterProviderConfig(FullResource(), options);
  ASSERT_TRUE(config.has_value());

  auto filter_fn =
      config->exporter_options.get<otel::ResourceFilterDataFnOption>();
  ASSERT_NE(filter_fn, nullptr);

  // Explicit filter should take precedence
  EXPECT_TRUE(filter_fn("explicit_test"));
  EXPECT_FALSE(filter_fn("service_name"));
}

TEST(GrpcMetricsExporter, MakeFilterWithEmptyExcludedLabels) {
  auto excluded_labels = std::set<std::string>{};

  // Test when empty excluded labels returns nullptr.
  auto options =
      TestOptions().set<storage_experimental::GrpcMetricsExcludedLabelsOption>(
          excluded_labels);
  auto config = MakeMeterProviderConfig(FullResource(), options);
  ASSERT_TRUE(config.has_value());

  auto filter_fn =
      config->exporter_options.get<otel::ResourceFilterDataFnOption>();
  EXPECT_EQ(filter_fn, nullptr);
}

TEST(GrpcMetricsExporter, MakeFilterWithNoExcludedLabelsOption) {
  auto options = TestOptions();

  // Test when no excluded labels option returns nullptr.
  auto config = MakeMeterProviderConfig(FullResource(), options);
  ASSERT_TRUE(config.has_value());

  auto filter_fn =
      config->exporter_options.get<otel::ResourceFilterDataFnOption>();
  EXPECT_EQ(filter_fn, nullptr);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
