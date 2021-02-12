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

#include "google/cloud/internal/setenv.h"
// We need _putenv_s() on WIN32 and setenv()/unsetenv() on Posix. clang-tidy
// recommends including <cstdlib>. That seems wrong, as <cstdlib> is not
// guaranteed to define the Posix/WIN32 functions.
#include <stdlib.h>  // NOLINT(modernize-deprecated-headers)

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

void UnsetEnv(char const* variable) {
#ifdef _WIN32
  (void)_putenv_s(variable, "");
#else
  unsetenv(variable);
#endif  // _WIN32
}

void SetEnv(char const* variable, char const* value) {
  if (value == nullptr) {
    UnsetEnv(variable);
    return;
  }
#ifdef _WIN32
  (void)_putenv_s(variable, value);
#else
  (void)setenv(variable, value, 1);
#endif  // _WIN32
}

void SetEnv(char const* variable, absl::optional<std::string> value) {
  if (!value.has_value()) {
    UnsetEnv(variable);
    return;
  }
  SetEnv(variable, value->data());
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
