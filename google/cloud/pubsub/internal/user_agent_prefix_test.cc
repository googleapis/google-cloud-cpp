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

#include "google/cloud/pubsub/internal/user_agent_prefix.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/compiler_info.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::HasSubstr;
using ::testing::StartsWith;

TEST(UserAgentPrefix, Format) {
  auto const actual = UserAgentPrefix();
  EXPECT_THAT(actual, StartsWith("gcloud-cpp/"));
  EXPECT_THAT(actual, HasSubstr(pubsub::VersionString()));
  EXPECT_THAT(actual, HasSubstr(::google::cloud::internal::CompilerId()));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
