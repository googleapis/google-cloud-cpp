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

#include "google/cloud/storage/internal/google_application_default_credentials_file.h"
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

class DefaultServiceAccountFileTest : public ::testing::Test {
 public:
  DefaultServiceAccountFileTest()
      : home_(GoogleApplicationDefaultCredentialsHomeVariable()),
        override_variable_("GOOGLE_APPLICATION_CREDENTIALS") {}

 protected:
  void SetUp() override {
    home_.SetUp();
    override_variable_.SetUp();
  }
  void TearDown() override {
    override_variable_.TearDown();
    home_.TearDown();
  }

 protected:
  EnvironmentVariableRestore home_;
  EnvironmentVariableRestore override_variable_;
};

/// @test Verify that the application can override the default credentials.
TEST_F(DefaultServiceAccountFileTest, EnvironmentVariableSet) {
  SetEnv("GOOGLE_APPLICATION_CREDENTIALS", "/foo/bar/baz");
  auto actual = GoogleApplicationDefaultCredentialsFile();
  EXPECT_EQ("/foo/bar/baz", actual);
}

/// @test Verify that the file path works as expected when using HOME.
TEST_F(DefaultServiceAccountFileTest, HomeSet) {
  UnsetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  char const* home = GoogleApplicationDefaultCredentialsHomeVariable();
  SetEnv(home, "/foo/bar/baz");
  auto actual = GoogleApplicationDefaultCredentialsFile();
  using testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("/foo/bar/baz"));
  EXPECT_THAT(actual, HasSubstr(".json"));
}

/// @test Verify that the service account file path fails when HOME is not set.
TEST_F(DefaultServiceAccountFileTest, HomeNotSet) {
  UnsetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  char const* home = GoogleApplicationDefaultCredentialsHomeVariable();
  UnsetEnv(home);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(GoogleApplicationDefaultCredentialsFile(), std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(GoogleApplicationDefaultCredentialsFile(),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
