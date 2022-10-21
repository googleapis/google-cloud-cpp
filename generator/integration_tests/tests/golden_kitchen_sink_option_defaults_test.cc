// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_option_defaults.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gtest/gtest.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;

TEST(GoldenKitchenSinkDefaultOptions, DefaultEndpoint) {
  auto env = ScopedEnvironment("GOLDEN_KITCHEN_SINK_ENDPOINT", absl::nullopt);
  Options options;
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("goldenkitchensink.googleapis.com",
            updated_options.get<EndpointOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, EnvVarEndpoint) {
  auto env =
      ScopedEnvironment("GOLDEN_KITCHEN_SINK_ENDPOINT", "foo.googleapis.com");
  Options options;
  options.set<EndpointOption>("bar.googleapis.com");
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("foo.googleapis.com", updated_options.get<EndpointOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, OptionEndpoint) {
  Options options;
  options.set<EndpointOption>("bar.googleapis.com");
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("bar.googleapis.com", updated_options.get<EndpointOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, UserProjectDefault) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  auto options = Options{};
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_FALSE(updated_options.has<UserProjectOption>());
  EXPECT_EQ("", updated_options.get<UserProjectOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, UserProjectEnvVar) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_CPP_USER_PROJECT", "test-project");
  auto options = Options{};
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("test-project", updated_options.get<UserProjectOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, UserProjectOptions) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  auto options = Options{}.set<UserProjectOption>("another-project");
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("another-project", updated_options.get<UserProjectOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, UserProjectOptionAndEnvVar) {
  auto env = ScopedEnvironment("GOOGLE_CLOUD_CPP_USER_PROJECT", "test-project");
  auto options = Options{}.set<UserProjectOption>("another-project");
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("test-project", updated_options.get<UserProjectOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
