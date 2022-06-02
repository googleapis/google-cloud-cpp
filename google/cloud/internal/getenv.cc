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

#include "google/cloud/internal/getenv.h"
#if defined(_MSC_VER)
// We need _dupenv_s()
#include <stdlib.h>
#else
// With other compilers we need setenv()/unsetenv(), which are defined here:
#include <cstdlib>
#endif  // _MSC_VER
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

absl::optional<std::string> GetEnv(char const* variable) {
#if defined(_MSC_VER)
  // With MSVC, std::getenv() is not thread-safe. It returns a pointer that can
  // be invalidated by _putenv_s(). We must use the thread-safe alternative,
  // which unfortunately allocates the buffer using malloc():
  char* buffer;
  std::size_t size;
  _dupenv_s(&buffer, &size, variable);
  std::unique_ptr<char, decltype(&free)> release(buffer, &free);
#else
  char* buffer = std::getenv(variable);
#endif  // _MSC_VER
  if (buffer == nullptr) return absl::nullopt;
  return std::string{buffer};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
