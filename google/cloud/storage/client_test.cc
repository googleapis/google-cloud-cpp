// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/setenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ClientImplDetails;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::ElementsAre;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

class ObservableRetryPolicy : public LimitedErrorCountRetryPolicy {
 public:
  using LimitedErrorCountRetryPolicy::LimitedErrorCountRetryPolicy;

  std::unique_ptr<LimitedErrorCountRetryPolicy::BaseType> clone()
      const override {
    return std::unique_ptr<LimitedErrorCountRetryPolicy::BaseType>(
        new ObservableRetryPolicy(*this));
  }

  bool IsExhausted() const override {
    ++is_exhausted_call_count_;
    return LimitedErrorCountRetryPolicy::IsExhausted();
  }

  static int is_exhausted_call_count_;
};
int ObservableRetryPolicy::is_exhausted_call_count_;

class ObservableBackoffPolicy : public ExponentialBackoffPolicy {
 public:
  using ExponentialBackoffPolicy::ExponentialBackoffPolicy;

  std::unique_ptr<BackoffPolicy> clone() const override {
    return std::unique_ptr<BackoffPolicy>(new ObservableBackoffPolicy(*this));
  }

  std::chrono::milliseconds OnCompletion() override {
    ++on_completion_call_count_;
    return ExponentialBackoffPolicy::OnCompletion();
  }

  static int on_completion_call_count_;
};
int ObservableBackoffPolicy::on_completion_call_count_;

class ClientTest : public ::testing::Test {
 public:
  ClientTest()
      : enable_tracing_("CLOUD_STORAGE_ENABLE_TRACING", {}),
        endpoint_("CLOUD_STORAGE_EMULATOR_ENDPOINT", {}),
        old_endpoint_("CLOUD_STORAGE_TESTBENCH_ENDPOINT", {}),
        generator_(std::random_device{}()) {}

 protected:
  void SetUp() override {
    mock_ = std::make_shared<testing::MockClient>();
    ObservableRetryPolicy::is_exhausted_call_count_ = 0;
    ObservableBackoffPolicy::on_completion_call_count_ = 0;
  }
  void TearDown() override {
    ObservableRetryPolicy::is_exhausted_call_count_ = 0;
    ObservableBackoffPolicy::on_completion_call_count_ = 0;
    mock_.reset();
  }

  std::shared_ptr<testing::MockClient> mock_;
  ScopedEnvironment enable_tracing_;
  ScopedEnvironment endpoint_;
  ScopedEnvironment old_endpoint_;
  google::cloud::internal::DefaultPRNG generator_;
};

TEST_F(ClientTest, Equality) {
  auto a =
      storage::Client(Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeInsecureCredentials()));
  auto b =
      storage::Client(Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeInsecureCredentials()));
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(b == b);
  auto c = a;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_TRUE(a == c);
  b = std::move(a);
  EXPECT_TRUE(b == c);
}

TEST_F(ClientTest, OverrideRetryPolicy) {
  auto client = testing::ClientFromMock(mock_, ObservableRetryPolicy(3));

  // Reset the counters at the beginning of the test.

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_LE(1, ObservableRetryPolicy::is_exhausted_call_count_);
  EXPECT_EQ(0, ObservableBackoffPolicy::on_completion_call_count_);
}

TEST_F(ClientTest, OverrideBackoffPolicy) {
  using ms = std::chrono::milliseconds;
  auto client = testing::ClientFromMock(
      mock_, ObservableBackoffPolicy(ms(20), ms(100), 2.0));

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_EQ(0, ObservableRetryPolicy::is_exhausted_call_count_);
  EXPECT_LE(1, ObservableBackoffPolicy::on_completion_call_count_);
}

TEST_F(ClientTest, OverrideBothPolicies) {
  using ms = std::chrono::milliseconds;
  auto client = testing::ClientFromMock(
      mock_, ObservableBackoffPolicy(ms(20), ms(100), 2.0),
      ObservableRetryPolicy(3));

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_LE(1, ObservableRetryPolicy::is_exhausted_call_count_);
  EXPECT_LE(1, ObservableBackoffPolicy::on_completion_call_count_);
}

/// @test Verify the constructor creates the right set of StorageConnection
/// decorations.
TEST_F(ClientTest, DefaultDecoratorsRestClient) {
  ScopedEnvironment disable_grpc("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);

  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto tested =
      Client(Options{}
                 .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                 .set<LoggingComponentsOption>({}));

  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "StorageConnectionImpl"));
}

/// @test Verify the constructor creates the right set of StorageConnection
/// decorations.
TEST_F(ClientTest, LoggingDecoratorsRestClient) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment legacy("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP",
                           absl::nullopt);

  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto tested =
      Client(Options{}
                 .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                 .set<LoggingComponentsOption>({"raw-client"}));

  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "LoggingStub", "StorageConnectionImpl"));
}

using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;

TEST_F(ClientTest, OTelEnableTracing) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment legacy("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP",
                           absl::nullopt);

  // Create a client. Use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto options = Options{}
                     .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                     .set<LoggingComponentsOption>({"raw-client"});

  auto tested = Client(EnableTracing(std::move(options)));
  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());

  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "LoggingStub", "StorageConnectionImpl",
                          "TracingConnection"));
}

TEST_F(ClientTest, OTelDisableTracing) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment legacy("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP",
                           absl::nullopt);

  // Create a client. Use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto options = Options{}
                     .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                     .set<LoggingComponentsOption>({"raw-client"});

  auto tested = Client(DisableTracing(std::move(options)));
  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());

  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "LoggingStub", "StorageConnectionImpl"));
}

TEST_F(ClientTest, EndpointsDefault) {
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                           {});
  auto options = internal::DefaultOptions();
  EXPECT_EQ("https://storage.googleapis.com",
            options.get<RestEndpointOption>());
  EXPECT_EQ("https://iamcredentials.googleapis.com/v1",
            options.get<IamEndpointOption>());
}

TEST_F(ClientTest, EndpointsOverride) {
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                           {});
  auto options = internal::DefaultOptions(

      Options{}.set<RestEndpointOption>("http://127.0.0.1.nip.io:1234"));
  EXPECT_EQ("http://127.0.0.1.nip.io:1234", options.get<RestEndpointOption>());
  EXPECT_EQ("https://iamcredentials.googleapis.com/v1",
            options.get<IamEndpointOption>());
}

TEST_F(ClientTest, EndpointsEmulator) {
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                           "http://localhost:1234");
  auto options = internal::DefaultOptions();
  EXPECT_EQ("http://localhost:1234", options.get<RestEndpointOption>());
  EXPECT_EQ("http://localhost:1234/iamapi", options.get<IamEndpointOption>());
}

TEST_F(ClientTest, OldEndpointsEmulator) {
  google::cloud::testing_util::UnsetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                           "http://localhost:1234");
  auto options = internal::DefaultOptions();
  EXPECT_EQ("http://localhost:1234", options.get<RestEndpointOption>());
  EXPECT_EQ("http://localhost:1234/iamapi", options.get<IamEndpointOption>());
}

TEST_F(ClientTest, DefaultOptions) {
  auto o = internal::DefaultOptions();
  EXPECT_EQ("https://storage.googleapis.com", o.get<RestEndpointOption>());

  // Verify any set values are respected overridden.
  o = internal::DefaultOptions(
      Options{}.set<RestEndpointOption>("https://private.googleapis.com"));
  EXPECT_EQ("https://private.googleapis.com", o.get<RestEndpointOption>());

  o = internal::DefaultOptions();
  EXPECT_EQ("https://storage.googleapis.com", o.get<RestEndpointOption>());
  EXPECT_EQ("https://iamcredentials.googleapis.com/v1",
            o.get<IamEndpointOption>());

  EXPECT_EQ("v1", o.get<internal::TargetApiVersionOption>());
  EXPECT_LT(0, o.get<ConnectionPoolSizeOption>());
  EXPECT_LT(0, o.get<DownloadBufferSizeOption>());
  EXPECT_LT(0, o.get<UploadBufferSizeOption>());
  EXPECT_LE(0, o.get<MaximumSimpleUploadSizeOption>());
  EXPECT_TRUE(o.get<EnableCurlSslLockingOption>());
  EXPECT_TRUE(o.get<EnableCurlSigpipeHandlerOption>());
  EXPECT_EQ(0, o.get<MaximumCurlSocketRecvSizeOption>());
  EXPECT_EQ(0, o.get<MaximumCurlSocketSendSizeOption>());
  EXPECT_LT(std::chrono::seconds(0), o.get<TransferStallTimeoutOption>());
  EXPECT_LT(0, o.get<TransferStallMinimumRateOption>());
  EXPECT_LT(std::chrono::seconds(0), o.get<DownloadStallTimeoutOption>());
  EXPECT_LT(0, o.get<DownloadStallMinimumRateOption>());

  namespace rest = ::google::cloud::rest_internal;
  EXPECT_EQ(o.get<rest::DownloadStallTimeoutOption>(),
            o.get<DownloadStallTimeoutOption>());
  EXPECT_EQ(o.get<rest::DownloadStallMinimumRateOption>(),
            o.get<DownloadStallMinimumRateOption>());
  EXPECT_EQ(o.get<rest::TransferStallTimeoutOption>(),
            o.get<TransferStallTimeoutOption>());
  EXPECT_EQ(o.get<rest::TransferStallMinimumRateOption>(),
            o.get<TransferStallMinimumRateOption>());
  EXPECT_EQ(o.get<rest::MaximumCurlSocketRecvSizeOption>(),
            o.get<MaximumCurlSocketRecvSizeOption>());
  EXPECT_EQ(o.get<rest::MaximumCurlSocketSendSizeOption>(),
            o.get<MaximumCurlSocketSendSizeOption>());
  EXPECT_EQ(o.get<rest::ConnectionPoolSizeOption>(),
            o.get<ConnectionPoolSizeOption>());
  EXPECT_EQ(o.get<rest::EnableCurlSslLockingOption>(),
            o.get<EnableCurlSslLockingOption>());
  EXPECT_EQ(o.get<rest::EnableCurlSigpipeHandlerOption>(),
            o.get<EnableCurlSigpipeHandlerOption>());

  EXPECT_FALSE(o.has<rest::HttpVersionOption>());
  EXPECT_FALSE(o.has<rest::CAPathOption>());
}

TEST_F(ClientTest, IncorporatesUniverseDomain) {
  auto o = internal::DefaultOptions(
      Options{}.set<google::cloud::internal::UniverseDomainOption>(
          "my-ud.net"));
  EXPECT_EQ(o.get<RestEndpointOption>(), "https://storage.my-ud.net");
  EXPECT_EQ(o.get<IamEndpointOption>(), "https://iamcredentials.my-ud.net/v1");
}

TEST_F(ClientTest, IncorporatesUniverseDomainEnvVar) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto o = internal::DefaultOptions(
      Options{}.set<google::cloud::internal::UniverseDomainOption>(
          "ud-option.net"));
  EXPECT_EQ(o.get<RestEndpointOption>(), "https://storage.ud-env-var.net");
  EXPECT_EQ(o.get<IamEndpointOption>(),
            "https://iamcredentials.ud-env-var.net/v1");
}

TEST_F(ClientTest, CustomEndpointOverridesUniverseDomain) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto o = internal::DefaultOptions(
      Options{}
          .set<RestEndpointOption>("https://custom-storage.googleapis.com")
          .set<IamEndpointOption>(
              "https://custom-iamcredentials.googleapis.com/v1")
          .set<google::cloud::internal::UniverseDomainOption>("ud-option.net"));
  EXPECT_EQ(o.get<RestEndpointOption>(),
            "https://custom-storage.googleapis.com");
  EXPECT_EQ(o.get<IamEndpointOption>(),
            "https://custom-iamcredentials.googleapis.com/v1");
}

TEST_F(ClientTest, HttpVersion) {
  namespace rest = ::google::cloud::rest_internal;
  auto const options = internal::DefaultOptions(
      Options{}.set<storage_experimental::HttpVersionOption>("2.0"));
  EXPECT_EQ("2.0", options.get<rest::HttpVersionOption>());
}

TEST_F(ClientTest, CAPathOption) {
  namespace rest = ::google::cloud::rest_internal;
  auto const options = internal::DefaultOptions(
      Options{}.set<internal::CAPathOption>("test-only"));
  EXPECT_EQ("test-only", options.get<rest::CAPathOption>());
}

TEST_F(ClientTest, LoggingWithoutEnv) {
  ScopedEnvironment env_common("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                               absl::nullopt);
  ScopedEnvironment env("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto const options = internal::DefaultOptions();
  EXPECT_FALSE(options.has<LoggingComponentsOption>());
}

TEST_F(ClientTest, LoggingWithEnv) {
  ScopedEnvironment env_common("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                               absl::nullopt);
  ScopedEnvironment env("CLOUD_STORAGE_ENABLE_TRACING", "rpc,http");
  auto const options = internal::DefaultOptions();
  EXPECT_THAT(options.get<LoggingComponentsOption>(),
              UnorderedElementsAre("rpc", "http"));
}

TEST_F(ClientTest, TracingWithoutEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING",
                        absl::nullopt);
  auto options = internal::DefaultOptions();
  EXPECT_FALSE(options.get<OpenTelemetryTracingOption>());

  options =
      internal::DefaultOptions(Options{}.set<OpenTelemetryTracingOption>(true));
  EXPECT_TRUE(options.get<OpenTelemetryTracingOption>());
}

TEST_F(ClientTest, TracingWithEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", "ON");
  auto const options = internal::DefaultOptions(
      Options{}.set<OpenTelemetryTracingOption>(false));
  EXPECT_TRUE(options.get<OpenTelemetryTracingOption>());
}

TEST_F(ClientTest, ProjectIdWithoutEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto const options = internal::DefaultOptions();
  EXPECT_FALSE(options.has<ProjectIdOption>());
}

TEST_F(ClientTest, ProjectIdtWithEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_PROJECT", "my-project");
  auto const options = internal::DefaultOptions();
  EXPECT_EQ("my-project", options.get<ProjectIdOption>());
}

TEST_F(ClientTest, OverrideWithRestInternal) {
  namespace rest = ::google::cloud::rest_internal;
  auto const options =
      internal::DefaultOptions(Options{}
                                   .set<rest::ConnectionPoolSizeOption>(1234)
                                   .set<ConnectionPoolSizeOption>(2345));
  EXPECT_EQ(1234, options.get<rest::ConnectionPoolSizeOption>());
  EXPECT_EQ(2345, options.get<ConnectionPoolSizeOption>());
}

TEST_F(ClientTest, Timeouts) {
  EXPECT_EQ(std::chrono::seconds(42),
            internal::DefaultOptions(Options{}.set<TransferStallTimeoutOption>(
                                         std::chrono::seconds(42)))
                .get<DownloadStallTimeoutOption>());

  EXPECT_EQ(std::chrono::seconds(7),
            internal::DefaultOptions(
                Options{}
                    .set<TransferStallTimeoutOption>(std::chrono::seconds(42))
                    .set<DownloadStallTimeoutOption>(std::chrono::seconds(7)))
                .get<DownloadStallTimeoutOption>());

  EXPECT_EQ(std::chrono::seconds(7),
            internal::DefaultOptions(Options{}.set<DownloadStallTimeoutOption>(
                                         std::chrono::seconds(7)))
                .get<DownloadStallTimeoutOption>());

  EXPECT_NE(std::chrono::seconds(0),
            internal::DefaultOptions().get<DownloadStallTimeoutOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
