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

#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/str_cat.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class MockGcpDetector : public google::cloud::internal::GcpDetector {
 public:
  MOCK_METHOD(bool, IsGoogleCloudBios, (), (override));
  MOCK_METHOD(bool, IsGoogleCloudServerless, (), (override));
};

using ::google::cloud::internal::GrpcGoogleDefaultTlsOnlyOption;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

google::cloud::Options TestOptions() {
  return Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
}

std::shared_ptr<google::cloud::internal::GcpDetector> MakeNotOnGcpDetector() {
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));
  return mock_detector;
}

/// Returns the name of the `UnifiedCredentialsOption` in @p options, or an
/// empty string if the option is not set.
std::string CredentialsName(Options const& options) {
  if (!options.has<UnifiedCredentialsOption>()) return {};
  ::google::cloud::testing_util::TestCredentialsVisitor visitor;
  ::google::cloud::internal::CredentialsVisitor::dispatch(
      *options.get<UnifiedCredentialsOption>(), visitor);
  return visitor.name;
}

/// Returns the logging components of the `UnifiedCredentialsOption` in
/// @p options, or an empty set if the option is not set.
std::set<std::string> CredentialsLoggingComponents(Options const& options) {
  if (!options.has<UnifiedCredentialsOption>()) return {};
  ::google::cloud::testing_util::TestCredentialsVisitor visitor;
  ::google::cloud::internal::CredentialsVisitor::dispatch(
      *options.get<UnifiedCredentialsOption>(), visitor);
  return visitor.options.get<LoggingComponentsOption>();
}

/// Returns whether OpenTelemetry tracing is enabled on the
/// `UnifiedCredentialsOption` in @p options.
bool CredentialsOpenTelemetryTracing(Options const& options) {
  if (!options.has<UnifiedCredentialsOption>()) return false;
  ::google::cloud::testing_util::TestCredentialsVisitor visitor;
  ::google::cloud::internal::CredentialsVisitor::dispatch(
      *options.get<UnifiedCredentialsOption>(), visitor);
  return visitor.options.get<OpenTelemetryTracingOption>();
}

TEST(DefaultOptionsGrpc, DefaultOptionsGrpcChannelCount) {
  using ::google::cloud::GrpcNumChannelsOption;
  struct TestCase {
    std::string endpoint;
    int lower_bound;
    int upper_bound;
  } cases[] = {
      {"storage.googleapis.com", 4, std::numeric_limits<int>::max()},
      {"google-c2p:///storage.googleapis.com", 1, 1},
      {"google-c2p-experimental:///storage.googleapis.com", 1, 1},
      {"google-c2p:///storage-direct.googleapis.com?force-xds", 1, 1},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with " + test.endpoint);
    auto opts =
        DefaultOptionsGrpc(TestOptions().set<EndpointOption>(test.endpoint));
    auto const count = opts.get<GrpcNumChannelsOption>();
    EXPECT_LE(test.lower_bound, count);
    EXPECT_GE(test.upper_bound, count);

    auto override = DefaultOptionsGrpc(TestOptions()
                                           .set<EndpointOption>(test.endpoint)
                                           .set<GrpcNumChannelsOption>(42));
    EXPECT_EQ(42, override.get<GrpcNumChannelsOption>());
  }
}

/// @test Verify the REST emulator endpoints do not change gRPC credentials.
TEST(DefaultOptionsGrpc, RestEmulatorDoesNotChangeGrpcCredentials) {
  for (char const* var : {"CLOUD_STORAGE_EMULATOR_ENDPOINT",
                          "CLOUD_STORAGE_TESTBENCH_ENDPOINT"}) {
    SCOPED_TRACE(var);
    ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
    ScopedEnvironment old_emulator("CLOUD_STORAGE_TESTBENCH_ENDPOINT", {});
    ScopedEnvironment testbench(
        "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
    ScopedEnvironment rest(var, "http://localhost:1234");
    Options const options =
        DefaultOptionsGrpc(Options{}, MakeNotOnGcpDetector());
    EXPECT_EQ(CredentialsName(options), "GoogleDefaultCredentialsConfig");
  }
}

/// @test Verify the gRPC testbench still uses insecure credentials when the
/// REST emulator endpoint is also set.
TEST(DefaultOptionsGrpc, GrpcTestbenchStillInsecureWithRestEmulator) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                             "http://localhost:1234");
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", "localhost:9999");
  ScopedEnvironment preserve(
      "GOOGLE_CLOUD_CPP_STORAGE_TESTING_PRESERVE_CREDENTIALS", {});
  Options const options = DefaultOptionsGrpc(Options{}, MakeNotOnGcpDetector());
  EXPECT_EQ(CredentialsName(options), "InsecureCredentialsConfig");
}

/// @test Verify explicit credentials are kept when the REST emulator endpoint
/// is set.
TEST(DefaultOptionsGrpc, GrpcKeepsExplicitCredentialsWithRestEmulator) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                             "http://localhost:1234");
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  Options const options = DefaultOptionsGrpc(
      Options{}.set<UnifiedCredentialsOption>(MakeAccessTokenCredentials(
          "token1", std::chrono::system_clock::now() + std::chrono::hours(1))),
      MakeNotOnGcpDetector());
  EXPECT_EQ(CredentialsName(options), "AccessTokenConfig");
}

/// @test Verify the default gRPC credentials preserve explicit auth options.
TEST(DefaultOptionsGrpc, DefaultCredentialsKeepAuthOptions) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment storage_tracing("CLOUD_STORAGE_ENABLE_TRACING", {});
  ScopedEnvironment common_tracing("GOOGLE_CLOUD_CPP_ENABLE_TRACING", {});
  ScopedEnvironment otel_tracing("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", {});
  Options const options =
      DefaultOptionsGrpc(Options{}
                             .set<LoggingComponentsOption>({"auth"})
                             .set<OpenTelemetryTracingOption>(true),
                         MakeNotOnGcpDetector());
  EXPECT_EQ(CredentialsName(options), "GoogleDefaultCredentialsConfig");
  EXPECT_THAT(CredentialsLoggingComponents(options), ElementsAre("auth"));
  EXPECT_TRUE(CredentialsOpenTelemetryTracing(options));
}

/// @test Verify `CLOUD_STORAGE_ENABLE_TRACING` configures the client logging
/// only. The default credentials use `GOOGLE_CLOUD_CPP_ENABLE_TRACING`.
TEST(DefaultOptionsGrpc, DefaultCredentialsIgnoreStorageTracingEnvironment) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment storage_tracing("CLOUD_STORAGE_ENABLE_TRACING",
                                    "raw-client,http");
  ScopedEnvironment common_tracing("GOOGLE_CLOUD_CPP_ENABLE_TRACING", "auth");
  ScopedEnvironment otel_tracing("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", {});
  Options const options = DefaultOptionsGrpc(Options{}, MakeNotOnGcpDetector());
  EXPECT_THAT(options.get<LoggingComponentsOption>(),
              UnorderedElementsAre("raw-client", "http"));
  EXPECT_EQ(CredentialsName(options), "GoogleDefaultCredentialsConfig");
  EXPECT_THAT(CredentialsLoggingComponents(options), ElementsAre("auth"));
  EXPECT_FALSE(CredentialsOpenTelemetryTracing(options));
}

/// @test Verify `GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING` reaches the default
/// gRPC credentials.
TEST(DefaultOptionsGrpc, DefaultCredentialsUseOpenTelemetryTracingEnvironment) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment otel_tracing("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING",
                                 "ON");
  Options const default_options =
      DefaultOptionsGrpc(Options{}, MakeNotOnGcpDetector());
  EXPECT_TRUE(default_options.get<OpenTelemetryTracingOption>());
  EXPECT_EQ(CredentialsName(default_options), "GoogleDefaultCredentialsConfig");
  EXPECT_TRUE(CredentialsOpenTelemetryTracing(default_options));

  Options const overridden_options = DefaultOptionsGrpc(
      Options{}.set<OpenTelemetryTracingOption>(false), MakeNotOnGcpDetector());
  EXPECT_TRUE(overridden_options.get<OpenTelemetryTracingOption>());
  EXPECT_EQ(CredentialsName(overridden_options),
            "GoogleDefaultCredentialsConfig");
  EXPECT_TRUE(CredentialsOpenTelemetryTracing(overridden_options));
}

/// @test Verify `GrpcCredentialOption` is honored and not shadowed by default
/// `UnifiedCredentialsOption`, even when the REST emulator endpoint is set.
TEST(DefaultOptionsGrpc, GrpcCredentialOptionIsHonored) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                             "http://localhost:1234");
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  Options const options = DefaultOptionsGrpc(
      Options{}.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()),
      MakeNotOnGcpDetector());
  EXPECT_FALSE(options.has<UnifiedCredentialsOption>());
  EXPECT_TRUE(options.has<GrpcCredentialOption>());
}

/// @test Verify explicit `UnifiedCredentialsOption` takes precedence when both
/// `UnifiedCredentialsOption` and `GrpcCredentialOption` are set.
TEST(DefaultOptionsGrpc,
     UnifiedCredentialsOptionTakesPrecedenceOverGrpcCredentialOption) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  Options const options = DefaultOptionsGrpc(
      Options{}
          .set<UnifiedCredentialsOption>(MakeAccessTokenCredentials(
              "token1",
              std::chrono::system_clock::now() + std::chrono::hours(1)))
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()),
      MakeNotOnGcpDetector());
  EXPECT_EQ(CredentialsName(options), "AccessTokenConfig");
}

/// @test Verify the gRPC testbench overrides `GrpcCredentialOption` unless
/// `GOOGLE_CLOUD_CPP_STORAGE_TESTING_PRESERVE_CREDENTIALS` is set.
TEST(DefaultOptionsGrpc,
     GrpcTestbenchOverridesGrpcCredentialOptionUnlessPreserved) {
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", "localhost:9999");
  {
    ScopedEnvironment preserve(
        "GOOGLE_CLOUD_CPP_STORAGE_TESTING_PRESERVE_CREDENTIALS", {});
    Options const options = DefaultOptionsGrpc(
        Options{}.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()),
        MakeNotOnGcpDetector());
    EXPECT_EQ(CredentialsName(options), "InsecureCredentialsConfig");
  }
  {
    ScopedEnvironment preserve(
        "GOOGLE_CLOUD_CPP_STORAGE_TESTING_PRESERVE_CREDENTIALS", "1");
    Options const options = DefaultOptionsGrpc(
        Options{}.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()),
        MakeNotOnGcpDetector());
    EXPECT_FALSE(options.has<UnifiedCredentialsOption>());
    EXPECT_TRUE(options.has<GrpcCredentialOption>());
  }
}

TEST(DefaultOptionsGrpc, DefaultEndpointsCloudPath) {
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  auto options = DefaultOptionsGrpc(Options{}, mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

TEST(DefaultOptionsGrpc, DefaultEndpointsDirectPath) {
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(true));

  auto options = DefaultOptionsGrpc(Options{}, mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  // Regular DirectPath relies on ALTS, which only Google Default credentials
  // select.
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

struct DirectPathCredentialsTestCase {
  std::string test_name;
  std::function<Options()> make_options;
  bool expected_direct_path;
};

class DefaultEndpointsDirectPathCredentialsTest
    : public ::testing::TestWithParam<DirectPathCredentialsTestCase> {};

/// @test Verify that on GCE, `DefaultOptionsGrpc()` defaults to the
/// `google-c2p` DirectPath endpoint only when the configured credentials can
/// use ALTS (Google Default credentials or an opaque `GrpcCredentialOption`).
/// Non-ALTS credential types fall back to the multi-channel CloudPath default.
TEST_P(DefaultEndpointsDirectPathCredentialsTest, GatesEndpointOnCredentials) {
  DirectPathCredentialsTestCase const& test_case = GetParam();
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment interconnect(
      "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", {});
  ScopedEnvironment universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN", {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  Options const options =
      DefaultOptionsGrpc(test_case.make_options(), mock_detector);
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
  if (test_case.expected_direct_path) {
    EXPECT_EQ(options.get<EndpointOption>(),
              "google-c2p:///storage.googleapis.com");
    EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  } else {
    EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
    EXPECT_GE(options.get<GrpcNumChannelsOption>(), 4);
  }
}

INSTANTIATE_TEST_SUITE_P(
    DefaultOptionsGrpc, DefaultEndpointsDirectPathCredentialsTest,
    ::testing::Values(
        DirectPathCredentialsTestCase{"DefaultCredentials",
                                      [] { return Options{}; }, true},
        DirectPathCredentialsTestCase{
            "GoogleDefaultCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeGoogleDefaultCredentials());
            },
            true},
        DirectPathCredentialsTestCase{
            "ComputeEngineCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeComputeEngineCredentials());
            },
            false},
        DirectPathCredentialsTestCase{
            "ComputeEngineCredentialsWithCaRoots",
            [] {
              return Options{}
                  .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials())
                  .set<CARootsFilePathOption>("/path/to/roots.pem");
            },
            false},
        DirectPathCredentialsTestCase{
            "ComputeEngineCredentialsWithOptionCaRoots",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeComputeEngineCredentials(
                      Options{}.set<CARootsFilePathOption>(
                          "/path/to/roots.pem")));
            },
            false},
        DirectPathCredentialsTestCase{
            "AccessTokenCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeAccessTokenCredentials("test-token",
                                             std::chrono::system_clock::now() +
                                                 std::chrono::hours(1)));
            },
            false},
        DirectPathCredentialsTestCase{
            "ServiceAccountCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeServiceAccountCredentials("{}"));
            },
            false},
        DirectPathCredentialsTestCase{
            "ImpersonateServiceAccountCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeImpersonateServiceAccountCredentials(
                      MakeGoogleDefaultCredentials(),
                      "target@example.iam.gserviceaccount.com"));
            },
            false},
        DirectPathCredentialsTestCase{
            "ApiKeyCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeApiKeyCredentials("test-api-key"));
            },
            false},
        DirectPathCredentialsTestCase{
            "ExternalAccountCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeExternalAccountCredentials("{}"));
            },
            false},
        DirectPathCredentialsTestCase{
            "GDCHServiceAccountCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeGDCHServiceAccountCredentials(
                      "{}", "https://storage.googleapis.com"));
            },
            false},
        DirectPathCredentialsTestCase{
            "InsecureCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  MakeInsecureCredentials());
            },
            false},
        DirectPathCredentialsTestCase{
            "ErrorCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  google::cloud::internal::MakeErrorCredentials(
                      Status(StatusCode::kInternal, "test-error")));
            },
            false},
        DirectPathCredentialsTestCase{
            "UserAccountCredentials",
            [] {
              return Options{}.set<UnifiedCredentialsOption>(
                  google::cloud::internal::MakeUserAccountCredentials("{}"));
            },
            false},
        // `GrpcCredentialOption` cannot be inspected for ALTS capability and
        // preserves existing `google-c2p` behavior.
        DirectPathCredentialsTestCase{
            "GrpcCredentialOptionOnly",
            [] {
              return Options{}.set<GrpcCredentialOption>(
                  grpc::InsecureChannelCredentials());
            },
            true},
        // `UnifiedCredentialsOption` takes precedence over
        // `GrpcCredentialOption` when both are present.
        DirectPathCredentialsTestCase{
            "UnifiedNonAltsOverridesGrpcCredentialOption",
            [] {
              return Options{}
                  .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials())
                  .set<GrpcCredentialOption>(
                      grpc::InsecureChannelCredentials());
            },
            false},
        DirectPathCredentialsTestCase{
            "UnifiedGoogleDefaultOverridesGrpcCredentialOption",
            [] {
              return Options{}
                  .set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials())
                  .set<GrpcCredentialOption>(
                      grpc::InsecureChannelCredentials());
            },
            true},
        // A null `UnifiedCredentialsOption` pointer must not be dereferenced
        // and does not qualify for ALTS, even when paired with
        // `GrpcCredentialOption` (`UnifiedCredentialsOption` takes precedence).
        DirectPathCredentialsTestCase{
            "NullUnifiedCredentialsOption",
            [] { return Options{}.set<UnifiedCredentialsOption>(nullptr); },
            false},
        DirectPathCredentialsTestCase{
            "NullUnifiedOverridesGrpcCredentialOption",
            [] {
              return Options{}
                  .set<UnifiedCredentialsOption>(nullptr)
                  .set<GrpcCredentialOption>(
                      grpc::InsecureChannelCredentials());
            },
            false}),
    [](::testing::TestParamInfo<DirectPathCredentialsTestCase> const& info) {
      return info.param.test_name;
    });

/// @test Verify serverless-only GCP detection applies the same ALTS credential
/// gate as GCE BIOS detection.
TEST(DefaultOptionsGrpc, DefaultEndpointsServerlessGatesOnCredentials) {
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment interconnect(
      "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", {});
  ScopedEnvironment universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN", {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(true));

  Options const default_creds = DefaultOptionsGrpc(
      Options{}.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials()),
      mock_detector);
  EXPECT_EQ(default_creds.get<EndpointOption>(),
            "google-c2p:///storage.googleapis.com");
  EXPECT_EQ(default_creds.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_EQ(default_creds.get<GrpcNumChannelsOption>(), 1);
  EXPECT_FALSE(default_creds.get<GrpcGoogleDefaultTlsOnlyOption>());

  Options const compute_creds = DefaultOptionsGrpc(
      Options{}.set<UnifiedCredentialsOption>(MakeComputeEngineCredentials()),
      mock_detector);
  EXPECT_EQ(compute_creds.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(compute_creds.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_GE(compute_creds.get<GrpcNumChannelsOption>(), 4);
  EXPECT_FALSE(compute_creds.get<GrpcGoogleDefaultTlsOnlyOption>());
}

struct ExplicitDirectPathEndpointTestCase {
  std::string test_name;
  std::string endpoint;
};

class ExplicitDirectPathEndpointWithNonAltsCredsTest
    : public ::testing::TestWithParam<ExplicitDirectPathEndpointTestCase> {};

/// @test Verify explicit `google-c2p` endpoints are preserved even when paired
/// with non-ALTS credentials, because the library only gates the default
/// endpoint it selects.
TEST_P(ExplicitDirectPathEndpointWithNonAltsCredsTest, PreservesEndpoint) {
  ExplicitDirectPathEndpointTestCase const& test_case = GetParam();
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment interconnect(
      "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", {});
  ScopedEnvironment universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN", {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}
          .set<EndpointOption>(test_case.endpoint)
          .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials()),
      mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(), test_case.endpoint);
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

INSTANTIATE_TEST_SUITE_P(
    DefaultOptionsGrpc, ExplicitDirectPathEndpointWithNonAltsCredsTest,
    ::testing::Values(
        ExplicitDirectPathEndpointTestCase{
            "StandardScheme", "google-c2p:///storage.googleapis.com"},
        ExplicitDirectPathEndpointTestCase{
            "ExperimentalScheme",
            "google-c2p-experimental:///storage.googleapis.com"},
        ExplicitDirectPathEndpointTestCase{
            "OtherHost", "google-c2p:///other-host.googleapis.com"}),
    [](::testing::TestParamInfo<ExplicitDirectPathEndpointTestCase> const&
           info) { return info.param.test_name; });

/// @test Verify non-ALTS credentials remain on CloudPath off GCP.
TEST(DefaultOptionsGrpc, DefaultEndpointsCloudPathWithNonAltsCredentials) {
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment interconnect(
      "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", {});
  ScopedEnvironment universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN", {});

  Options const options = DefaultOptionsGrpc(
      Options{}.set<UnifiedCredentialsOption>(MakeComputeEngineCredentials()),
      MakeNotOnGcpDetector());
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_GE(options.get<GrpcNumChannelsOption>(), 4);
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify DirectPath over Interconnect uses TLS rather than ALTS and
/// therefore still selects the Interconnect endpoint with non-ALTS credentials,
/// while falling back to CloudPath on GCE when gRPC does not support
/// `force-xds`.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectWithNonAltsCredentials) {
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", {});
  ScopedEnvironment universe_domain("GOOGLE_CLOUD_UNIVERSE_DOMAIN", {});
  {
    ScopedEnvironment interconnect(
        "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", {});
    auto mock_detector = std::make_shared<MockGcpDetector>();
    EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
    EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

    Options const options = DefaultOptionsGrpc(
        Options{}
            .set<storage_experimental::DirectPathXdsOverInterconnectOption>(
                true)
            .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials()),
        mock_detector, /*grpc_supports_force_xds=*/true);
    EXPECT_EQ(options.get<EndpointOption>(),
              "google-c2p:///storage-direct.googleapis.com?force-xds");
    EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
    EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
    EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
  }
  {
    ScopedEnvironment interconnect(
        "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", "true");
    auto mock_detector = std::make_shared<MockGcpDetector>();
    EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
    EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

    Options const options = DefaultOptionsGrpc(
        Options{}.set<UnifiedCredentialsOption>(MakeComputeEngineCredentials()),
        mock_detector, /*grpc_supports_force_xds=*/true);
    EXPECT_EQ(options.get<EndpointOption>(),
              "google-c2p:///storage-direct.googleapis.com?force-xds");
    EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
    EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
    EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
  }
  {
    ScopedEnvironment interconnect(
        "GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT", {});
    auto mock_detector = std::make_shared<MockGcpDetector>();
    EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
        .WillRepeatedly(Return(false));

    Options const options = DefaultOptionsGrpc(
        Options{}
            .set<storage_experimental::DirectPathXdsOverInterconnectOption>(
                true)
            .set<UnifiedCredentialsOption>(MakeComputeEngineCredentials()),
        mock_detector, /*grpc_supports_force_xds=*/false);
    EXPECT_TRUE(
        options
            .get<storage_experimental::DirectPathXdsOverInterconnectOption>());
    EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
    EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
    EXPECT_GE(options.get<GrpcNumChannelsOption>(), 4);
    EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
  }
}

/// @test Verify the option defaults to disabled, and that the resolved default
/// is reported when neither the option nor the environment variable is set.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectDefault) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  auto options = DefaultOptionsGrpc(Options{}, mock_detector);
  EXPECT_TRUE(
      options.has<storage_experimental::DirectPathXdsOverInterconnectOption>());
  EXPECT_FALSE(
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>());
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify enabling the option off-GCE targets the DirectPath resolver,
/// sets the Interconnect `:authority`, uses a single channel, and switches the
/// default credentials to TLS.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectOption) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          true),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage-direct.googleapis.com?force-xds");
  // The service identifies DirectPath over Interconnect traffic by this
  // `:authority`.
  EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify the environment variable enables the feature when the option is
/// unset, i.e. the zero-code rollout path for operators.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectEnvVar) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        "true");
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(Options{}, mock_detector,
                                             /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage-direct.googleapis.com?force-xds");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify explicitly disabling the option off-GCE keeps the client on
/// CloudPath, including the CloudPath multi-channel defaults.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectDisabled) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  auto options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          false),
      mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_GE(options.get<GrpcNumChannelsOption>(), 4);
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify a caller-supplied endpoint wins over the DirectPath target, so
/// testbench and private-endpoint setups keep working.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectUserEndpointOverride) {
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  auto options = DefaultOptionsGrpc(
      Options{}
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true)
          .set<EndpointOption>("custom-endpoint")
          .set<AuthorityOption>("custom-authority"),
      mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(), "custom-endpoint");
  EXPECT_EQ(options.get<AuthorityOption>(), "custom-authority");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify a caller-supplied universe domain wins over the DirectPath
/// target, as DirectPath is only available in the default universe.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectUniverseDomainOverride) {
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  auto options = DefaultOptionsGrpc(
      Options{}
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true)
          .set<internal::UniverseDomainOption>("my-ud.net"),
      mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(), "storage.my-ud.net");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.my-ud.net");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify `GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT=true`
/// overrides the option, consistent with other environment variables.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectEnvVarOverridesOption) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        "true");
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          false),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_TRUE(
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>());
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage-direct.googleapis.com?force-xds");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify `GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT=false`
/// disables the feature even when the option is set, providing an opt-out that
/// does not require rebuilding the application.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectEnvVarFalseOverridesOption) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        "false");
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  auto options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          true),
      mock_detector);
  EXPECT_FALSE(
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>());
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_GE(options.get<GrpcNumChannelsOption>(), 4);
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify a caller-supplied authority is preserved when the DirectPath
/// target is selected; `storage-direct.googleapis.com` is only the default.
/// The credentials still switch to TLS: the authority does not change what
/// transport security the Interconnect endpoint accepts.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectPreservesCustomAuthority) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}
          .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true)
          .set<AuthorityOption>("custom-authority"),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage-direct.googleapis.com?force-xds");
  EXPECT_EQ(options.get<AuthorityOption>(), "custom-authority");
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify an application-supplied Interconnect endpoint also gets the
/// Interconnect `:authority`, as the service keys on the authority regardless
/// of how the endpoint was configured.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectExplicitEndpoint) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<EndpointOption>(
          "google-c2p:///storage-direct.googleapis.com?force-xds"),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage-direct.googleapis.com?force-xds");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify an application-supplied Interconnect endpoint keeps the
/// standard `:authority` and the standard credentials when gRPC ignores
/// `force-xds`, as the channel then uses regular DirectPath or CloudPath
/// rather than Interconnect.
TEST(DefaultOptionsGrpc,
     DirectPathOverInterconnectExplicitEndpointUnsupportedGrpc) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<EndpointOption>(
          "google-c2p:///storage-direct.googleapis.com?force-xds"),
      mock_detector, /*grpc_supports_force_xds=*/false);
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify an application-supplied `force-xds` endpoint for any host other
/// than `storage-direct.googleapis.com` or its preprod counterpart keeps the
/// standard `:authority`. The service recognizes the Interconnect authority
/// only for those hosts. The credentials still switch to TLS, because ALTS is
/// unavailable to Interconnect clients whatever host they dial.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectExplicitEndpointOtherHost) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options =
      DefaultOptionsGrpc(Options{}.set<EndpointOption>(
                             "google-c2p:///storage.googleapis.com?force-xds"),
                         mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage.googleapis.com?force-xds");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

struct InterconnectAuthorityTestCase {
  std::string test_name;
  std::string endpoint;
  // The value of `GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT`.
  std::optional<std::string> env_var;
  // An application-supplied `AuthorityOption`.
  std::optional<std::string> authority;
  // An application-supplied `UniverseDomainOption`.
  std::optional<std::string> universe_domain;
  bool grpc_supports_force_xds;
  std::string expected_authority;
  bool expected_tls_only;
};

class DirectPathOverInterconnectAuthorityTest
    : public ::testing::TestWithParam<InterconnectAuthorityTestCase> {};

/// @test Verify the default `:authority` for application-supplied endpoints
/// that target the production or preprod Interconnect host, or only resemble
/// one. The service identifies Interconnect traffic by the authority, so an
/// endpoint for either Interconnect host must default to that same host, while
/// an application-supplied endpoint is never rewritten and an explicit
/// `AuthorityOption` always wins.
TEST_P(DirectPathOverInterconnectAuthorityTest, DefaultsAuthority) {
  InterconnectAuthorityTestCase const& test_case = GetParam();
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        test_case.env_var);
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  auto input = Options{}.set<EndpointOption>(test_case.endpoint);
  if (test_case.authority) input.set<AuthorityOption>(*test_case.authority);
  if (test_case.universe_domain) {
    input.set<internal::UniverseDomainOption>(*test_case.universe_domain);
  }
  Options const options = DefaultOptionsGrpc(std::move(input), mock_detector,
                                             test_case.grpc_supports_force_xds);
  EXPECT_EQ(options.get<EndpointOption>(), test_case.endpoint);
  EXPECT_EQ(options.get<AuthorityOption>(), test_case.expected_authority);
  EXPECT_EQ(options.get<GrpcGoogleDefaultTlsOnlyOption>(),
            test_case.expected_tls_only);
}

INSTANTIATE_TEST_SUITE_P(
    DefaultOptionsGrpc, DirectPathOverInterconnectAuthorityTest,
    ::testing::Values(
        InterconnectAuthorityTestCase{
            "Preprod",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds",
            std::nullopt, std::nullopt, std::nullopt, true,
            "storage-preprod-direct.sandbox.googleapis.com", true},
        InterconnectAuthorityTestCase{
            "PreprodExperimentalScheme",
            "google-c2p-experimental:///"
            "storage-preprod-direct.sandbox.googleapis.com?force-xds",
            std::nullopt, std::nullopt, std::nullopt, true,
            "storage-preprod-direct.sandbox.googleapis.com", true},
        // The variable governs only the endpoint the library selects.
        InterconnectAuthorityTestCase{
            "PreprodEnvVarFalse",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds",
            "false", std::nullopt, std::nullopt, true,
            "storage-preprod-direct.sandbox.googleapis.com", true},
        InterconnectAuthorityTestCase{
            "PreprodCustomAuthority",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds",
            std::nullopt, "custom-authority", std::nullopt, true,
            "custom-authority", true},
        // gRPC < 1.85 ignores `force-xds`, so the channel is not Interconnect.
        InterconnectAuthorityTestCase{
            "PreprodUnsupportedGrpc",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds",
            std::nullopt, std::nullopt, std::nullopt, false,
            "storage.googleapis.com", false},
        // Without `force-xds` this is a regular DirectPath target.
        InterconnectAuthorityTestCase{
            "PreprodWithoutForceXds",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com",
            std::nullopt, std::nullopt, std::nullopt, true,
            "storage.googleapis.com", false},
        // The non-direct preprod host is not rewritten to the Interconnect
        // host, matching how the production host is handled.
        InterconnectAuthorityTestCase{
            "PreprodNonDirectHost",
            "google-c2p:///storage-preprod.sandbox.googleapis.com?force-xds",
            std::nullopt, std::nullopt, std::nullopt, true,
            "storage.googleapis.com", true},
        // An explicit Interconnect endpoint takes its authority from the
        // endpoint, not from the universe domain, for both hosts alike.
        InterconnectAuthorityTestCase{
            "UniverseDomain",
            "google-c2p:///storage-direct.googleapis.com?force-xds",
            std::nullopt, std::nullopt, "my-ud.net", true,
            "storage-direct.googleapis.com", true},
        InterconnectAuthorityTestCase{
            "PreprodUniverseDomain",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds",
            std::nullopt, std::nullopt, "my-ud.net", true,
            "storage-preprod-direct.sandbox.googleapis.com", true}),
    [](::testing::TestParamInfo<InterconnectAuthorityTestCase> const& info) {
      return info.param.test_name;
    });

struct InterconnectHostDelimiterTestCase {
  std::string test_name;
  std::string endpoint;
  std::string expected_authority;
};

class DirectPathOverInterconnectHostDelimiterTest
    : public ::testing::TestWithParam<InterconnectHostDelimiterTestCase> {};

/// @test Verify the host of an application-supplied `force-xds` endpoint is
/// compared against the Interconnect hosts up to the first delimiter (`:`,
/// `/`, `?`, `#`), so a port, path or fragment does not hide a match, and a
/// look-alike host that merely starts with one of them does not match.
TEST_P(DirectPathOverInterconnectHostDelimiterTest, DetectsExactHostOnly) {
  InterconnectHostDelimiterTestCase const& test_case = GetParam();
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options =
      DefaultOptionsGrpc(Options{}.set<EndpointOption>(test_case.endpoint),
                         mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(), test_case.endpoint);
  EXPECT_EQ(options.get<AuthorityOption>(), test_case.expected_authority);
}

INSTANTIATE_TEST_SUITE_P(
    DefaultOptionsGrpc, DirectPathOverInterconnectHostDelimiterTest,
    ::testing::Values(
        InterconnectHostDelimiterTestCase{
            "Port", "google-c2p:///storage-direct.googleapis.com:443?force-xds",
            "storage-direct.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "Path", "google-c2p:///storage-direct.googleapis.com/?force-xds",
            "storage-direct.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "FragmentAfterQuery",
            "google-c2p:///storage-direct.googleapis.com?force-xds#frag",
            "storage-direct.googleapis.com"},
        // Per RFC 3986 a `?` inside the fragment is not a query, so this is a
        // plain DirectPath target and keeps the standard authority.
        InterconnectHostDelimiterTestCase{
            "FragmentBeforeQuery",
            "google-c2p:///storage-direct.googleapis.com#frag?force-xds",
            "storage.googleapis.com"},
        // The host starts with `storage-direct.googleapis.com` but is a
        // different host.
        InterconnectHostDelimiterTestCase{
            "LookAlikeHost",
            "google-c2p:///storage-direct.googleapis.com.example.com?force-xds",
            "storage.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "PreprodPort",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com:443"
            "?force-xds",
            "storage-preprod-direct.sandbox.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "PreprodPath",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com/"
            "?force-xds",
            "storage-preprod-direct.sandbox.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "PreprodFragmentAfterQuery",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "?force-xds#frag",
            "storage-preprod-direct.sandbox.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "PreprodFragmentBeforeQuery",
            "google-c2p:///storage-preprod-direct.sandbox.googleapis.com"
            "#frag?force-xds",
            "storage.googleapis.com"},
        InterconnectHostDelimiterTestCase{
            "PreprodLookAlikeHost",
            "google-c2p:///"
            "storage-preprod-direct.sandbox.googleapis.com.example.com"
            "?force-xds",
            "storage.googleapis.com"}),
    [](::testing::TestParamInfo<InterconnectHostDelimiterTestCase> const&
           info) { return info.param.test_name; });

/// @test Verify the `google-c2p-experimental` scheme selects the Interconnect
/// `:authority` like `google-c2p` does; the library treats both as DirectPath.
TEST(DefaultOptionsGrpc,
     DirectPathOverInterconnectExplicitEndpointExperimentalScheme) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<EndpointOption>(
          "google-c2p-experimental:///storage-direct.googleapis.com?force-xds"),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
  EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify `GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT=false`
/// leaves an application-supplied Interconnect endpoint, and therefore its
/// `:authority` and credentials, unchanged. The variable and the option govern
/// only the endpoint the library selects, and the authority and credentials
/// must match the endpoint actually in use.
TEST(DefaultOptionsGrpc,
     DirectPathOverInterconnectExplicitEndpointEnvVarFalse) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        "false");
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<EndpointOption>(
          "google-c2p:///storage-direct.googleapis.com?force-xds"),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_FALSE(
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>());
  EXPECT_EQ(options.get<EndpointOption>(),
            "google-c2p:///storage-direct.googleapis.com?force-xds");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage-direct.googleapis.com");
  EXPECT_TRUE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify universe domain precedence also holds when the domain comes
/// from `GOOGLE_CLOUD_UNIVERSE_DOMAIN` rather than an option.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectUniverseDomainEnvVar) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "my-ud.net");
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  auto options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          true),
      mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(), "storage.my-ud.net");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.my-ud.net");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify the testbench endpoint wins over the Interconnect option and
/// keeps the (insecure) testbench credentials.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectTestbenchOverride) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", "localhost:9999");
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);

  Options const options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          true),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(), "localhost:9999");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

/// @test Verify the credentials option is recomputed from the effective
/// endpoint rather than inherited from the input options, so a value carried
/// over from another client cannot select TLS on a CloudPath endpoint.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectTlsOnlyRecomputed) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  Options const options = DefaultOptionsGrpc(
      Options{}.set<GrpcGoogleDefaultTlsOnlyOption>(true), mock_detector,
      /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
}

class DirectPathOverInterconnectInvalidEnvVarTest
    : public ::testing::TestWithParam<std::tuple<std::string, bool>> {};

/// @test Verify unrecognized environment variable values are ignored and leave
/// the option as the application configured it. Each value is tried against
/// both an enabled and a disabled option, so that a case-insensitive match
/// (for example treating `"TRUE"` as `"true"`) would be detected.
TEST_P(DirectPathOverInterconnectInvalidEnvVarTest, InvalidValueIgnored) {
  auto const& [env_value, configured] = GetParam();
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        env_value);

  auto mock_detector = std::make_shared<MockGcpDetector>();
  if (configured) {
    EXPECT_CALL(*mock_detector, IsGoogleCloudBios()).Times(0);
    EXPECT_CALL(*mock_detector, IsGoogleCloudServerless()).Times(0);
  } else {
    EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
        .WillRepeatedly(Return(false));
  }

  Options const options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          configured),
      mock_detector, /*grpc_supports_force_xds=*/true);
  EXPECT_EQ(
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>(),
      configured);
  EXPECT_EQ(options.get<EndpointOption>(),
            configured ? "google-c2p:///storage-direct.googleapis.com?force-xds"
                       : "storage.googleapis.com");
  EXPECT_EQ(options.get<GrpcGoogleDefaultTlsOnlyOption>(), configured);
}

INSTANTIATE_TEST_SUITE_P(
    DefaultOptionsGrpc, DirectPathOverInterconnectInvalidEnvVarTest,
    ::testing::Combine(::testing::Values("1", "0", "TRUE", "False", "yes", ""),
                       ::testing::Bool()),
    ([](::testing::TestParamInfo<
         DirectPathOverInterconnectInvalidEnvVarTest::ParamType> const& info) {
      auto const& val = std::get<0>(info.param);
      auto const configured = std::get<1>(info.param);
      return absl::StrCat(val.empty() ? "Empty" : val, "_",
                          configured ? "OptionTrue" : "OptionFalse");
    }));

struct UnsupportedGrpcTestCase {
  std::string test_name;
  bool on_gce;
  bool use_env_var;
};

class DirectPathOverInterconnectUnsupportedGrpcTest
    : public ::testing::TestWithParam<UnsupportedGrpcTestCase> {};

/// @test Verify that when gRPC does not support `force-xds`, enabling
/// DirectPath over Interconnect (via option or environment variable) falls back
/// to the standard endpoint defaults while preserving the requested option.
TEST_P(DirectPathOverInterconnectUnsupportedGrpcTest, FallsBackToDefaults) {
  UnsupportedGrpcTestCase const& test_case = GetParam();
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        test_case.use_env_var
                            ? std::make_optional<std::string>("true")
                            : std::nullopt);
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(test_case.on_gce));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  Options input_options;
  if (!test_case.use_env_var) {
    input_options
        .set<storage_experimental::DirectPathXdsOverInterconnectOption>(true);
  }
  Options const options =
      DefaultOptionsGrpc(std::move(input_options), mock_detector,
                         /*grpc_supports_force_xds=*/false);
  EXPECT_TRUE(
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>());
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.googleapis.com");
  EXPECT_FALSE(options.get<GrpcGoogleDefaultTlsOnlyOption>());
  if (test_case.on_gce) {
    EXPECT_EQ(options.get<EndpointOption>(),
              "google-c2p:///storage.googleapis.com");
    EXPECT_EQ(options.get<GrpcNumChannelsOption>(), 1);
  } else {
    EXPECT_EQ(options.get<EndpointOption>(), "storage.googleapis.com");
    EXPECT_GE(options.get<GrpcNumChannelsOption>(), 4);
  }
}

INSTANTIATE_TEST_SUITE_P(
    DefaultOptionsGrpc, DirectPathOverInterconnectUnsupportedGrpcTest,
    ::testing::Values(UnsupportedGrpcTestCase{"OffGce_Option", false, false},
                      UnsupportedGrpcTestCase{"OffGce_EnvVar", false, true},
                      UnsupportedGrpcTestCase{"OnGce_Option", true, false},
                      UnsupportedGrpcTestCase{"OnGce_EnvVar", true, true}),
    [](::testing::TestParamInfo<UnsupportedGrpcTestCase> const& info) {
      return info.param.test_name;
    });

TEST(DefaultOptionsGrpc, EndpointOptionsOverrideDefaults) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto options = DefaultOptionsGrpc(
      Options{}
          .set<EndpointOption>("from-option")
          .set<AuthorityOption>("host-from-option")
          .set<internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "from-option");
  EXPECT_EQ(options.get<AuthorityOption>(), "host-from-option");
}

TEST(DefaultOptionsGrpc, EnvVarsOverrideOptionsAndDefaults) {
  ScopedEnvironment e("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                      "from-testbench-env");
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto options = DefaultOptionsGrpc(
      Options{}
          .set<EndpointOption>("from-ep-option")
          .set<internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "from-testbench-env");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.ud-env-var.net");
}

TEST(DefaultOptionsGrpc, IncorporatesUniverseDomain) {
  auto options = DefaultOptionsGrpc(
      Options{}.set<internal::UniverseDomainOption>("my-ud.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "storage.my-ud.net");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.my-ud.net");
}

TEST(DefaultOptionsGrpc, IncorporatesUniverseDomainEnvVar) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto options = DefaultOptionsGrpc(
      Options{}.set<internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(options.get<EndpointOption>(), "storage.ud-env-var.net");
  EXPECT_EQ(options.get<AuthorityOption>(), "storage.ud-env-var.net");
}

TEST(DefaultOptionsGrpc, DefaultOptionsUploadBuffer) {
  auto const with_defaults =
      DefaultOptionsGrpc(Options{}).get<storage::UploadBufferSizeOption>();
  EXPECT_GE(with_defaults, 32 * 1024 * 1024L);

  auto const with_override =
      DefaultOptionsGrpc(
          Options{}.set<storage::UploadBufferSizeOption>(256 * 1024))
          .get<storage::UploadBufferSizeOption>();
  EXPECT_EQ(with_override, 256 * 1024L);
}

TEST(DefaultOptionsGrpc, GrpcEnableMetricsIsSafe) {
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(0, 1, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(0, 65, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 62, 0));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 62, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 62, 1));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 63, 0));
  EXPECT_FALSE(GrpcEnableMetricsIsSafe(1, 64, 0));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 63, 1));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 63, 2));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 64, 1));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 64, 2));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(1, 65, 0));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(2, 0, 0));
  EXPECT_TRUE(GrpcEnableMetricsIsSafe(2, 1, 0));
}

TEST(DefaultOptionsGrpc, GrpcForceXdsIsSupported) {
  EXPECT_FALSE(GrpcForceXdsIsSupported(0, 85, 0));
  EXPECT_FALSE(GrpcForceXdsIsSupported(1, 76, 0));
  EXPECT_FALSE(GrpcForceXdsIsSupported(1, 84, 0));
  EXPECT_FALSE(GrpcForceXdsIsSupported(1, 84, 9));
  EXPECT_TRUE(GrpcForceXdsIsSupported(1, 85, 0));
  EXPECT_TRUE(GrpcForceXdsIsSupported(1, 85, 1));
  EXPECT_TRUE(GrpcForceXdsIsSupported(1, 86, 0));
  EXPECT_TRUE(GrpcForceXdsIsSupported(2, 0, 0));
}

TEST(DefaultOptionsGrpc, MetricsEnabled) {
  ScopedEnvironment env("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                        std::nullopt);
  auto const options = DefaultOptionsGrpc(Options{});
  auto const expected = GrpcEnableMetricsIsSafe();
  EXPECT_EQ(options.get<storage_experimental::EnableGrpcMetricsOption>(),
            expected);
}

/// @test Verify the 2-argument `DefaultOptionsGrpc` overload delegates using
/// `GrpcForceXdsIsSupported()`.
TEST(DefaultOptionsGrpc, DirectPathOverInterconnectFollowsGrpcVersion) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT",
                        {});
  auto mock_detector = std::make_shared<MockGcpDetector>();
  EXPECT_CALL(*mock_detector, IsGoogleCloudBios())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_detector, IsGoogleCloudServerless())
      .WillRepeatedly(Return(false));

  Options const options = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          true),
      mock_detector);
  EXPECT_EQ(options.get<EndpointOption>(),
            GrpcForceXdsIsSupported()
                ? "google-c2p:///storage-direct.googleapis.com?force-xds"
                : "storage.googleapis.com");
  EXPECT_EQ(options.get<AuthorityOption>(),
            GrpcForceXdsIsSupported() ? "storage-direct.googleapis.com"
                                      : "storage.googleapis.com");
  EXPECT_EQ(options.get<GrpcGoogleDefaultTlsOnlyOption>(),
            GrpcForceXdsIsSupported());
}

TEST(DefaultOptionsGrpc, MetricsDisabled) {
  ScopedEnvironment env("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                        "test-only-unused");
  auto const options = DefaultOptionsGrpc(Options{});
  EXPECT_FALSE(options.get<storage_experimental::EnableGrpcMetricsOption>());
}

TEST(DefaultOptionsGrpc, MetricsPeriod) {
  auto const options = DefaultOptionsGrpc(Options{});
  EXPECT_GE(options.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(60));
}

TEST(DefaultOptionsGrpc, MinMetricsPeriod) {
  auto const o0 = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(0)));
  EXPECT_GT(o0.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(0));

  auto const m5 = DefaultOptionsGrpc(
      Options{}.set<storage_experimental::GrpcMetricsPeriodOption>(
          std::chrono::seconds(-5)));
  EXPECT_GT(m5.get<storage_experimental::GrpcMetricsPeriodOption>(),
            std::chrono::seconds(0));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
