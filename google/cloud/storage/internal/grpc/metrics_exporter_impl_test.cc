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
#include "google/cloud/storage/internal/grpc/enable_metrics.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/constants.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/universe_domain_options.h"
#include "google/api/monitored_resource.pb.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <nlohmann/json.hpp>
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
using ::google::cloud::testing_util::ScopedLog;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pair;

auto EmptyResource() {
  return opentelemetry::sdk::resource::Resource::Create({});
}

auto FullResource() {
  return opentelemetry::sdk::resource::Resource::Create(
      {{"cloud.provider", "gcp"}, {"cloud.account.id", "project-id-resource"}});
}

std::shared_ptr<Credentials> AccessTokenCredentials() {
  return MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::minutes(15));
}

auto TestOptions(Options opts = {}) {
  // In CI builds these environment variables may be set and affect the behavior
  // in ways we do not need to (re)test here.
  auto env = ScopedEnvironment("GOOGLE_CLOUD_PROJECT", std::nullopt);
  auto otel_env =
      ScopedEnvironment("GCP_STORAGE_CPP_ENABLE_OTEL_METRICS", std::nullopt);
  auto debug_env = ScopedEnvironment(
      "GCP_STORAGE_CPP_ENABLE_OTEL_DEBUG_METRICS", std::nullopt);
  ScopedEnvironment const emulator_env(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", std::nullopt);
  ScopedEnvironment const universe_env("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                       std::nullopt);
  // Use the credentials of the host. Only for those does the resource passed
  // to `MakeMeterProviderConfig()` supply the monitoring project.
  opts.set<storage_experimental::EnableGrpcMetricsOption>(true)
      .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials());
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
  // Access token credentials name no project. With the host's credentials the
  // lookup would continue with the metadata server.
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      TestOptions().set<UnifiedCredentialsOption>(AccessTokenCredentials()));
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

  std::optional<ExporterConfig> const config =
      MakeMeterProviderConfig(resource, TestOptions(options));

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

  std::optional<ExporterConfig> const config =
      MakeMeterProviderConfig(resource, TestOptions(options));

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

  std::optional<ExporterConfig> const config =
      MakeMeterProviderConfig(resource, TestOptions(options));

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

/// A path where no file exists, for tests about a missing credentials file.
std::string MissingFile() {
  return internal::PathAppend(::testing::TempDir(),
                              "grpc-metrics-exporter-test-missing-file.json");
}

/// The resource the detector reports on a Compute Engine instance.
opentelemetry::sdk::resource::Resource HostResource() {
  return opentelemetry::sdk::resource::Resource::Create({
      {"cloud.provider", "gcp"},
      {"cloud.account.id", "project-id-resource"},
      {"cloud.platform", "gcp_compute_engine"},
      {"cloud.availability_zone", "us-central1-c"},
      {"host.id", "test-host-id"},
  });
}

/// The labels of the monitored resource the exporter attaches to its time
/// series.
google::protobuf::Map<std::string, std::string> MonitoredResourceLabels(
    ExporterConfig const& config) {
  return config.exporter_options.get<otel::MonitoredResourceOption>().labels();
}

/// The options of a client that enables the exporter explicitly, so a result
/// does not depend on the gRPC version the test is built with.
Options EnabledOptions() {
  return Options{}.set<storage_experimental::EnableGrpcMetricsOption>(true);
}

/// As `EnabledOptions()`, with a monitoring project that depends neither on
/// the host nor on the credentials.
Options EnabledOptionsWithProject() {
  return EnabledOptions().set<storage::ProjectIdOption>("project-id-option");
}

/// A service account key file for the project `project-id-credentials`. Like
/// key files created before the field existed, it has no `universe_domain`.
nlohmann::json ServiceAccountKey() {
  return nlohmann::json{
      {"type", "service_account"},
      {"project_id", "project-id-credentials"},
      {"private_key_id", "test-only-key-id"},
      {"private_key", google::cloud::storage::testing::kWellFormattedKey},
      {"client_email", "sa@invalid-test-only-project.iam.gserviceaccount.com"},
      {"client_id", "invalid-test-only-client-id"},
      {"auth_uri", "https://accounts.google.com/o/oauth2/auth"},
      {"token_uri", "https://accounts.google.com/o/oauth2/token"},
      {"auth_provider_x509_cert_url",
       "https://www.googleapis.com/oauth2/v1/certs"},
      {"client_x509_cert_url",
       "https://www.googleapis.com/robot/v1/metadata/x509/"
       "foo-email%40foo-project.iam.gserviceaccount.com"},
  };
}

/// The minimum an external account (Workload Identity Federation) file needs
/// to be accepted. The subject token file is only read to fetch a token.
nlohmann::json ExternalAccountFile() {
  return nlohmann::json{
      {"type", "external_account"},
      {"audience",
       "//iam.googleapis.com/projects/123456/locations/global/"
       "workloadIdentityPools/test-pool/providers/test-provider"},
      {"subject_token_type", "urn:ietf:params:oauth:token-type:jwt"},
      {"token_url", "https://sts.googleapis.com/v1/token"},
      {"credential_source", {{"file", "/var/run/secrets/test-subject-token"}}},
  };
}

/// The file `gcloud auth application-default login` writes.
nlohmann::json AuthorizedUserFile() {
  return nlohmann::json{
      {"type", "authorized_user"},
      {"client_id", "test-client-id.apps.googleusercontent.com"},
      {"client_secret", "test-client-secret"},
      {"refresh_token", "test-refresh-token"},
  };
}

/// Returns the text of @p credentials_file with its `universe_domain` field
/// set to @p universe_domain.
std::string WithUniverseDomain(nlohmann::json credentials_file,
                               nlohmann::json const& universe_domain) {
  credentials_file["universe_domain"] = universe_domain;
  return credentials_file.dump();
}

/// Controls every process-wide input of `DefaultOptionsGrpc()` and of the
/// Application Default Credentials lookup, so a test depends only on the
/// options and variables it sets itself. Otherwise the results would change
/// with the gcloud credentials of whoever runs the test.
class GrpcMetricsExporterConfigTest : public ::testing::Test {
 protected:
  /// Makes a file with @p contents the Application Default Credentials.
  void SetApplicationDefaultCredentials(std::string const& contents) {
    adc_file_.emplace(contents);
    adc_override_.emplace("GOOGLE_APPLICATION_CREDENTIALS", adc_file_->name());
  }

  /// Names a file that does not exist as the Application Default Credentials.
  void SetMissingApplicationDefaultCredentials() {
    adc_override_.emplace("GOOGLE_APPLICATION_CREDENTIALS", MissingFile());
  }

 private:
  ScopedEnvironment project_{"GOOGLE_CLOUD_PROJECT", std::nullopt};
  ScopedEnvironment otel_metrics_{"GCP_STORAGE_CPP_ENABLE_OTEL_METRICS",
                                  std::nullopt};
  ScopedEnvironment otel_debug_metrics_{
      "GCP_STORAGE_CPP_ENABLE_OTEL_DEBUG_METRICS", std::nullopt};
  ScopedEnvironment universe_domain_{"GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                     std::nullopt};
  ScopedEnvironment emulator_{
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", std::nullopt};
  ScopedEnvironment preserve_credentials_{
      "GOOGLE_CLOUD_CPP_STORAGE_TESTING_PRESERVE_CREDENTIALS", std::nullopt};
  ScopedEnvironment interconnect_{
      "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", std::nullopt};
  // Unless a test sets one, there is no Application Default Credentials file:
  // the variable is unset and the gcloud location names a missing file.
  ScopedEnvironment adc_{"GOOGLE_APPLICATION_CREDENTIALS", std::nullopt};
  ScopedEnvironment gcloud_adc_{"GOOGLE_GCLOUD_ADC_PATH_OVERRIDE",
                                MissingFile()};
  std::optional<storage::testing::TempFile> adc_file_;
  std::optional<ScopedEnvironment> adc_override_;
};

/// @test Verify `UnifiedCredentialsOption` is propagated to the Monitoring
/// connection options.
TEST_F(GrpcMetricsExporterConfigTest, UsesClientCredentials) {
  std::shared_ptr<Credentials> const credentials = AccessTokenCredentials();
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
              credentials)));
  ASSERT_TRUE(config.has_value());
  EXPECT_THAT(
      config->exporter_connection_options.get<UnifiedCredentialsOption>(),
      Eq(credentials));
}

/// @test Verify the default credentials populated by `DefaultOptionsGrpc()`
/// are forwarded to the Monitoring connection options.
TEST_F(GrpcMetricsExporterConfigTest, UsesClientDefaultCredentials) {
  Options const options = DefaultOptionsGrpc(EnabledOptionsWithProject());
  std::optional<ExporterConfig> const config =
      MakeMeterProviderConfig(EmptyResource(), options);
  ASSERT_TRUE(config.has_value());
  EXPECT_THAT(
      config->exporter_connection_options.get<UnifiedCredentialsOption>(),
      AllOf(NotNull(), Eq(options.get<UnifiedCredentialsOption>())));
}

/// @test Verify `GrpcCredentialOption` is forwarded without setting
/// `UnifiedCredentialsOption`, which would take precedence when creating the
/// Monitoring channel.
TEST_F(GrpcMetricsExporterConfigTest, UsesClientGrpcCredentials) {
  std::shared_ptr<grpc::ChannelCredentials> const credentials =
      grpc::InsecureChannelCredentials();
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<GrpcCredentialOption>(credentials)));
  ASSERT_TRUE(config.has_value());
  Options const& exporter = config->exporter_connection_options;
  EXPECT_THAT(exporter.get<GrpcCredentialOption>(), Eq(credentials));
  EXPECT_FALSE(exporter.has<UnifiedCredentialsOption>());
}

/// @test Verify both credential options are forwarded when both are set, so
/// the Monitoring connection resolves them with the same precedence as the
/// Storage connection.
TEST_F(GrpcMetricsExporterConfigTest, UsesBothCredentialOptions) {
  std::shared_ptr<Credentials> const unified = AccessTokenCredentials();
  std::shared_ptr<grpc::ChannelCredentials> const channel =
      grpc::InsecureChannelCredentials();
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(EnabledOptionsWithProject()
                             .set<UnifiedCredentialsOption>(unified)
                             .set<GrpcCredentialOption>(channel)));
  ASSERT_TRUE(config.has_value());
  Options const& exporter = config->exporter_connection_options;
  EXPECT_THAT(exporter.get<UnifiedCredentialsOption>(), Eq(unified));
  EXPECT_THAT(exporter.get<GrpcCredentialOption>(), Eq(channel));
}

/// @test Verify `CARootsFilePathOption` is forwarded to the Monitoring
/// connection options.
TEST_F(GrpcMetricsExporterConfigTest, UsesClientCARoots) {
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject()
              .set<UnifiedCredentialsOption>(AccessTokenCredentials())
              .set<CARootsFilePathOption>("/test-only/ca-certificates.pem")));
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->exporter_connection_options.get<CARootsFilePathOption>(),
            "/test-only/ca-certificates.pem");
}

/// @test Verify `ProxyOption` is forwarded to the Monitoring connection
/// options.
TEST_F(GrpcMetricsExporterConfigTest, UsesClientProxy) {
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject()
              .set<UnifiedCredentialsOption>(AccessTokenCredentials())
              .set<ProxyOption>(ProxyConfig()
                                    .set_hostname("proxy.example.com")
                                    .set_port("3128"))));
  ASSERT_TRUE(config.has_value());
  ProxyConfig const& proxy =
      config->exporter_connection_options.get<ProxyOption>();
  EXPECT_EQ(proxy.hostname(), "proxy.example.com");
  EXPECT_EQ(proxy.port(), "3128");
}

/// @test Verify Storage-specific channel and per-request options (such as
/// `AuthorityOption`, `UserProjectOption`, and
/// `GrpcGoogleDefaultTlsOnlyOption`) are not copied into the Monitoring
/// connection options.
TEST_F(GrpcMetricsExporterConfigTest, ForwardsOnlyConnectionOptions) {
  Options const options = DefaultOptionsGrpc(
      EnabledOptionsWithProject()
          .set<EndpointOption>(
              "google-c2p:///storage-direct.googleapis.com?force-xds")
          .set<AuthorityOption>("storage-direct.googleapis.com")
          .set<UserProjectOption>("test-only-user-project")
          .set<CustomHeadersOption>({{"x-test-only-header", "value"}})
          .set<GrpcChannelArgumentsOption>({{"test-only-argument", "value"}})
          .set<GrpcNumChannelsOption>(3),
      internal::MakeGcpDetector(), /*grpc_supports_force_xds=*/true);
  ASSERT_TRUE(options.get<internal::GrpcGoogleDefaultTlsOnlyOption>());

  std::optional<ExporterConfig> const config =
      MakeMeterProviderConfig(EmptyResource(), options);
  ASSERT_TRUE(config.has_value());
  Options const& exporter = config->exporter_connection_options;
  EXPECT_FALSE(exporter.has<EndpointOption>());
  EXPECT_FALSE(exporter.has<AuthorityOption>());
  EXPECT_FALSE(exporter.has<UserProjectOption>());
  EXPECT_FALSE(exporter.has<CustomHeadersOption>());
  EXPECT_FALSE(exporter.has<GrpcChannelArgumentsOption>());
  EXPECT_FALSE(exporter.has<GrpcNumChannelsOption>());
  EXPECT_FALSE(exporter.has<internal::GrpcGoogleDefaultTlsOnlyOption>());
  EXPECT_FALSE(exporter.has<storage::ProjectIdOption>());
  EXPECT_FALSE(exporter.has<CARootsFilePathOption>());
  EXPECT_FALSE(exporter.has<ProxyOption>());
}

/// @test Verify metrics export is disabled when the client uses
/// `MakeInsecureCredentials()` (for example against a local emulator), and
/// logs the reason.
TEST_F(GrpcMetricsExporterConfigTest, SkipsExportForInsecureCredentials) {
  ScopedLog log;
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      HostResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
              MakeInsecureCredentials())));
  EXPECT_FALSE(config.has_value());
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("metrics export is disabled"),
                             HasSubstr("insecure credentials"))));
}

/// @test Verify the emulator endpoint gets no exporter, even when the
/// application enables metrics explicitly. `DefaultOptionsGrpc()` points such
/// a client at the emulator with insecure credentials.
TEST_F(GrpcMetricsExporterConfigTest, SkipsExportForEmulatorEndpoint) {
  ScopedEnvironment const emulator(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", "localhost:1234");
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      HostResource(), DefaultOptionsGrpc(EnabledOptionsWithProject()));
  EXPECT_FALSE(config.has_value());
}

/// @test Verify `GOOGLE_CLOUD_CPP_METRIC_SERVICE_ENDPOINT` does not change
/// whether metrics are exported.
///
/// The variable redirects the Monitoring connection of every client in the
/// process. It says nothing about the universe of one client's endpoint, so it
/// neither enables the exporter for an endpoint outside the universe domain
/// nor disables it for one inside.
TEST_F(GrpcMetricsExporterConfigTest, IgnoresMetricServiceEndpointVariable) {
  ScopedEnvironment const metric_endpoint(
      "GOOGLE_CLOUD_CPP_METRIC_SERVICE_ENDPOINT", "monitoring.example.com");
  Options const client_options =
      EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
          AccessTokenCredentials());

  std::optional<ExporterConfig> const inside = MakeMeterProviderConfig(
      EmptyResource(), DefaultOptionsGrpc(client_options));
  EXPECT_TRUE(inside.has_value());

  std::optional<ExporterConfig> const outside = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          Options(client_options).set<EndpointOption>("storage.ud.net")));
  EXPECT_FALSE(outside.has_value());
}

/// @test Verify `GOOGLE_CLOUD_UNIVERSE_DOMAIN` selects the universe domain the
/// endpoint is compared with, as `UniverseDomainOption` does. The variable
/// applies to every client in the process, so an application-supplied endpoint
/// in the default universe is then outside the universe domain.
TEST_F(GrpcMetricsExporterConfigTest, UniverseDomainVariableSelectsUniverse) {
  ScopedEnvironment const universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                                          "ud.net");
  Options const client_options =
      EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
          AccessTokenCredentials());

  std::optional<ExporterConfig> const library_endpoint =
      MakeMeterProviderConfig(EmptyResource(),
                              DefaultOptionsGrpc(client_options));
  ASSERT_TRUE(library_endpoint.has_value());
  EXPECT_EQ(library_endpoint->exporter_connection_options
                .get<internal::UniverseDomainOption>(),
            "ud.net");

  std::optional<ExporterConfig> const default_universe_endpoint =
      MakeMeterProviderConfig(
          EmptyResource(),
          DefaultOptionsGrpc(
              Options(client_options)
                  .set<EndpointOption>("storage.googleapis.com")));
  EXPECT_FALSE(default_universe_endpoint.has_value());
}

/// @test Verify an empty `GOOGLE_CLOUD_UNIVERSE_DOMAIN` is treated as unset,
/// so the default endpoint is still in the (default) universe domain.
TEST_F(GrpcMetricsExporterConfigTest, EmptyUniverseDomainVariableIsIgnored) {
  ScopedEnvironment const universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "");
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
              AccessTokenCredentials())));
  ASSERT_TRUE(config.has_value());
  EXPECT_FALSE(config->exporter_connection_options
                   .has<internal::UniverseDomainOption>());
}

struct EndpointTestCase {
  std::string test_name;
  // An application-supplied `EndpointOption`. When unset, the library selects
  // the endpoint.
  std::optional<std::string> endpoint;
  // An application-supplied `UniverseDomainOption`.
  std::optional<std::string> universe_domain;
  bool expected_export;
};

class GrpcMetricsExporterEndpointTest
    : public GrpcMetricsExporterConfigTest,
      public ::testing::WithParamInterface<EndpointTestCase> {};

/// @test Verify metrics export is enabled only when the client's endpoint
/// resolves to a subdomain of the configured universe domain
/// (case-insensitive), and disabled for custom hosts, IP literals, or
/// unsupported gRPC target schemes.
TEST_P(GrpcMetricsExporterEndpointTest, ExportsOnlyWithinUniverseDomain) {
  EndpointTestCase const& test_case = GetParam();
  Options client_options =
      EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
          AccessTokenCredentials());
  if (test_case.endpoint) {
    client_options.set<EndpointOption>(*test_case.endpoint);
  }
  if (test_case.universe_domain) {
    client_options.set<internal::UniverseDomainOption>(
        *test_case.universe_domain);
  }
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(), DefaultOptionsGrpc(client_options));
  EXPECT_EQ(config.has_value(), test_case.expected_export)
      << "endpoint=" << test_case.endpoint.value_or("(library default)")
      << ", universe_domain="
      << test_case.universe_domain.value_or("(not set)");
}

INSTANTIATE_TEST_SUITE_P(
    GrpcMetricsExporter, GrpcMetricsExporterEndpointTest,
    ::testing::Values(
        // Endpoints the library selects.
        EndpointTestCase{"LibraryDefault", std::nullopt, std::nullopt, true},
        EndpointTestCase{"LibraryDefaultWithUniverseDomain", std::nullopt,
                         "ud.net", true},
        // Application-supplied endpoints in the default universe.
        EndpointTestCase{"CloudPath", "storage.googleapis.com", std::nullopt,
                         true},
        EndpointTestCase{"CloudPathWithPort", "storage.googleapis.com:443",
                         std::nullopt, true},
        EndpointTestCase{"MixedCase", "STORAGE.GoogleAPIs.com", std::nullopt,
                         true},
        EndpointTestCase{"DirectPath", "google-c2p:///storage.googleapis.com",
                         std::nullopt, true},
        EndpointTestCase{"DirectPathWithPort",
                         "google-c2p:///storage.googleapis.com:443",
                         std::nullopt, true},
        EndpointTestCase{"DirectPathExperimentalScheme",
                         "google-c2p-experimental:///storage.googleapis.com",
                         std::nullopt, true},
        EndpointTestCase{
            "Interconnect",
            "google-c2p:///storage-direct.googleapis.com?force-xds",
            std::nullopt, true},
        EndpointTestCase{
            "InterconnectPreprod",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds",
            std::nullopt, true},
        EndpointTestCase{"PrivateServiceConnect",
                         "storage-foo.p.googleapis.com", std::nullopt, true},
        EndpointTestCase{"PrivateVip", "private.googleapis.com", std::nullopt,
                         true},
        EndpointTestCase{"RestrictedVip", "restricted.googleapis.com",
                         std::nullopt, true},
        // Application-supplied endpoints in another universe, with the
        // matching universe domain.
        EndpointTestCase{"UniverseDomain", "storage.ud.net", "ud.net", true},
        EndpointTestCase{"UniverseDomainMixedCase", "storage.ud.net", "UD.Net",
                         true},
        EndpointTestCase{"UniverseDomainDirectPath",
                         "google-c2p:///storage.ud.net", "ud.net", true},
        EndpointTestCase{"UniverseDomainPrivateVip", "private.ud.net", "ud.net",
                         true},
        // The endpoint and the universe domain name different universes.
        EndpointTestCase{"OtherUniverse", "storage.ud.net", std::nullopt,
                         false},
        EndpointTestCase{"OtherUniverseDirectPath",
                         "google-c2p:///storage.ud.net", std::nullopt, false},
        EndpointTestCase{"DefaultUniverseWithUniverseDomain",
                         "storage.googleapis.com", "ud.net", false},
        EndpointTestCase{"DefaultUniversePrivateVipWithUniverseDomain",
                         "private.googleapis.com", "ud.net", false},
        EndpointTestCase{"DefaultUniversePrivateServiceConnectWithUniverseDoma"
                         "in",
                         "storage-foo.p.googleapis.com", "ud.net", false},
        // Non-subdomain host names.
        EndpointTestCase{"SuffixWithoutDot", "storage.notgoogleapis.com",
                         std::nullopt, false},
        EndpointTestCase{"SuffixWithoutLabel", "examplegoogleapis.com",
                         std::nullopt, false},
        EndpointTestCase{"UniverseDomainItself", "googleapis.com", std::nullopt,
                         false},
        EndpointTestCase{"UniverseDomainAsPrefix",
                         "storage.googleapis.com.example.com", std::nullopt,
                         false},
        // Non-universe hosts.
        EndpointTestCase{"Localhost", "localhost:8080", std::nullopt, false},
        EndpointTestCase{"Ipv4Address", "127.0.0.1:9000", std::nullopt, false},
        EndpointTestCase{"Ipv6Address", "[::1]:9000", std::nullopt, false},
        EndpointTestCase{"PrivateDnsName", "storage.corp.example:443",
                         std::nullopt, false},
        // Unsupported gRPC target formats and invalid DNS host names.
        EndpointTestCase{"HttpsScheme", "https://storage.googleapis.com",
                         std::nullopt, false},
        EndpointTestCase{"DnsScheme", "dns:///storage.googleapis.com",
                         std::nullopt, false},
        EndpointTestCase{"UnixScheme", "unix:/tmp/test-only.sock", std::nullopt,
                         false},
        EndpointTestCase{"DirectPathWithAuthority",
                         "google-c2p://xds/storage.googleapis.com",
                         std::nullopt, false},
        EndpointTestCase{"UnderscoreInHost", "storage_1.googleapis.com",
                         std::nullopt, false},
        EndpointTestCase{"Path", "storage.googleapis.com/x", std::nullopt,
                         false},
        EndpointTestCase{"DirectPathPath",
                         "google-c2p:///storage.googleapis.com/x", std::nullopt,
                         false},
        EndpointTestCase{"DirectPathPortAndPath",
                         "google-c2p:///storage.googleapis.com:443/v1",
                         std::nullopt, false},
        EndpointTestCase{"Query", "storage.googleapis.com?x", std::nullopt,
                         false},
        EndpointTestCase{"DirectPathFragment",
                         "google-c2p:///storage.googleapis.com#x", std::nullopt,
                         false},
        EndpointTestCase{"TrailingDot", "storage.googleapis.com.", std::nullopt,
                         false},
        EndpointTestCase{"EmptyLabel", "storage..googleapis.com", std::nullopt,
                         false},
        EndpointTestCase{"NonNumericPort", "storage.googleapis.com:https",
                         std::nullopt, false},
        EndpointTestCase{"DirectPathNonNumericPort",
                         "google-c2p:///storage.googleapis.com:https",
                         std::nullopt, false},
        EndpointTestCase{"EmptyPort", "storage.googleapis.com:", std::nullopt,
                         false},
        EndpointTestCase{"DirectPathEmptyPort",
                         "google-c2p:///storage.googleapis.com:", std::nullopt,
                         false},
        EndpointTestCase{"DirectPathWithoutHost", "google-c2p:///",
                         std::nullopt, false},
        EndpointTestCase{"EmptyEndpoint", "", std::nullopt, false},
        // An empty universe domain names no universe.
        EndpointTestCase{"EmptyUniverseDomain", std::nullopt, "", false},
        EndpointTestCase{"EmptyUniverseDomainWithEndpoint",
                         "storage.googleapis.com", "", false}),
    [](::testing::TestParamInfo<EndpointTestCase> const& info) {
      return info.param.test_name;
    });

// The credentials the tables below are built from. Each credentials file
// comes in two forms: as the tooling wrote it, and stating a universe domain.
std::shared_ptr<Credentials> ServiceAccount() {
  return MakeServiceAccountCredentials(ServiceAccountKey().dump());
}

std::shared_ptr<Credentials> ServiceAccountFor(
    nlohmann::json const& universe_domain) {
  return MakeServiceAccountCredentials(
      WithUniverseDomain(ServiceAccountKey(), universe_domain));
}

std::shared_ptr<Credentials> ExternalAccount() {
  return MakeExternalAccountCredentials(ExternalAccountFile().dump());
}

std::shared_ptr<Credentials> ExternalAccountFor(
    nlohmann::json const& universe_domain) {
  return MakeExternalAccountCredentials(
      WithUniverseDomain(ExternalAccountFile(), universe_domain));
}

std::shared_ptr<Credentials> AuthorizedUser() {
  return internal::MakeUserAccountCredentials(AuthorizedUserFile().dump());
}

std::shared_ptr<Credentials> AuthorizedUserFor(
    nlohmann::json const& universe_domain) {
  return internal::MakeUserAccountCredentials(
      WithUniverseDomain(AuthorizedUserFile(), universe_domain));
}

std::shared_ptr<Credentials> ImpersonationFrom(
    std::shared_ptr<Credentials> const& base) {
  return MakeImpersonateServiceAccountCredentials(
      base, "sa@invalid-test-only-project.iam.gserviceaccount.com");
}

Options Unified(std::shared_ptr<Credentials> const& credentials) {
  return Options{}.set<UnifiedCredentialsOption>(credentials);
}

struct HostResourceTestCase {
  std::string test_name;
  // The credential options the application sets; none for the library default.
  Options credential_options;
  // An application-supplied `storage::ProjectIdOption`.
  std::optional<std::string> project_option;
  // The monitoring project. When unset, no exporter is expected.
  std::optional<std::string> expected_project;
  // Whether the monitored resource names the host's zone, platform and id.
  bool expected_host_labels;
};

class GrpcMetricsExporterHostResourceTest
    : public GrpcMetricsExporterConfigTest,
      public ::testing::WithParamInterface<HostResourceTestCase> {};

/// @test Verify host resource attributes (project, zone, platform, and host
/// id) are used only when the client authenticates via the host's metadata
/// server; otherwise the project comes from `storage::ProjectIdOption` or the
/// credentials with default resource labels.
TEST_P(GrpcMetricsExporterHostResourceTest,
       UsesHostResourceOnlyForMetadataCredentials) {
  HostResourceTestCase const& test_case = GetParam();
  Options client_options =
      internal::MergeOptions(test_case.credential_options, EnabledOptions());
  if (test_case.project_option) {
    client_options.set<storage::ProjectIdOption>(*test_case.project_option);
  }
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      HostResource(), DefaultOptionsGrpc(client_options));
  if (!test_case.expected_project) {
    EXPECT_FALSE(config.has_value());
    return;
  }
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project(*test_case.expected_project));
  bool const host = test_case.expected_host_labels;
  EXPECT_THAT(
      MonitoredResourceLabels(*config),
      AllOf(Contains(Pair("project_id", *test_case.expected_project)),
            Contains(Pair("location", host ? "us-central1-c" : "global")),
            Contains(Pair("cloud_platform",
                          host ? "gcp_compute_engine" : "unknown")),
            Contains(Pair("host_id", host ? "test-host-id" : "unknown"))));
}

INSTANTIATE_TEST_SUITE_P(
    GrpcMetricsExporter, GrpcMetricsExporterHostResourceTest,
    ::testing::Values(
        // Credentials served by the host's metadata server. The host's
        // project takes precedence over `storage::ProjectIdOption`.
        HostResourceTestCase{"LibraryDefaultCredentials", Options{},
                             std::nullopt, "project-id-resource", true},
        HostResourceTestCase{"LibraryDefaultCredentialsWithProjectOption",
                             Options{}, "project-id-option",
                             "project-id-resource", true},
        HostResourceTestCase{"ComputeEngineCredentials",
                             Unified(MakeComputeEngineCredentials()),
                             std::nullopt, "project-id-resource", true},
        HostResourceTestCase{
            "ImpersonationFromComputeEngineCredentials",
            Unified(ImpersonationFrom(MakeComputeEngineCredentials())),
            std::nullopt, "project-id-resource", true},
        // Credentials from anywhere else.
        HostResourceTestCase{"AccessToken", Unified(AccessTokenCredentials()),
                             "project-id-option", "project-id-option", false},
        HostResourceTestCase{"AccessTokenWithoutProject",
                             Unified(AccessTokenCredentials()), std::nullopt,
                             std::nullopt, false},
        HostResourceTestCase{
            "ImpersonationFromAccessToken",
            Unified(ImpersonationFrom(AccessTokenCredentials())),
            "project-id-option", "project-id-option", false},
        HostResourceTestCase{"ServiceAccountKey", Unified(ServiceAccount()),
                             std::nullopt, "project-id-credentials", false},
        HostResourceTestCase{"ServiceAccountKeyWithProjectOption",
                             Unified(ServiceAccount()), "project-id-option",
                             "project-id-option", false},
        // gRPC channel credentials are opaque, so the library cannot tell
        // whether they come from the host.
        HostResourceTestCase{"GrpcCredentials",
                             Options{}.set<GrpcCredentialOption>(
                                 grpc::InsecureChannelCredentials()),
                             "project-id-option", "project-id-option", false},
        HostResourceTestCase{"GrpcCredentialsWithoutProject",
                             Options{}.set<GrpcCredentialOption>(
                                 grpc::InsecureChannelCredentials()),
                             std::nullopt, std::nullopt, false}),
    [](::testing::TestParamInfo<HostResourceTestCase> const& info) {
      return info.param.test_name;
    });

struct CredentialsUniverseTestCase {
  std::string test_name;
  // The `UnifiedCredentialsOption` of the client.
  std::shared_ptr<Credentials> credentials;
  // An application-supplied `UniverseDomainOption`. The client uses the
  // default endpoint of that universe.
  std::optional<std::string> universe_domain;
  bool expected_export;
};

class GrpcMetricsExporterCredentialsUniverseTest
    : public GrpcMetricsExporterConfigTest,
      public ::testing::WithParamInterface<CredentialsUniverseTestCase> {};

/// @test Verify metrics export requires a matching `universe_domain` for
/// credential types that carry one (service account, external account, and
/// authorized user JSON), defaulting to `googleapis.com` when the field is
/// absent, and skips export when the JSON cannot be read or parsed.
TEST_P(GrpcMetricsExporterCredentialsUniverseTest,
       ExportsOnlyWithCredentialsForUniverseDomain) {
  CredentialsUniverseTestCase const& test_case = GetParam();
  Options client_options =
      EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
          test_case.credentials);
  if (test_case.universe_domain) {
    client_options.set<internal::UniverseDomainOption>(
        *test_case.universe_domain);
  }
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(), DefaultOptionsGrpc(client_options));
  EXPECT_EQ(config.has_value(), test_case.expected_export);
}

INSTANTIATE_TEST_SUITE_P(
    GrpcMetricsExporter, GrpcMetricsExporterCredentialsUniverseTest,
    ::testing::Values(
        // Service account keys.
        CredentialsUniverseTestCase{"ServiceAccountWithoutUniverse",
                                    ServiceAccount(), std::nullopt, true},
        CredentialsUniverseTestCase{
            "ServiceAccountWithoutUniverseInOtherUniverse", ServiceAccount(),
            "ud.net", false},
        CredentialsUniverseTestCase{"ServiceAccountForDefaultUniverse",
                                    ServiceAccountFor("googleapis.com"),
                                    std::nullopt, true},
        CredentialsUniverseTestCase{
            "ServiceAccountForDefaultUniverseInOtherUniverse",
            ServiceAccountFor("googleapis.com"), "ud.net", false},
        CredentialsUniverseTestCase{"ServiceAccountForOtherUniverse",
                                    ServiceAccountFor("ud.net"), "ud.net",
                                    true},
        CredentialsUniverseTestCase{"ServiceAccountForOtherUniverseMixedCase",
                                    ServiceAccountFor("UD.Net"), "ud.net",
                                    true},
        CredentialsUniverseTestCase{
            "ServiceAccountForOtherUniverseInDefaultUniverse",
            ServiceAccountFor("ud.net"), std::nullopt, false},
        CredentialsUniverseTestCase{"ServiceAccountEmptyUniverse",
                                    ServiceAccountFor(""), std::nullopt, false},
        CredentialsUniverseTestCase{"ServiceAccountNonStringUniverse",
                                    ServiceAccountFor(42), std::nullopt, false},
        CredentialsUniverseTestCase{"ServiceAccountNotJson",
                                    MakeServiceAccountCredentials("not-json"),
                                    std::nullopt, false},
        CredentialsUniverseTestCase{"ServiceAccountNotAJsonObject",
                                    MakeServiceAccountCredentials("[]"),
                                    std::nullopt, false},
        CredentialsUniverseTestCase{
            "ServiceAccountFileMissing",
            MakeServiceAccountCredentialsFromFile(MissingFile()), std::nullopt,
            false},
        // The other credential types that name their universe.
        CredentialsUniverseTestCase{"ExternalAccountWithoutUniverse",
                                    ExternalAccount(), std::nullopt, true},
        CredentialsUniverseTestCase{"ExternalAccountForOtherUniverse",
                                    ExternalAccountFor("ud.net"), "ud.net",
                                    true},
        CredentialsUniverseTestCase{
            "ExternalAccountForOtherUniverseInDefaultUniverse",
            ExternalAccountFor("ud.net"), std::nullopt, false},
        CredentialsUniverseTestCase{"AuthorizedUserWithoutUniverse",
                                    AuthorizedUser(), std::nullopt, true},
        CredentialsUniverseTestCase{
            "AuthorizedUserForOtherUniverseInDefaultUniverse",
            AuthorizedUserFor("ud.net"), std::nullopt, false},
        // Impersonation, by the credentials it starts from.
        CredentialsUniverseTestCase{
            "ImpersonationFromServiceAccountForOtherUniverse",
            ImpersonationFrom(ServiceAccountFor("ud.net")), "ud.net", true},
        CredentialsUniverseTestCase{
            "ImpersonationFromServiceAccountForOtherUniverseInDefaultUniverse",
            ImpersonationFrom(ServiceAccountFor("ud.net")), std::nullopt,
            false},
        CredentialsUniverseTestCase{"ImpersonationWithoutBaseCredentials",
                                    ImpersonationFrom(nullptr), std::nullopt,
                                    false},
        // Credentials that carry no universe are not compared.
        CredentialsUniverseTestCase{"AccessTokenInOtherUniverse",
                                    AccessTokenCredentials(), "ud.net", true},
        CredentialsUniverseTestCase{"ComputeEngineInOtherUniverse",
                                    MakeComputeEngineCredentials(), "ud.net",
                                    true},
        CredentialsUniverseTestCase{
            "ImpersonationFromAccessTokenInOtherUniverse",
            ImpersonationFrom(AccessTokenCredentials()), "ud.net", true},
        // Credentials not supported for Cloud Monitoring export.
        CredentialsUniverseTestCase{"ApiKeyInDefaultUniverse",
                                    MakeApiKeyCredentials("test-only-api-key"),
                                    std::nullopt, false},
        CredentialsUniverseTestCase{"ApiKeyInOtherUniverse",
                                    MakeApiKeyCredentials("test-only-api-key"),
                                    "ud.net", false},
        CredentialsUniverseTestCase{
            "GdchServiceAccountInDefaultUniverse",
            MakeGDCHServiceAccountCredentials(
                "{}", "https://test-only-audience.example.com"),
            std::nullopt, false},
        CredentialsUniverseTestCase{
            "GdchServiceAccountInOtherUniverse",
            MakeGDCHServiceAccountCredentials(
                "{}", "https://test-only-audience.example.com"),
            "ud.net", false},
        // Credentials that failed to load are passed on like any other. Each
        // export then fails with the stored error.
        CredentialsUniverseTestCase{
            "ErrorCredentialsInOtherUniverse",
            internal::MakeErrorCredentials(Status(StatusCode::kPermissionDenied,
                                                  "test-only")),
            "ud.net", true}),
    [](::testing::TestParamInfo<CredentialsUniverseTestCase> const& info) {
      return info.param.test_name;
    });

/// @test Verify the universe domain of a service account key is also checked
/// when the application names the key file instead of passing its contents.
TEST_F(GrpcMetricsExporterConfigTest, ReadsUniverseDomainOfServiceAccountFile) {
  storage::testing::TempFile const key_file(
      WithUniverseDomain(ServiceAccountKey(), "ud.net"));
  Options const client_options =
      EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentialsFromFile(key_file.name()));

  std::optional<ExporterConfig> const same_universe = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(Options(client_options)
                             .set<internal::UniverseDomainOption>("ud.net")));
  EXPECT_TRUE(same_universe.has_value());

  std::optional<ExporterConfig> const default_universe =
      MakeMeterProviderConfig(EmptyResource(),
                              DefaultOptionsGrpc(client_options));
  EXPECT_FALSE(default_universe.has_value());
}

struct DefaultCredentialsTestCase {
  std::string test_name;
  // The contents of the file `GOOGLE_APPLICATION_CREDENTIALS` names. When
  // unset, the variable names a file that does not exist.
  std::optional<std::string> adc_contents;
  // An application-supplied `UniverseDomainOption`. The client uses the
  // default endpoint of that universe.
  std::optional<std::string> universe_domain;
  // An application-supplied `storage::ProjectIdOption`.
  std::optional<std::string> project_option;
  // The monitoring project. When unset, no exporter is expected.
  std::optional<std::string> expected_project;
};

class GrpcMetricsExporterDefaultCredentialsTest
    : public GrpcMetricsExporterConfigTest,
      public ::testing::WithParamInterface<DefaultCredentialsTestCase> {};

/// @test Verify default credentials resolved from an Application Default
/// Credentials file use the file's project and `universe_domain` with default
/// resource labels, and disable export when the file cannot be used by gRPC's
/// Google Default credentials.
TEST_P(GrpcMetricsExporterDefaultCredentialsTest,
       FollowsApplicationDefaultCredentialsFile) {
  DefaultCredentialsTestCase const& test_case = GetParam();
  if (test_case.adc_contents) {
    SetApplicationDefaultCredentials(*test_case.adc_contents);
  } else {
    SetMissingApplicationDefaultCredentials();
  }
  Options client_options = EnabledOptions();
  if (test_case.universe_domain) {
    client_options.set<internal::UniverseDomainOption>(
        *test_case.universe_domain);
  }
  if (test_case.project_option) {
    client_options.set<storage::ProjectIdOption>(*test_case.project_option);
  }
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      HostResource(), DefaultOptionsGrpc(client_options));
  if (!test_case.expected_project) {
    EXPECT_FALSE(config.has_value());
    return;
  }
  ASSERT_TRUE(config.has_value());
  EXPECT_EQ(config->project, Project(*test_case.expected_project));
  EXPECT_THAT(MonitoredResourceLabels(*config),
              AllOf(Contains(Pair("location", "global")),
                    Contains(Pair("cloud_platform", "unknown")),
                    Contains(Pair("host_id", "unknown"))));
}

INSTANTIATE_TEST_SUITE_P(
    GrpcMetricsExporter, GrpcMetricsExporterDefaultCredentialsTest,
    ::testing::Values(
        DefaultCredentialsTestCase{"ServiceAccount", ServiceAccountKey().dump(),
                                   std::nullopt, std::nullopt,
                                   "project-id-credentials"},
        DefaultCredentialsTestCase{"ServiceAccountWithProjectOption",
                                   ServiceAccountKey().dump(), std::nullopt,
                                   "project-id-option", "project-id-option"},
        DefaultCredentialsTestCase{
            "ServiceAccountForOtherUniverse",
            WithUniverseDomain(ServiceAccountKey(), "ud.net"), "ud.net",
            std::nullopt, "project-id-credentials"},
        DefaultCredentialsTestCase{
            "ServiceAccountForOtherUniverseInDefaultUniverse",
            WithUniverseDomain(ServiceAccountKey(), "ud.net"), std::nullopt,
            "project-id-option", std::nullopt},
        DefaultCredentialsTestCase{
            "ServiceAccountWithoutUniverseInOtherUniverse",
            ServiceAccountKey().dump(), "ud.net", "project-id-option",
            std::nullopt},
        DefaultCredentialsTestCase{"AuthorizedUser",
                                   AuthorizedUserFile().dump(), std::nullopt,
                                   "project-id-option", "project-id-option"},
        // gcloud user credentials name no project, and the host's is not
        // used for them.
        DefaultCredentialsTestCase{"AuthorizedUserWithoutProject",
                                   AuthorizedUserFile().dump(), std::nullopt,
                                   std::nullopt, std::nullopt},
        DefaultCredentialsTestCase{
            "AuthorizedUserForOtherUniverseInDefaultUniverse",
            WithUniverseDomain(AuthorizedUserFile(), "ud.net"), std::nullopt,
            "project-id-option", std::nullopt},
        DefaultCredentialsTestCase{"ExternalAccount",
                                   ExternalAccountFile().dump(), std::nullopt,
                                   "project-id-option", "project-id-option"},
        DefaultCredentialsTestCase{
            "ExternalAccountForOtherUniverseInDefaultUniverse",
            WithUniverseDomain(ExternalAccountFile(), "ud.net"), std::nullopt,
            "project-id-option", std::nullopt},
        DefaultCredentialsTestCase{
            "UnsupportedType",
            nlohmann::json{{"type", "impersonated_service_account"}}.dump(),
            std::nullopt, "project-id-option", std::nullopt},
        DefaultCredentialsTestCase{"NotJson", "not-json", std::nullopt,
                                   "project-id-option", std::nullopt},
        DefaultCredentialsTestCase{"MissingFile", std::nullopt, std::nullopt,
                                   "project-id-option", std::nullopt}),
    [](::testing::TestParamInfo<DefaultCredentialsTestCase> const& info) {
      return info.param.test_name;
    });

/// @test Verify the log line for a skipped exporter names the host of the
/// endpoint and the universe domain it was compared with.
TEST_F(GrpcMetricsExporterConfigTest, LogsEndpointHostAndUniverseDomain) {
  ScopedLog log;
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject()
              .set<UnifiedCredentialsOption>(AccessTokenCredentials())
              .set<EndpointOption>("storage.ud.net:443")));
  EXPECT_FALSE(config.has_value());
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("metrics export is disabled"),
                             HasSubstr("endpoint_host=storage.ud.net,"),
                             HasSubstr("universe_domain=googleapis.com"))));
}

/// @test Verify the disabled-export log line records `(unrecognized)` instead
/// of the raw endpoint string when the endpoint has no extractable host.
TEST_F(GrpcMetricsExporterConfigTest, LogsPlaceholderForUnrecognizedEndpoint) {
  ScopedLog log;
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject()
              .set<UnifiedCredentialsOption>(AccessTokenCredentials())
              .set<EndpointOption>("unix:/tmp/test-only-storage.sock")));
  EXPECT_FALSE(config.has_value());
  std::vector<std::string> const lines = log.ExtractLines();
  EXPECT_THAT(lines,
              Contains(AllOf(HasSubstr("metrics export is disabled"),
                             HasSubstr("endpoint_host=(unrecognized)"))));
  EXPECT_THAT(lines, Each(Not(HasSubstr("test-only-storage.sock"))));
}

/// @test Verify the log line for credentials of another universe names both
/// universe domains.
TEST_F(GrpcMetricsExporterConfigTest, LogsCredentialsUniverseDomain) {
  ScopedLog log;
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
              MakeServiceAccountCredentials(
                  WithUniverseDomain(ServiceAccountKey(), "ud.net")))));
  EXPECT_FALSE(config.has_value());
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("metrics export is disabled"),
                             HasSubstr("credentials_universe_domain=ud.net"),
                             HasSubstr("universe_domain=googleapis.com"))));
}

/// @test Verify the log line says so when the universe domain of the
/// credentials cannot be determined.
TEST_F(GrpcMetricsExporterConfigTest, LogsUnknownCredentialsUniverseDomain) {
  ScopedLog log;
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
              MakeServiceAccountCredentials("not-json"))));
  EXPECT_FALSE(config.has_value());
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("metrics export is disabled"),
                     HasSubstr("cannot determine the universe domain of the "
                               "credentials"))));
}

/// @test Verify the log line reports unsupported credentials when export is
/// disabled for API key or GDCH credentials.
TEST_F(GrpcMetricsExporterConfigTest, LogsUnsupportedCredentials) {
  for (std::shared_ptr<Credentials> const& credentials : {
           MakeApiKeyCredentials("test-only-api-key"),
           MakeGDCHServiceAccountCredentials(
               "{}", "https://test-only-audience.example.com"),
       }) {
    ScopedLog log;
    std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
        EmptyResource(),
        DefaultOptionsGrpc(
            EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
                credentials)));
    EXPECT_FALSE(config.has_value());
    EXPECT_THAT(log.ExtractLines(),
                Contains(AllOf(HasSubstr("metrics export is disabled"),
                               HasSubstr("unsupported credentials"))));
  }
}

/// @test Verify nothing is logged about a skipped exporter when the exporter
/// is created.
TEST_F(GrpcMetricsExporterConfigTest, DoesNotLogWhenExporting) {
  ScopedLog log;
  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      EmptyResource(),
      DefaultOptionsGrpc(
          EnabledOptionsWithProject().set<UnifiedCredentialsOption>(
              AccessTokenCredentials())));
  EXPECT_TRUE(config.has_value());
  EXPECT_THAT(log.ExtractLines(),
              Each(Not(HasSubstr("metrics export is disabled"))));
}

struct ResourceDetectionTestCase {
  std::string test_name;
  // The options the application sets on the client.
  Options client_options;
};

class GrpcMetricsExporterResourceDetectionTest
    : public GrpcMetricsExporterConfigTest,
      public ::testing::WithParamInterface<ResourceDetectionTestCase> {};

/// @test Verify `EnableGrpcMetrics()` skips GCP resource detection when the
/// client's options disable metrics export.
TEST_P(GrpcMetricsExporterResourceDetectionTest, SkipsDetectionWithoutExport) {
  int detections = 0;
  EnableGrpcMetrics(GetParam().client_options, [&detections] {
    ++detections;
    return HostResource();
  });
  EXPECT_EQ(detections, 0);
}

INSTANTIATE_TEST_SUITE_P(
    GrpcMetricsExporter, GrpcMetricsExporterResourceDetectionTest,
    ::testing::Values(
        ResourceDetectionTestCase{
            "MetricsDisabled",
            Unified(AccessTokenCredentials())
                .set<storage_experimental::EnableGrpcMetricsOption>(false)},
        ResourceDetectionTestCase{
            "InsecureCredentials",
            Unified(MakeInsecureCredentials())
                .set<storage_experimental::EnableGrpcMetricsOption>(true)},
        ResourceDetectionTestCase{
            "EndpointOutsideUniverseDomain",
            Unified(AccessTokenCredentials())
                .set<storage_experimental::EnableGrpcMetricsOption>(true)
                .set<EndpointOption>("storage.ud.net")},
        ResourceDetectionTestCase{
            "CredentialsForOtherUniverse",
            Unified(ServiceAccountFor("ud.net"))
                .set<storage_experimental::EnableGrpcMetricsOption>(true)},
        ResourceDetectionTestCase{
            "OTelMetricsInsecureCredentials",
            Unified(MakeInsecureCredentials())
                .set<storage_experimental::EnableGrpcMetricsOption>(false)
                .set<storage_experimental::EnableOTelMetricsOption>(true)},
        ResourceDetectionTestCase{
            "OTelMetricsEndpointOutsideUniverseDomain",
            Unified(AccessTokenCredentials())
                .set<storage_experimental::EnableGrpcMetricsOption>(false)
                .set<storage_experimental::EnableOTelMetricsOption>(true)
                .set<EndpointOption>("storage.ud.net")},
        ResourceDetectionTestCase{
            "ApiKeyCredentials",
            Unified(MakeApiKeyCredentials("test-only-api-key"))
                .set<storage_experimental::EnableGrpcMetricsOption>(true)},
        ResourceDetectionTestCase{
            "OTelDebugMetricsCredentialsForOtherUniverse",
            Unified(ServiceAccountFor("ud.net"))
                .set<storage_experimental::EnableGrpcMetricsOption>(false)
                .set<storage_experimental::EnableOTelDebugMetricsOption>(
                    true)}),
    [](::testing::TestParamInfo<ResourceDetectionTestCase> const& info) {
      return info.param.test_name;
    });

/// @test Verify the emulator endpoint disables export and skips resource
/// detection even when `GCP_STORAGE_CPP_ENABLE_OTEL_METRICS` and
/// `GCP_STORAGE_CPP_ENABLE_OTEL_DEBUG_METRICS` are enabled.
TEST_F(GrpcMetricsExporterConfigTest,
       SkipsExportForEmulatorWithOTelMetricsEnv) {
  ScopedEnvironment const emulator(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", "localhost:1234");
  ScopedEnvironment const otel_metrics("GCP_STORAGE_CPP_ENABLE_OTEL_METRICS",
                                       "true");
  ScopedEnvironment const otel_debug_metrics(
      "GCP_STORAGE_CPP_ENABLE_OTEL_DEBUG_METRICS", "true");
  Options const client_options =
      Options{}.set<storage::ProjectIdOption>("project-id-option");

  int detections = 0;
  EnableGrpcMetrics(client_options, [&detections] {
    ++detections;
    return HostResource();
  });
  EXPECT_EQ(detections, 0);

  std::optional<ExporterConfig> const config = MakeMeterProviderConfig(
      HostResource(), DefaultOptionsGrpc(client_options));
  EXPECT_FALSE(config.has_value());
}

/// @test Verify OTel-only metrics configurations (`EnableGrpcMetricsOption`
/// disabled with `EnableOTelMetricsOption` or `EnableOTelDebugMetricsOption`
/// enabled) forward client connection options, select non-host resource
/// attributes for key-file credentials, and enforce universe-domain matching.
TEST_F(GrpcMetricsExporterConfigTest,
       OTelOnlyMetricsApplyClientOptionsAndResourceMode) {
  std::shared_ptr<Credentials> const credentials = ServiceAccount();
  Options const base_options =
      Options{}
          .set<storage_experimental::EnableGrpcMetricsOption>(false)
          .set<UnifiedCredentialsOption>(credentials)
          .set<CARootsFilePathOption>("/test-only/ca-certificates.pem")
          .set<ProxyOption>(
              ProxyConfig().set_hostname("proxy.example.com").set_port("3128"));

  for (Options const& enabled :
       {Options(base_options)
            .set<storage_experimental::EnableOTelMetricsOption>(true),
        Options(base_options)
            .set<storage_experimental::EnableOTelDebugMetricsOption>(true)}) {
    std::optional<ExporterConfig> const config =
        MakeMeterProviderConfig(HostResource(), DefaultOptionsGrpc(enabled));
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->enable_grpc_plugin);
    EXPECT_EQ(config->project, Project("project-id-credentials"));
    EXPECT_THAT(MonitoredResourceLabels(*config),
                AllOf(Contains(Pair("project_id", "project-id-credentials")),
                      Contains(Pair("location", "global")),
                      Contains(Pair("cloud_platform", "unknown")),
                      Contains(Pair("host_id", "unknown"))));
    Options const& exporter = config->exporter_connection_options;
    EXPECT_THAT(exporter.get<UnifiedCredentialsOption>(), Eq(credentials));
    EXPECT_EQ(exporter.get<CARootsFilePathOption>(),
              "/test-only/ca-certificates.pem");
    EXPECT_EQ(exporter.get<ProxyOption>().hostname(), "proxy.example.com");

    EXPECT_FALSE(MakeMeterProviderConfig(
                     HostResource(),
                     DefaultOptionsGrpc(Options(enabled).set<EndpointOption>(
                         "storage.ud.net")))
                     .has_value());
    EXPECT_FALSE(
        MakeMeterProviderConfig(
            HostResource(),
            DefaultOptionsGrpc(Options(enabled).set<UnifiedCredentialsOption>(
                ServiceAccountFor("ud.net"))))
            .has_value());
  }
}

/// @test Verify the resource is detected once for a client using metadata
/// credentials, and the exporter is created. The authority is unique to this
/// test because the registry of exporters is process-wide.
TEST_F(GrpcMetricsExporterConfigTest, DetectsResourceOnceWhenExporting) {
  std::string const authority = "test-only-detects-resource.example.com";
  int detections = 0;
  EnableGrpcMetrics(
      EnabledOptionsWithProject()
          .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials())
          .set<AuthorityOption>(authority),
      [&detections] {
        ++detections;
        return HostResource();
      });
  ScopedProviderShutdown const provider_shutdown(FindMeterProvider(authority));
  EXPECT_EQ(detections, 1);
  EXPECT_THAT(FindMeterProvider(authority), NotNull());
}

/// @test Verify `EnableGrpcMetrics()` skips GCP resource detection and still
/// creates an exporter when the client uses non-host credentials.
TEST_F(GrpcMetricsExporterConfigTest,
       SkipsResourceDetectionForNonHostCredentials) {
  std::string const authority =
      "test-only-skips-detection-non-host.example.com";
  int detections = 0;
  EnableGrpcMetrics(EnabledOptionsWithProject()
                        .set<UnifiedCredentialsOption>(AccessTokenCredentials())
                        .set<AuthorityOption>(authority),
                    [&detections] {
                      ++detections;
                      return HostResource();
                    });
  ScopedProviderShutdown const provider_shutdown(FindMeterProvider(authority));
  EXPECT_EQ(detections, 0);
  EXPECT_THAT(FindMeterProvider(authority), NotNull());
}

/// @test Verify clients sharing an authority reuse the first client's
/// `MeterProvider` even when later clients supply different credentials.
TEST_F(GrpcMetricsExporterConfigTest,
       ReusesFirstExporterPerAuthorityAcrossCredentials) {
  std::string const authority =
      "test-only-reuses-provider-across-credentials.example.com";
  EnableGrpcMetrics(EnabledOptionsWithProject()
                        .set<UnifiedCredentialsOption>(AccessTokenCredentials())
                        .set<AuthorityOption>(authority),
                    [] { return HostResource(); });
  std::shared_ptr<opentelemetry::metrics::MeterProvider> const first =
      FindMeterProvider(authority);
  ScopedProviderShutdown const provider_shutdown(first);
  ASSERT_THAT(first, NotNull());

  EnableGrpcMetrics(EnabledOptions()
                        .set<UnifiedCredentialsOption>(ServiceAccount())
                        .set<AuthorityOption>(authority),
                    [] { return HostResource(); });
  EXPECT_THAT(FindMeterProvider(authority), Eq(first));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
