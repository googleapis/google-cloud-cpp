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

#include "google/cloud/internal/getenv.h"
#ifdef _WIN32
// We need _dupenv_s()
#include <stdlib.h>
#else
// On Unix-like systems we need setenv()/unsetenv(), which are defined here:
#include <cstdlib>
#endif  // _WIN32

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

optional<std::string> GetEnv(char const* variable) {
#if _WIN32
  // On Windows, std::getenv() is not thread-safe. It returns a pointer that
  // can be invalidated by _putenv_s(). We must use the thread-safe alternative,
  // which unfortunately allocates the buffer using malloc():
  char* buffer;
  std::size_t size;
  _dupenv_s(&buffer, &size, variable);
  std::unique_ptr<char, decltype(&free)> release(buffer, &free);
#else
  char* buffer = std::getenv(variable);
#endif  // _WIN32
  if (buffer == nullptr) {
    return optional<std::string>();
  }
  return optional<std::string>(std::string{buffer});
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
