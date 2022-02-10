// Copyright 2018 Google LLC
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

#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

/// @test Ensure we can override the value for the GCE metadata hostname.
TEST(ComputeEngineUtilTest, CanOverrideGceMetadataHostname) {
  google::cloud::testing_util::ScopedEnvironment gce_metadata_hostname_env_var(
      GceMetadataHostnameEnvVar(), {});
  google::cloud::testing_util::ScopedEnvironment gce_metadata_hostname_set(
      GceMetadataHostnameEnvVar(), "foo.bar");
  EXPECT_EQ(std::string("foo.bar"), GceMetadataHostname());

  // If not overridden for testing, we should get the actual hostname.
  google::cloud::testing_util::ScopedEnvironment gce_metadata_hostname_unset(
      GceMetadataHostnameEnvVar(), {});
  EXPECT_EQ(std::string("metadata.google.internal"), GceMetadataHostname());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
