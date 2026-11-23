// Copyright 2022 Google LLC
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

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <optional>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ClientImplDetails;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::ElementsAre;
using ::testing::NotNull;

Options TestOptions() {
  return Options{}
      .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
      .set<storage::RestEndpointOption>("http://localhost:1")
      .set<EndpointOption>("localhost:1");
}

TEST(GrpcPluginTest, DefaultCreatesGrpc) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", std::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", std::nullopt);
  auto client = MakeGrpcClient(TestOptions());
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("GrpcStub", "StorageConnectionImpl"));
}

TEST(GrpcPluginTest, UnsetConfigCreatesMetadata) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", std::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", std::nullopt);
  auto client = MakeGrpcClient(TestOptions());
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("GrpcStub", "StorageConnectionImpl"));
}

TEST(GrpcPluginTest, GrpcMetricsExcludedLabelsOption) {
  auto const expected =
      std::set<std::string>{"service_name", "service_version", "custom_label"};
  auto opts =
      google::cloud::Options{}
          .set<storage_experimental::GrpcMetricsExcludedLabelsOption>(expected);

  EXPECT_EQ(expected,
            opts.get<storage_experimental::GrpcMetricsExcludedLabelsOption>());
}

TEST(GrpcPluginTest, GrpcMetricsExcludedLabelsOptionEmpty) {
  auto const expected = std::set<std::string>{};
  auto opts =
      google::cloud::Options{}
          .set<storage_experimental::GrpcMetricsExcludedLabelsOption>(expected);

  EXPECT_TRUE(opts.get<storage_experimental::GrpcMetricsExcludedLabelsOption>()
                  .empty());
}

TEST(GrpcPluginTest, GrpcMetricsExcludedLabelsOptionSingle) {
  auto const expected = std::set<std::string>{"service_name"};
  auto opts =
      google::cloud::Options{}
          .set<storage_experimental::GrpcMetricsExcludedLabelsOption>(expected);

  EXPECT_EQ(
      1,
      opts.get<storage_experimental::GrpcMetricsExcludedLabelsOption>().size());
  EXPECT_EQ(expected,
            opts.get<storage_experimental::GrpcMetricsExcludedLabelsOption>());
}

TEST(GrpcPluginTest, GrpcBackgroundThreadsFactoryOptionWithTracing) {
  struct Fake : google::cloud::BackgroundThreads {
    google::cloud::CompletionQueue cq() const override { return {}; }
  };
  bool invoked = false;
  auto factory = [&invoked] {
    invoked = true;
    return std::make_unique<Fake>();
  };
  auto logging = ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", "1");
  auto client = MakeGrpcClient(
      TestOptions()
          .set<GrpcBackgroundThreadsFactoryOption>(factory)
          .set<storage_experimental::OTelSpanEnrichmentOption>(true));
  EXPECT_TRUE(invoked);
}

/// @test Verify `MakeGrpcClient()` accepts `GrpcCredentialOption` without
/// `UnifiedCredentialsOption`, including stub and IAM channel creation.
TEST(GrpcPluginTest, MakeGrpcClientWithGrpcCredentialOption) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", std::nullopt);
  ScopedEnvironment otel("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING",
                         std::nullopt);
  ScopedEnvironment config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                           std::nullopt);
  ScopedEnvironment emulator("CLOUD_STORAGE_EMULATOR_ENDPOINT", std::nullopt);
  ScopedEnvironment testbench(
      "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", std::nullopt);
  Client client = MakeGrpcClient(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<EndpointOption>("localhost:1")
          .set<GrpcNumChannelsOption>(1)
          .set<GrpcBackgroundThreadPoolSizeOption>(1)
          .set<storage_experimental::EnableGrpcMetricsOption>(false));
  std::shared_ptr<internal::StorageConnection> const impl =
      ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("GrpcStub", "StorageConnectionImpl"));
  Options const options = impl->options();
  EXPECT_FALSE(options.has<UnifiedCredentialsOption>());
  EXPECT_TRUE(options.has<GrpcCredentialOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
