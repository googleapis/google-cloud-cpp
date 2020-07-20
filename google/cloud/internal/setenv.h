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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SETENV_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SETENV_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/// Unset (remove) an environment variable.
void UnsetEnv(char const* variable);

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
 */
void SetEnv(char const* variable, char const* value);

/**
 * Set the @p variable environment variable to @p value.
 *
 * If @value is an unset absl::optional then the variable is unset.
 */
void SetEnv(char const* variable, absl::optional<std::string> value);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SETENV_H
