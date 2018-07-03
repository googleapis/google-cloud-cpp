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

#include "google/cloud/testing_util/environment_variable_restore.h"
#include "google/cloud/internal/setenv.h"
#include <cstdlib>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

void EnvironmentVariableRestore::SetUp() {
  auto ptr = std::getenv(variable_name_.c_str());
  was_null_ = (ptr == nullptr);
  if (not was_null_) {
    previous_ = std::string(ptr);
  }
}

void EnvironmentVariableRestore::TearDown() {
  if (was_null_) {
    google::cloud::internal::UnsetEnv(variable_name_.c_str());
  } else {
    google::cloud::internal::SetEnv(variable_name_.c_str(), previous_.c_str());
  }
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
