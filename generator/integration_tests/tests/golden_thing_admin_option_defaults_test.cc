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

#include "generator/integration_tests/golden/internal/golden_thing_admin_option_defaults.h"
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

TEST(GoldenThingAdminDefaultOptions, DefaultEndpoint) {
  Options options;
  auto updated_options = GoldenThingAdminDefaultOptions(options);
  EXPECT_EQ("test.googleapis.com", updated_options.get<EndpointOption>());
}

TEST(GoldenThingAdminDefaultOptions, EnvVarEndpoint) {
  auto env =
      ScopedEnvironment("GOLDEN_KITCHEN_SINK_ENDPOINT", "foo.googleapis.com");
  Options options;
  options.set<EndpointOption>("bar.googleapis.com");
  auto updated_options = GoldenThingAdminDefaultOptions(options);
  EXPECT_EQ("foo.googleapis.com", updated_options.get<EndpointOption>());
}

TEST(GoldenThingAdminDefaultOptions, OptionEndpoint) {
  Options options;
  options.set<EndpointOption>("bar.googleapis.com");
  auto updated_options = GoldenThingAdminDefaultOptions(options);
  EXPECT_EQ("bar.googleapis.com", updated_options.get<EndpointOption>());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
