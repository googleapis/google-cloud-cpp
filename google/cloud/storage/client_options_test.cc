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

#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/setenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::UnorderedElementsAre;

namespace {
class ClientOptionsTest : public ::testing::Test {
 public:
  ClientOptionsTest()
      : enable_tracing_("CLOUD_STORAGE_ENABLE_TRACING", {}),
        endpoint_("CLOUD_STORAGE_EMULATOR_ENDPOINT", {}),
        old_endpoint_("CLOUD_STORAGE_TESTBENCH_ENDPOINT", {}),
        generator_(std::random_device{}()) {}

  std::string CreateRandomFileName() {
    // When running on the internal Google CI systems we cannot write to the
    // local directory. GTest has a good temporary directory in that case.
    return google::cloud::internal::PathAppend(
        ::testing::TempDir(),
        google::cloud::internal::Sample(
            generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789") +
            ".json");
  }

 protected:
  ScopedEnvironment enable_tracing_;
  ScopedEnvironment endpoint_;
  ScopedEnvironment old_endpoint_;
  google::cloud::internal::DefaultPRNG generator_;
};

TEST_F(ClientOptionsTest, EndpointsDefault) {
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                           {});
  auto options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_EQ("https://storage.googleapis.com",
            options.get<RestEndpointOption>());
  EXPECT_EQ("https://iamcredentials.googleapis.com/v1",
            internal::IamEndpoint(options));
}

TEST_F(ClientOptionsTest, EndpointsOverride) {
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                           {});
  auto options = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<RestEndpointOption>("http://127.0.0.1.nip.io:1234"));
  EXPECT_EQ("http://127.0.0.1.nip.io:1234", options.get<RestEndpointOption>());
  EXPECT_EQ("https://iamcredentials.googleapis.com/v1",
            internal::IamEndpoint(options));
}

TEST_F(ClientOptionsTest, EndpointsEmulator) {
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                           "http://localhost:1234");
  auto options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_EQ("http://localhost:1234", options.get<RestEndpointOption>());
  EXPECT_EQ("http://localhost:1234/iamapi", internal::IamEndpoint(options));
}

TEST_F(ClientOptionsTest, OldEndpointsEmulator) {
  google::cloud::testing_util::UnsetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
  testing_util::ScopedEnvironment endpoint("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                           "http://localhost:1234");
  auto options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_EQ("http://localhost:1234", options.get<RestEndpointOption>());
  EXPECT_EQ("http://localhost:1234/iamapi", internal::IamEndpoint(options));
}

TEST_F(ClientOptionsTest, DefaultOptions) {
  auto o = internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_EQ("https://storage.googleapis.com", o.get<RestEndpointOption>());

  // Verify any set values are respected overridden.
  o = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<RestEndpointOption>("https://private.googleapis.com"));
  EXPECT_EQ("https://private.googleapis.com", o.get<RestEndpointOption>());

  o = internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
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

TEST_F(ClientOptionsTest, IncorporatesUniverseDomain) {
  auto o = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<google::cloud::internal::UniverseDomainOption>(
          "my-ud.net"));
  EXPECT_EQ(o.get<RestEndpointOption>(), "https://storage.my-ud.net");
  EXPECT_EQ(o.get<IamEndpointOption>(), "https://iamcredentials.my-ud.net/v1");
}

TEST_F(ClientOptionsTest, IncorporatesUniverseDomainEnvVar) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto o = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<google::cloud::internal::UniverseDomainOption>(
          "ud-option.net"));
  EXPECT_EQ(o.get<RestEndpointOption>(), "https://storage.ud-env-var.net");
  EXPECT_EQ(o.get<IamEndpointOption>(),
            "https://iamcredentials.ud-env-var.net/v1");
}

TEST_F(ClientOptionsTest, CustomEndpointOverridesUniverseDomain) {
  ScopedEnvironment ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", "ud-env-var.net");

  auto o = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
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

TEST_F(ClientOptionsTest, HttpVersion) {
  namespace rest = ::google::cloud::rest_internal;
  auto const options = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<storage_experimental::HttpVersionOption>("2.0"));
  EXPECT_EQ("2.0", options.get<rest::HttpVersionOption>());
}

TEST_F(ClientOptionsTest, CAPathOption) {
  namespace rest = ::google::cloud::rest_internal;
  auto const options = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<internal::CAPathOption>("test-only"));
  EXPECT_EQ("test-only", options.get<rest::CAPathOption>());
}

TEST_F(ClientOptionsTest, LoggingWithoutEnv) {
  ScopedEnvironment env_common("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                               absl::nullopt);
  ScopedEnvironment env("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto const options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_FALSE(options.has<LoggingComponentsOption>());
}

TEST_F(ClientOptionsTest, LoggingWithEnv) {
  ScopedEnvironment env_common("GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                               absl::nullopt);
  ScopedEnvironment env("CLOUD_STORAGE_ENABLE_TRACING", "rpc,http");
  auto const options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_THAT(options.get<LoggingComponentsOption>(),
              UnorderedElementsAre("rpc", "http"));
}

TEST_F(ClientOptionsTest, TracingWithoutEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING",
                        absl::nullopt);
  auto options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_FALSE(options.get<OpenTelemetryTracingOption>());

  options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(),
                               Options{}.set<OpenTelemetryTracingOption>(true));
  EXPECT_TRUE(options.get<OpenTelemetryTracingOption>());
}

TEST_F(ClientOptionsTest, TracingWithEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", "ON");
  auto const options = internal::DefaultOptions(
      oauth2::CreateAnonymousCredentials(),
      Options{}.set<OpenTelemetryTracingOption>(false));
  EXPECT_TRUE(options.get<OpenTelemetryTracingOption>());
}

TEST_F(ClientOptionsTest, ProjectIdWithoutEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_PROJECT", absl::nullopt);
  auto const options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_FALSE(options.has<ProjectIdOption>());
}

TEST_F(ClientOptionsTest, ProjecIdtWithEnv) {
  ScopedEnvironment env("GOOGLE_CLOUD_PROJECT", "my-project");
  auto const options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), {});
  EXPECT_EQ("my-project", options.get<ProjectIdOption>());
}

TEST_F(ClientOptionsTest, OverrideWithRestInternal) {
  namespace rest = ::google::cloud::rest_internal;
  auto const options =
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(),
                               Options{}
                                   .set<rest::ConnectionPoolSizeOption>(1234)
                                   .set<ConnectionPoolSizeOption>(2345));
  EXPECT_EQ(1234, options.get<rest::ConnectionPoolSizeOption>());
  EXPECT_EQ(2345, options.get<ConnectionPoolSizeOption>());
}

TEST_F(ClientOptionsTest, Timeouts) {
  EXPECT_EQ(std::chrono::seconds(42),
            internal::DefaultOptions(oauth2::CreateAnonymousCredentials(),
                                     Options{}.set<TransferStallTimeoutOption>(
                                         std::chrono::seconds(42)))
                .get<DownloadStallTimeoutOption>());

  EXPECT_EQ(std::chrono::seconds(7),
            internal::DefaultOptions(
                oauth2::CreateAnonymousCredentials(),
                Options{}
                    .set<TransferStallTimeoutOption>(std::chrono::seconds(42))
                    .set<DownloadStallTimeoutOption>(std::chrono::seconds(7)))
                .get<DownloadStallTimeoutOption>());

  EXPECT_EQ(std::chrono::seconds(7),
            internal::DefaultOptions(oauth2::CreateAnonymousCredentials(),
                                     Options{}.set<DownloadStallTimeoutOption>(
                                         std::chrono::seconds(7)))
                .get<DownloadStallTimeoutOption>());

  EXPECT_NE(
      std::chrono::seconds(0),
      internal::DefaultOptions(oauth2::CreateAnonymousCredentials(), Options{})
          .get<DownloadStallTimeoutOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
