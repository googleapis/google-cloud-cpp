// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::HasSubstr;
using ::testing::StartsWith;

TEST(ConnectionOptionsTraits, AdminEndpoint) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_EQ(ConnectionOptionsTraits::default_endpoint(), options.endpoint());
  options.set_endpoint("invalid-endpoint");
  EXPECT_EQ("invalid-endpoint", options.endpoint());
}

TEST(ConnectionOptionsTraits, NumChannels) {
  ConnectionOptions options(grpc::InsecureChannelCredentials());
  int num_channels = options.num_channels();
  EXPECT_EQ(ConnectionOptionsTraits::default_num_channels(), num_channels);
  num_channels *= 2;  // ensure we change it from the default value.
  options.set_num_channels(num_channels);
  EXPECT_EQ(num_channels, options.num_channels());
}

TEST(ConnectionOptionsTraits, UserAgentPrefix) {
  auto actual = ConnectionOptionsTraits::user_agent_prefix();
  EXPECT_THAT(actual, StartsWith("gcloud-cpp/" + VersionString()));
  EXPECT_THAT(actual, HasSubstr(google::cloud::internal::CompilerId()));
  EXPECT_THAT(actual, HasSubstr(google::cloud::internal::CompilerVersion()));
  EXPECT_THAT(actual, HasSubstr(google::cloud::internal::CompilerFeatures()));

  ConnectionOptions options(grpc::InsecureChannelCredentials());
  EXPECT_EQ(actual, options.user_agent_prefix());
  options.add_user_agent_prefix("test-prefix/1.2.3");
  EXPECT_EQ("test-prefix/1.2.3 " + actual, options.user_agent_prefix());
}

TEST(DefaultEndpoint, EnvironmentWorks) {
  EXPECT_NE("invalid-endpoint", ConnectionOptionsTraits::default_endpoint());
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT",
                        "invalid-endpoint");
  EXPECT_EQ("invalid-endpoint", ConnectionOptionsTraits::default_endpoint());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
