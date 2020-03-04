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

#include "google_application_default_credentials_file.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::HasSubstr;

class DefaultServiceAccountFileTest : public ::testing::Test {
 public:
  DefaultServiceAccountFileTest()
      : home_env_var_(GoogleAdcHomeEnvVar(), {}),
        adc_env_var_(GoogleAdcEnvVar(), {}),
        gcloud_path_override_env_var_(GoogleGcloudAdcFileEnvVar(), {}) {}

 protected:
  ScopedEnvironment home_env_var_;
  ScopedEnvironment adc_env_var_;
  ScopedEnvironment gcloud_path_override_env_var_;
};

/// @test Verify that the specified path is given when the ADC env var is set.
TEST_F(DefaultServiceAccountFileTest, AdcEnvironmentVariableSet) {
  ScopedEnvironment adc_env_var(GoogleAdcEnvVar(), "/foo/bar/baz");
  EXPECT_EQ("/foo/bar/baz", GoogleAdcFilePathFromEnvVarOrEmpty());
}

/// @test Verify that an empty string is given when the ADC env var is unset.
TEST_F(DefaultServiceAccountFileTest, AdcEnvironmentVariableNotSet) {
  EXPECT_EQ(GoogleAdcFilePathFromEnvVarOrEmpty(), "");
}

/// @test Verify that the gcloud ADC file path can be overridden for testing.
TEST_F(DefaultServiceAccountFileTest, GcloudAdcPathOverrideViaEnvVar) {
  ScopedEnvironment gcloud_path_override_env_var(GoogleGcloudAdcFileEnvVar(),
                                                 "/foo/bar/baz");
  EXPECT_EQ(GoogleAdcFilePathFromWellKnownPathOrEmpty(), "/foo/bar/baz");
}

/// @test Verify that the gcloud ADC file path is given when HOME is set.
TEST_F(DefaultServiceAccountFileTest, HomeSet) {
  ScopedEnvironment home_env_var(GoogleAdcHomeEnvVar(), "/foo/bar/baz");

  auto actual = GoogleAdcFilePathFromWellKnownPathOrEmpty();

  EXPECT_THAT(actual, HasSubstr("/foo/bar/baz"));
  // The rest of the path differs depending on the OS; just make sure that we
  // appended this suffix of the path to the path prefix set above.
  EXPECT_THAT(actual, HasSubstr("gcloud/application_default_credentials.json"));
}

/// @test Verify that the gcloud ADC file path is not given when HOME is unset.
TEST_F(DefaultServiceAccountFileTest, HomeNotSet) {
  EXPECT_EQ(GoogleAdcFilePathFromWellKnownPathOrEmpty(), "");
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
