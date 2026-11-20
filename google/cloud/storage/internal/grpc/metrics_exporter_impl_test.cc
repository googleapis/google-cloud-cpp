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
#include "google/cloud/opentelemetry/internal/monitoring_exporter.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

auto EmptyResource() {
  return opentelemetry::sdk::resource::Resource::Create({});
}

auto FullResource() {
  return opentelemetry::sdk::resource::Resource::Create(
      {{"cloud.provider", "gcp"}, {"cloud.account.id", "project-id-resource"}});
}

auto TestOptions(Options opts = {}) {
  // In CI builds these environment variables may be set and affect the behavior
  // in ways we do not need to (re)test here.
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", std::nullopt);
  auto otel_env =
      ScopedEnvironment("GCP_STORAGE_CPP_ENABLE_OTEL_METRICS", std::nullopt);
  auto debug_env = ScopedEnvironment(
      "GCP_STORAGE_CPP_ENABLE_OTEL_DEBUG_METRICS", std::nullopt);
  opts.set<storage_experimental::EnableGrpcMetricsOption>(true)
      .set<UnifiedCredentialsOption>(MakeAccessTokenCredentials(
          "unused",
          std::chrono::system_clock::now() + std::chrono::minutes(15)));
  return DefaultOptionsGrpc(std::move(opts));
}

/// Shuts down a `MeterProvider` when the test ends, even after a failed
/// `ASSERT_*`.
///
/// `EnableGrpcMetricsImpl()` stores the provider in process-wide singletons
/// that are never destroyed. Calling `Shutdown()` joins the provider's
/// periodic reader thread before the test exits, so a background collection
/// cannot race with `exit()` destroying OpenTelemetry's static objects.
class ScopedProviderShutdown {
 public:
  explicit ScopedProviderShutdown(
      std::shared_ptr<opentelemetry::metrics::MeterProvider> provider)
      : provider_(std::move(provider)) {}
  ~ScopedProviderShutdown() {
    if (!provider_) return;
    // The API `MeterProvider` interface has no `Shutdown()`, and
    // `FindMeterProvider()` only returns `sdk::metrics::MeterProvider`
    // instances created by `MakeGrpcMeterProvider()`.
    static_cast<opentelemetry::sdk::metrics::MeterProvider*>(provider_.get())
        ->Shutdown();
  }
  ScopedProviderShutdown(ScopedProviderShutdown const&) = delete;
  ScopedProviderShutdown& operator=(ScopedProviderShutdown const&) = delete;

 private:
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
};

TEST(GrpcMetricsExporter, OptionCombinationsGateConfigAndGrpcPlugin) {
  for (bool const enable_grpc : {false, true}) {
    for (bool const enable_otel : {false, true}) {
      for (bool const enable_debug : {false, true}) {
        SCOPED_TRACE(std::string("grpc=") + (enable_grpc ? "true" : "false") +
                     ", otel=" + (enable_otel ? "true" : "false") +
                     ", debug=" + (enable_debug ? "true" : "false"));
        Options const opts =
            TestOptions()
                .set<storage_experimental::EnableGrpcMetricsOption>(enable_grpc)
                .set<storage_experimental::EnableOTelMetricsOption>(enable_otel)
                .set<storage_experimental::EnableOTelDebugMetricsOption>(
                    enable_debug);
        bool const expect_config = enable_grpc || enable_otel || enable_debug;
        EXPECT_EQ(AnyMetricsEnabled(opts), expect_config);
        std::optional<ExporterConfig> const config =
            MakeMeterProviderConfig(FullResource(), opts);
        ASSERT_EQ(config.has_value(), expect_config);
        if (expect_config) {
          EXPECT_EQ(config->enable_grpc_plugin, enable_grpc);
        }
      }
    }
  }
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

TEST(MakeMeterProviderConfigTest, NoExcludedLabels) {
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", "test-service"}, {"service.version", "1.0.0"}});

  Options options;
  options.set<storage_experimental::EnableGrpcMetricsOption>(true);
  options.set<storage::ProjectIdOption>("test-project");

  auto config = MakeMeterProviderConfig(resource, options);

  ASSERT_TRUE(config.has_value());
  EXPECT_FALSE(config->exporter_options
                   .has<otel_internal::ResourceFilterDataFnOption>());
}

TEST(MakeMeterProviderConfigTest, WithExcludedLabels) {
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", "test-service"}, {"service.version", "1.0.0"}});

  std::set<std::string> excluded_labels{"service_name", "service_version"};
  Options options;
  options.set<storage_experimental::EnableGrpcMetricsOption>(true);
  options.set<storage::ProjectIdOption>("test-project");
  options.set<storage_experimental::GrpcMetricsExcludedLabelsOption>(
      excluded_labels);

  auto config = MakeMeterProviderConfig(resource, options);

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->exporter_options
                  .has<otel_internal::ResourceFilterDataFnOption>());

  auto actual_excluded =
      config->exporter_options.get<otel_internal::ResourceFilterDataFnOption>();
  EXPECT_EQ(excluded_labels, actual_excluded);
}

TEST(MakeMeterProviderConfigTest, EmptyExcludedLabels) {
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", "test-service"}});

  Options options;
  options.set<storage_experimental::EnableGrpcMetricsOption>(true);
  options.set<storage::ProjectIdOption>("test-project");
  options.set<storage_experimental::GrpcMetricsExcludedLabelsOption>(
      std::set<std::string>{});

  auto config = MakeMeterProviderConfig(resource, options);

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->exporter_options
                  .has<otel_internal::ResourceFilterDataFnOption>());

  auto actual_excluded =
      config->exporter_options.get<otel_internal::ResourceFilterDataFnOption>();
  EXPECT_TRUE(actual_excluded.empty());
}

/// @test Verify `EnableGrpcMetricsImpl()` retains its provider for lookup.
///
/// Instruments created by this library, rather than by gRPC, find the provider
/// through `FindMeterProvider()`, so they share one exporter with gRPC's own
/// instruments. A repeated call for the same authority must keep the original
/// provider, otherwise those instruments would be split across two exporters.
/// The authority is unique to this test because the registry is process-wide.
TEST(GrpcMetricsExporter, FindMeterProviderAfterEnable) {
  std::string const authority = "test-only-find-provider.example.com";
  std::optional<ExporterConfig> config = MakeMeterProviderConfig(
      FullResource(), TestOptions().set<AuthorityOption>(authority));
  ASSERT_TRUE(config.has_value());
  EXPECT_THAT(FindMeterProvider(authority), IsNull());

  EnableGrpcMetricsImpl(*config);
  std::shared_ptr<opentelemetry::metrics::MeterProvider> const provider =
      FindMeterProvider(authority);
  ScopedProviderShutdown const provider_shutdown(provider);
  EXPECT_THAT(provider, NotNull());

  EnableGrpcMetricsImpl(*config);
  EXPECT_THAT(FindMeterProvider(authority), Eq(provider));
  EXPECT_THAT(FindMeterProvider("test-only-unknown.example.com"), IsNull());
}

/// @test Verify concurrent OTel-only and gRPC-enabled callers for the same
/// authority share one provider and every gRPC-enabled caller observes the
/// gRPC plugin registered before `EnableGrpcMetricsImpl()` returns.
TEST(GrpcMetricsExporter, ConcurrentEnableFindsSameProvider) {
  std::string const authority = "test-only-concurrent-provider.example.com";
  std::optional<ExporterConfig> const grpc_config = MakeMeterProviderConfig(
      FullResource(), TestOptions().set<AuthorityOption>(authority));
  std::optional<ExporterConfig> const otel_config = MakeMeterProviderConfig(
      FullResource(),
      TestOptions()
          .set<AuthorityOption>(authority)
          .set<storage_experimental::EnableGrpcMetricsOption>(false)
          .set<storage_experimental::EnableOTelMetricsOption>(true));
  ASSERT_TRUE(grpc_config.has_value());
  ASSERT_TRUE(otel_config.has_value());

  int constexpr kThreadCount = 8;
  std::atomic<int> plugin_registrations{0};
  auto register_plugin =
      [&](std::shared_ptr<opentelemetry::metrics::MeterProvider> const&,
          std::string const&) { ++plugin_registrations; };
  std::promise<void> start_promise;
  std::shared_future<void> const start = start_promise.get_future().share();
  std::vector<std::shared_ptr<opentelemetry::metrics::MeterProvider>> providers(
      kThreadCount);
  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);
  for (int i = 0; i != kThreadCount; ++i) {
    bool const grpc_enabled = (i % 2 == 0);
    threads.emplace_back([&, i, grpc_enabled] {
      start.wait();
      EnableGrpcMetricsImpl(grpc_enabled ? *grpc_config : *otel_config,
                            register_plugin);
      if (grpc_enabled) {
        EXPECT_EQ(plugin_registrations.load(), 1);
      }
      providers[i] = FindMeterProvider(authority);
    });
  }
  start_promise.set_value();
  for (auto& t : threads) t.join();
  ScopedProviderShutdown const provider_shutdown(FindMeterProvider(authority));

  EXPECT_EQ(plugin_registrations.load(), 1);
  EXPECT_THAT(providers[0], NotNull());
  for (int i = 1; i != kThreadCount; ++i) {
    EXPECT_THAT(providers[i], Eq(providers[0]));
  }
}

/// @test Verify OTel-only enable creates a provider without registering the
/// gRPC plugin, and a subsequent gRPC-metrics enable reuses the same provider
/// and registers the plugin once.
TEST(GrpcMetricsExporter, OTelOnlyProviderReusedByGrpcEnable) {
  std::string const authority = "test-only-otel-then-grpc.example.com";
  std::optional<ExporterConfig> const otel_config = MakeMeterProviderConfig(
      FullResource(),
      TestOptions()
          .set<AuthorityOption>(authority)
          .set<storage_experimental::EnableGrpcMetricsOption>(false)
          .set<storage_experimental::EnableOTelMetricsOption>(true));
  ASSERT_TRUE(otel_config.has_value());
  ASSERT_FALSE(otel_config->enable_grpc_plugin);

  int plugin_registrations = 0;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> registered_provider;
  auto register_plugin =
      [&](std::shared_ptr<opentelemetry::metrics::MeterProvider> const&
              provider,
          std::string const& registered_authority) {
        EXPECT_EQ(registered_authority, authority);
        registered_provider = provider;
        ++plugin_registrations;
      };

  EnableGrpcMetricsImpl(*otel_config, register_plugin);
  std::shared_ptr<opentelemetry::metrics::MeterProvider> const provider =
      FindMeterProvider(authority);
  ScopedProviderShutdown const provider_shutdown(provider);
  EXPECT_THAT(provider, NotNull());
  EXPECT_EQ(plugin_registrations, 0);

  // Repeated OTel-only calls reuse the same provider and skip plugin setup.
  EnableGrpcMetricsImpl(*otel_config, register_plugin);
  EXPECT_THAT(FindMeterProvider(authority), Eq(provider));
  EXPECT_EQ(plugin_registrations, 0);

  std::optional<ExporterConfig> const grpc_config = MakeMeterProviderConfig(
      FullResource(), TestOptions().set<AuthorityOption>(authority));
  ASSERT_TRUE(grpc_config.has_value());
  ASSERT_TRUE(grpc_config->enable_grpc_plugin);
  EnableGrpcMetricsImpl(*grpc_config, register_plugin);
  EXPECT_THAT(FindMeterProvider(authority), Eq(provider));
  EXPECT_THAT(registered_provider, Eq(provider));
  EXPECT_EQ(plugin_registrations, 1);

  // Repeated gRPC-enabled and subsequent OTel-only calls reuse the provider
  // without registering the gRPC plugin again.
  EnableGrpcMetricsImpl(*grpc_config, register_plugin);
  EnableGrpcMetricsImpl(*otel_config, register_plugin);
  EXPECT_THAT(FindMeterProvider(authority), Eq(provider));
  EXPECT_EQ(plugin_registrations, 1);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
