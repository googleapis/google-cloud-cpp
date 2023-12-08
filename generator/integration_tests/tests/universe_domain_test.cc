// Copyright 2023 Google LLC
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

#include "generator/integration_tests/golden/v1/golden_thing_admin_connection.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Eq;

TEST(GeneratorUniverseDomainTest, ConnectionEndpointOptionUnset) {
  auto connection = MakeGoldenThingAdminConnection();
  EXPECT_THAT(connection->options().get<EndpointOption>(),
              Eq("test.googleapis.com"));
}

TEST(GeneratorUniverseDomainTest, ConnectionEndpointOptionEmpty) {
  auto connection =
      MakeGoldenThingAdminConnection(Options{}.set<EndpointOption>(""));
  EXPECT_THAT(connection->options().get<EndpointOption>(), Eq(""));
}

TEST(GeneratorUniverseDomainTest, ConnectionEndpointOptionNonEmpty) {
  auto connection = MakeGoldenThingAdminConnection(
      Options{}.set<EndpointOption>("foo.bar.net"));
  EXPECT_THAT(connection->options().get<EndpointOption>(), Eq("foo.bar.net"));
}

TEST(GeneratorUniverseDomainTest, ConnectionEndpointEnvVarEmpty) {
  // TODO(#13229): Change env var name when this issue is resolved.
  ScopedEnvironment endpoint_var("GOLDEN_KITCHEN_SINK_ENDPOINT", "");
  auto connection = MakeGoldenThingAdminConnection();
  EXPECT_THAT(connection->options().get<EndpointOption>(),
              Eq("test.googleapis.com"));
}

TEST(GeneratorUniverseDomainTest, ConnectionEndpointEnvVarNonEmpty) {
  // TODO(#13229): Change env var name when this issue is resolved.
  ScopedEnvironment endpoint_var("GOLDEN_KITCHEN_SINK_ENDPOINT", "foo.bar.net");
  auto connection = MakeGoldenThingAdminConnection();
  EXPECT_THAT(connection->options().get<EndpointOption>(), Eq("foo.bar.net"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
