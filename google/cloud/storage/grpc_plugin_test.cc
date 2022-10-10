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
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/storage/internal/rest_client.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ClientImplDetails;
using ::google::cloud::storage::internal::GrpcClient;
using ::google::cloud::storage::internal::RestClient;
using ::google::cloud::storage::internal::RetryClient;
using ::google::cloud::storage_internal::HybridClient;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsNull;
using ::testing::NotNull;

Options TestOptions() {
  return Options{}
      .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
      .set<storage::RestEndpointOption>("http://localhost:1")
      .set<EndpointOption>("localhost:1");
}

TEST(GrpcPluginTest, MetadataConfigCreatesGrpc) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client =
      DefaultGrpcClient(TestOptions().set<GrpcPluginOption>("metadata"));
  auto const* const retry =
      dynamic_cast<RetryClient*>(ClientImplDetails::GetRawClient(client).get());
  ASSERT_THAT(retry, NotNull());
  auto const* const grpc =
      dynamic_cast<GrpcClient const*>(retry->client().get());
  ASSERT_THAT(grpc, NotNull());
}

TEST(GrpcPluginTest, EnvironmentOverrides) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", "none");
  auto client =
      DefaultGrpcClient(TestOptions().set<GrpcPluginOption>("metadata"));
  auto const* const retry =
      dynamic_cast<RetryClient*>(ClientImplDetails::GetRawClient(client).get());
  ASSERT_THAT(retry, NotNull());
  auto const* const grpc = dynamic_cast<GrpcClient*>(retry->client().get());
  ASSERT_THAT(grpc, IsNull());
}

TEST(GrpcPluginTest, UnsetConfigCreatesCurl) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = DefaultGrpcClient(Options{});
  auto const* const retry =
      dynamic_cast<RetryClient*>(ClientImplDetails::GetRawClient(client).get());
  ASSERT_THAT(retry, NotNull());
  auto const* const rest = dynamic_cast<RestClient*>(retry->client().get());
  ASSERT_THAT(rest, NotNull());
}

TEST(GrpcPluginTest, NoneConfigCreatesCurl) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = DefaultGrpcClient(TestOptions().set<GrpcPluginOption>("none"));
  auto const* const retry =
      dynamic_cast<RetryClient*>(ClientImplDetails::GetRawClient(client).get());
  ASSERT_THAT(retry, NotNull());
  auto const* const rest = dynamic_cast<RestClient*>(retry->client().get());
  ASSERT_THAT(rest, NotNull());
}

TEST(GrpcPluginTest, MediaConfigCreatesHybrid) {
  // Explicitly disable logging, which may be enabled by our CI builds.
  auto logging =
      ScopedEnvironment("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  auto config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto client = DefaultGrpcClient(TestOptions().set<GrpcPluginOption>("media"));
  auto const* const retry =
      dynamic_cast<RetryClient*>(ClientImplDetails::GetRawClient(client).get());
  ASSERT_THAT(retry, NotNull());
  auto const* const hybrid = dynamic_cast<HybridClient*>(retry->client().get());
  ASSERT_THAT(hybrid, NotNull());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
