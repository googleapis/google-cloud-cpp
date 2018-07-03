// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ENVIRONMENT_VARIABLE_RESTORE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ENVIRONMENT_VARIABLE_RESTORE_H_

#include "google/cloud/version.h"
#include <cstdlib>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * Helper class to restore the value of environment variables.
 *
 * Use in the test fixture SetUp() and TearDown() to restore environment
 * variables modified by a test.
 */
class EnvironmentVariableRestore {
 public:
  explicit EnvironmentVariableRestore(char const* variable_name)
      : EnvironmentVariableRestore(std::string(variable_name)) {}

  explicit EnvironmentVariableRestore(std::string variable_name)
      : variable_name_(std::move(variable_name)) {
    SetUp();
  }

  void SetUp();
  void TearDown();

 private:
  std::string variable_name_;
  bool was_null_;
  std::string previous_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ENVIRONMENT_VARIABLE_RESTORE_H_
