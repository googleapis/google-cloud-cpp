// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/setenv.h"
#include "generator/integration_tests/golden/internal/golden_kitchen_sink_option_defaults.gcpcxx.pb.h"
#include <gtest/gtest.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

TEST(GoldenKitchenSinkDefaultOptions, DefaultEndpoint) {
  Options options;
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("goldenkitchensink.googleapis.com",
            updated_options.get<EndpointOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, EnvVarEndpoint) {
  internal::SetEnv("GOOGLE_CLOUD_CPP_GOLDEN_KITCHEN_SINK_ENDPOINT",
                   "foo.googleapis.com");
  Options options;
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("foo.googleapis.com", updated_options.get<EndpointOption>());
}

TEST(GoldenKitchenSinkDefaultOptions, OptionEndpoint) {
  internal::SetEnv("GOOGLE_CLOUD_CPP_GOLDEN_KITCHEN_SINK_ENDPOINT",
                   "foo.googleapis.com");
  Options options;
  options.set<EndpointOption>("bar.googleapis.com");
  auto updated_options = GoldenKitchenSinkDefaultOptions(options);
  EXPECT_EQ("bar.googleapis.com", updated_options.get<EndpointOption>());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
