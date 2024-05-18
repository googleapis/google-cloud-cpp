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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

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
  EXPECT_TRUE(
      config->exporter_options.has<otel_internal::MonitoredResourceOption>());
  EXPECT_TRUE(
      config->exporter_options.has<otel_internal::MetricPrefixOption>());
  EXPECT_TRUE(
      config->exporter_options.get<otel_internal::ServiceTimeSeriesOption>());
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
  EXPECT_TRUE(
      config->exporter_options.has<otel_internal::MonitoredResourceOption>());
  EXPECT_TRUE(
      config->exporter_options.has<otel_internal::MetricPrefixOption>());
  EXPECT_TRUE(
      config->exporter_options.get<otel_internal::ServiceTimeSeriesOption>());
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
  EXPECT_TRUE(
      config->exporter_options.has<otel_internal::MonitoredResourceOption>());
  EXPECT_TRUE(
      config->exporter_options.has<otel_internal::MetricPrefixOption>());
  EXPECT_TRUE(
      config->exporter_options.get<otel_internal::ServiceTimeSeriesOption>());
  EXPECT_EQ(config->reader_options.export_interval_millis,
            std::chrono::seconds(45));
  EXPECT_GT(config->reader_options.export_timeout_millis,
            std::chrono::milliseconds(0));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
