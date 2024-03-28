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
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

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
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = MakeGrpcClient(TestOptions());
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("GrpcStub", "StorageConnectionImpl"));
}

#include "google/cloud/internal/disable_deprecation_warnings.inc"
TEST(GrpcPluginTest, MostConfigValuesCreatesGrpc) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  // Unless the config is set to "none" we want to create the gRPC stub.
  for (auto const* config : {"", "metadata", "media", "anything-but-none"}) {
    auto client = MakeGrpcClient(
        TestOptions().set<storage_experimental::GrpcPluginOption>(config));
    auto impl = ClientImplDetails::GetConnection(client);
    ASSERT_THAT(impl, NotNull());
    EXPECT_THAT(impl->InspectStackStructure(),
                ElementsAre("GrpcStub", "StorageConnectionImpl"));
  }
}

TEST(GrpcPluginTest, EnvironmentOverrides) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", "none");
  auto client = MakeGrpcClient(
      TestOptions().set<storage_experimental::GrpcPluginOption>("metadata"));
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "StorageConnectionImpl"));
}

TEST(GrpcPluginTest, UnsetConfigCreatesMetadata) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = MakeGrpcClient(TestOptions());
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("GrpcStub", "StorageConnectionImpl"));
}

TEST(GrpcPluginTest, NoneConfigCreatesCurl) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = MakeGrpcClient(
      TestOptions().set<storage_experimental::GrpcPluginOption>("none"));
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "StorageConnectionImpl"));
}

TEST(GrpcPluginTest, HybridUsesGrpcBufferOptions) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = MakeGrpcClient(
      TestOptions().set<storage_experimental::GrpcPluginOption>("media"));
  EXPECT_GE(
      client.raw_client()->options().get<storage::UploadBufferSizeOption>(),
      32 * 1024 * 1024L);
}

TEST(GrpcPluginTest, BackwardsCompatibilityShims) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  namespace gcs_ex = ::google::cloud::storage_experimental;
  auto client = gcs_ex::DefaultGrpcClient(
      TestOptions().set<gcs_ex::GrpcPluginOption>("none"));
  auto impl = ClientImplDetails::GetConnection(client);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "StorageConnectionImpl"));
}
#include "google/cloud/internal/diagnostics_pop.inc"

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
