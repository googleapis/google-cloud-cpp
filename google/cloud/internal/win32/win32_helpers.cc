// Copyright 2024 Google LLC
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

#ifdef _WIN32
#include "google/cloud/internal/win32/win32_helpers.h"
#include <memory>
#include <Windows.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

static_assert(std::is_same_v<DWORD, unsigned long>,
              "DWORD is not unsigned long");

std::string FormatWin32ErrorsImpl(
    absl::FunctionRef<std::string(absl::string_view, unsigned long)> f) {
  auto last_error = GetLastError();
  LPSTR message_buffer_raw = nullptr;
  auto size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&message_buffer_raw, 0, nullptr);
  std::unique_ptr<char, decltype(&LocalFree)> message_buffer(message_buffer_raw,
                                                             &LocalFree);
  return f(absl::string_view(message_buffer.get(), size), last_error);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // _WIN32
