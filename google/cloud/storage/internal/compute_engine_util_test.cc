// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::internal::SetEnv;
using ::google::cloud::internal::UnsetEnv;
using ::google::cloud::testing_util::EnvironmentVariableRestore;

class ComputeEngineUtilTest : public ::testing::Test {
 public:
  ComputeEngineUtilTest()
      : gce_check_override_env_var_(GceCheckOverrideEnvVar()),
        gce_metadata_hostname_env_var_(GceMetadataHostnameEnvVar()) {}

 protected:
  void SetUp() override {
    gce_check_override_env_var_.SetUp();
    gce_metadata_hostname_env_var_.SetUp();
  }

  void TearDown() override {
    gce_check_override_env_var_.TearDown();
    gce_metadata_hostname_env_var_.TearDown();
  }

 protected:
  EnvironmentVariableRestore gce_check_override_env_var_;
  EnvironmentVariableRestore gce_metadata_hostname_env_var_;
};

/// @test Ensure we can override the return value for checking if we're on GCE.
TEST_F(ComputeEngineUtilTest, CanOverrideRunningOnGceCheckViaEnvVar) {
  SetEnv(GceCheckOverrideEnvVar(), "1");
  EXPECT_TRUE(RunningOnComputeEngineVm());

  SetEnv(GceCheckOverrideEnvVar(), "0");
  EXPECT_FALSE(RunningOnComputeEngineVm());
}

/// @test Ensure we can override the value for the GCE metadata hostname.
TEST_F(ComputeEngineUtilTest, CanOverrideGceMetadataHostname) {
  SetEnv(GceMetadataHostnameEnvVar(), "foo.bar");
  EXPECT_EQ(std::string("foo.bar"), GceMetadataHostname());

  // If not overridden for testing, we should get the actual hostname.
  UnsetEnv(GceMetadataHostnameEnvVar());
  EXPECT_EQ(std::string("metadata.google.internal"), GceMetadataHostname());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
