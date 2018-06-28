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
#include "google/cloud/cloud_testing/environment_variable_restore.h"
#include "google/cloud/internal/setenv.h"
#include <gmock/gmock.h>

using namespace google::cloud::storage::internal;
using google::cloud::cloud_testing::EnvironmentVariableRestore;

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
  google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS",
                                  "/foo/bar/baz");
  auto actual = GoogleApplicationDefaultCredentialsFile();
  EXPECT_EQ("/foo/bar/baz", actual);
}

/// @test Verify that the file path works as expected when using HOME.
TEST_F(DefaultServiceAccountFileTest, HomeSet) {
  google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", nullptr);
  char const* home = GoogleApplicationDefaultCredentialsHomeVariable();
  google::cloud::internal::SetEnv(home, "/foo/bar/baz");
  auto actual = GoogleApplicationDefaultCredentialsFile();
  using testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("/foo/bar/baz"));
  EXPECT_THAT(actual, HasSubstr(".json"));
}

/// @test Verify that the service account file path fails when HOME is not set.
TEST_F(DefaultServiceAccountFileTest, HomeNotSet) {
  google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", nullptr);
  char const* home = GoogleApplicationDefaultCredentialsHomeVariable();
  google::cloud::internal::UnsetEnv(home);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(GoogleApplicationDefaultCredentialsFile(), std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(GoogleApplicationDefaultCredentialsFile(),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
