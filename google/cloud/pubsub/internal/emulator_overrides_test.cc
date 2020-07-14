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

#include "google/cloud/pubsub/internal/emulator_overrides.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;

TEST(EmulatorOverridesTest, NotSet) {
  ScopedEnvironment emulator("PUBSUB_EMULATOR_HOST", {});
  auto options = EmulatorOverrides(
      pubsub::ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("invalid-test-only"));
  EXPECT_EQ("invalid-test-only", options.endpoint());
}

TEST(EmulatorOverridesTest, Set) {
  ScopedEnvironment emulator("PUBSUB_EMULATOR_HOST",
                             "invalid-testing-override");
  auto options = EmulatorOverrides(
      pubsub::ConnectionOptions(grpc::InsecureChannelCredentials())
          .set_endpoint("invalid-test-only"));
  EXPECT_EQ("invalid-testing-override", options.endpoint());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
