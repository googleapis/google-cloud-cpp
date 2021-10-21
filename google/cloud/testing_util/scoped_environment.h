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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_ENVIRONMENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_ENVIRONMENT_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * Helper class to (un)set and restore the value of an environment variable.
 */
class ScopedEnvironment {
 public:
  /**
   * Set the @p variable environment variable to @p value. If @value is
   * an unset `absl::optional` then the variable is unset. The previous value
   * of the variable will be restored upon destruction.
   */
  ScopedEnvironment(std::string variable,
                    absl::optional<std::string> const& value);

  ~ScopedEnvironment();

  // Movable, but not copyable.
  ScopedEnvironment(ScopedEnvironment const&) = delete;
  ScopedEnvironment(ScopedEnvironment&&) = default;
  ScopedEnvironment& operator=(ScopedEnvironment const&) = delete;
  ScopedEnvironment& operator=(ScopedEnvironment&&) = default;

 private:
  std::string variable_;
  absl::optional<std::string> prev_value_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_ENVIRONMENT_H
