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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SETENV_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SETENV_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

/**
 * Set the @p variable environment variable to @p value.
 *
 * If @value is the null pointer then the variable is unset.
 *
 * @note On Windows, due to the underlying API function, an empty @value unsets
 * the variable, while on Linux an empty environment variable is created.
 *
 * @see
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/putenv-s-wputenv-s?view=vs-2019
 *
 * @warning A modification to the environment must be serialized with all
 *   other environment reads and writes, so this should only be used when
 *   we are single-threaded.
 */
void SetEnv(char const* variable, char const* value);

/**
 * Set the @p variable environment variable to @p value.
 *
 * If @value is an unset `absl::optional` then the variable is unset.
 *
 * @warning A modification to the environment must be serialized with all
 *   other environment reads and writes, so this should only be used when
 *   we are single-threaded.
 */
void SetEnv(char const* variable, absl::optional<std::string> value);

/**
 * Unset (remove) an environment variable.
 *
 * @warning A modification to the environment must be serialized with all
 *   other environment reads and writes, so this should only be used when
 *   we are single-threaded.
 */
void UnsetEnv(char const* variable);

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SETENV_H
