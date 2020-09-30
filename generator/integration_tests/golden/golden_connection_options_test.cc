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

#include "generator/integration_tests/golden/connection_options.gcpcxx.pb.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden {
inline namespace GOLDEN_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

TEST(GoldenConnectionOptionsTest, DefaultEndpoint) {
  ConnectionOptions options;
  EXPECT_EQ("test.googleapis.com", options.endpoint());
}

TEST(GoldenConnectionOptionsTest, UserAgentPrefix) {
  ConnectionOptions options;
  EXPECT_THAT(options.user_agent_prefix(), HasSubstr("gcloud-cpp/v1.19.0"));
}

TEST(GoldenConnectionOptionsTest, DefaultNumChannels) {
  ConnectionOptions options;
  EXPECT_EQ(4, options.num_channels());
}

}  // namespace
}  // namespace GOLDEN_CLIENT_NS
}  // namespace golden
}  // namespace cloud
}  // namespace google
