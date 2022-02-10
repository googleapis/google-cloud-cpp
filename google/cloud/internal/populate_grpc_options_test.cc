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
#include "google/cloud/testing_util/scoped_environment.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;

TEST(PopulateGrpcOptions, Simple) {
  // Unset any environment variables
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_TRACING_OPTIONS", absl::nullopt);
  auto actual = PopulateGrpcOptions(Options{}, {});
  EXPECT_TRUE(actual.has<GrpcCredentialOption>());
  EXPECT_TRUE(actual.has<GrpcTracingOptionsOption>());
  EXPECT_EQ(actual.get<GrpcTracingOptionsOption>()
                .truncate_string_field_longer_than(),
            TracingOptions{}.truncate_string_field_longer_than());
}

TEST(PopulateGrpcOptions, WithoutEmulator) {
  auto credentials = grpc::GoogleDefaultCredentials();
  auto actual =
      PopulateGrpcOptions(Options{}.set<GrpcCredentialOption>(credentials), "");
  EXPECT_EQ(actual.get<GrpcCredentialOption>(), credentials);
}

TEST(PopulateGrpcOptions, WithUnsetEmulator) {
  ScopedEnvironment env("EMULATOR", absl::nullopt);
  auto credentials = grpc::GoogleDefaultCredentials();
  auto actual = PopulateGrpcOptions(
      Options{}.set<GrpcCredentialOption>(credentials), "EMULATOR");
  EXPECT_EQ(actual.get<GrpcCredentialOption>(), credentials);
}

TEST(PopulateGrpcOptions, WithSetEmulator) {
  ScopedEnvironment env("EMULATOR", "test-only.example.com");
  auto credentials = grpc::GoogleDefaultCredentials();
  auto actual = PopulateGrpcOptions(
      Options{}.set<GrpcCredentialOption>(credentials), "EMULATOR");
  EXPECT_NE(actual.get<GrpcCredentialOption>(), credentials);
}

TEST(PopulateGrpcOptions, DefaultTracingOptionsNoEnvironment) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_TRACING_OPTIONS", absl::nullopt);
  auto const actual = internal::DefaultTracingOptions();
  auto const expected = TracingOptions{};
  EXPECT_EQ(expected.single_line_mode(), actual.single_line_mode());
  EXPECT_EQ(expected.use_short_repeated_primitives(),
            actual.use_short_repeated_primitives());
  EXPECT_EQ(expected.truncate_string_field_longer_than(),
            actual.truncate_string_field_longer_than());
}

TEST(PopulateGrpcOptions, DefaultTracingOptionsWithValue) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_TRACING_OPTIONS",
                        "single_line_mode=on"
                        ",use_short_repeated_primitives=ON"
                        ",truncate_string_field_longer_than=42");
  auto const actual = internal::DefaultTracingOptions();
  EXPECT_TRUE(actual.single_line_mode());
  EXPECT_TRUE(actual.use_short_repeated_primitives());
  EXPECT_EQ(42, actual.truncate_string_field_longer_than());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
