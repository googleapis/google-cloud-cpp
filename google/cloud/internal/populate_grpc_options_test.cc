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

#include "google/cloud/internal/populate_grpc_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/testing_util/credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::TestCredentialsVisitor;

TEST(PopulateGrpcOptions, Simple) {
  // Unset any environment variables
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_TRACING_OPTIONS", absl::nullopt);
  auto actual = PopulateGrpcOptions(Options{});
  EXPECT_TRUE(actual.has<GrpcTracingOptionsOption>());
  EXPECT_EQ(actual.get<GrpcTracingOptionsOption>()
                .truncate_string_field_longer_than(),
            TracingOptions{}.truncate_string_field_longer_than());

  EXPECT_TRUE(actual.has<UnifiedCredentialsOption>());
  auto const& creds = actual.get<UnifiedCredentialsOption>();
  TestCredentialsVisitor v;
  CredentialsVisitor::dispatch(*creds, v);
  EXPECT_EQ(v.name, "GoogleDefaultCredentialsConfig");
}

TEST(PopulateRestOptions, CustomCredentials) {
  auto const expiration = std::chrono::system_clock::now();
  auto creds = MakeAccessTokenCredentials("access-token", expiration);
  auto options =
      PopulateGrpcOptions(Options{}.set<UnifiedCredentialsOption>(creds));
  EXPECT_EQ(options.get<UnifiedCredentialsOption>(), creds);
}

TEST(PopulateGrpcOptions, GrpcCredentialOptionBackCompat) {
  auto creds = grpc::InsecureChannelCredentials();
  auto actual = PopulateGrpcOptions(Options{}.set<GrpcCredentialOption>(creds));
  EXPECT_FALSE(actual.has<UnifiedCredentialsOption>());
  EXPECT_TRUE(actual.has<GrpcCredentialOption>());
  EXPECT_EQ(creds, actual.get<GrpcCredentialOption>());
}

TEST(PopulateGrpcOptions, ApiKey) {
  auto actual = PopulateGrpcOptions(Options{}.set<ApiKeyOption>("api-key"));
  EXPECT_TRUE(actual.has<GrpcCredentialOption>());
  EXPECT_FALSE(actual.has<UnifiedCredentialsOption>());
}

TEST(PopulateGrpcOptions, ApiKeyWithCredentialsErrors) {
  auto actual = PopulateGrpcOptions(
      Options{}.set<ApiKeyOption>("api-key").set<UnifiedCredentialsOption>(
          MakeGoogleDefaultCredentials()));

  EXPECT_TRUE(actual.has<UnifiedCredentialsOption>());
  auto const& creds = actual.get<UnifiedCredentialsOption>();
  TestCredentialsVisitor v;
  CredentialsVisitor::dispatch(*creds, v);
  EXPECT_EQ(v.name, "ErrorCredentialsConfig");
}

TEST(PopulateGrpcOptions, TracingOptions) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
                        "truncate_string_field_longer_than=42");
  auto actual = PopulateGrpcOptions(Options{});
  auto tracing = actual.get<GrpcTracingOptionsOption>();
  EXPECT_EQ(tracing.truncate_string_field_longer_than(), 42);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
