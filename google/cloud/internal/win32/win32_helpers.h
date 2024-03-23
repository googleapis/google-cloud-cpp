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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_WIN32_WIN32_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_WIN32_WIN32_HELPERS_H

#ifdef _WIN32
#include "google/cloud/version.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <iterator>
#include <Windows.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Formats the last Win32 error into a human-readable string.
 *
 * @param prefixes A list of string-like objects to prepend to the error
 * message.
 */
template <typename... AV>
std::string FormatWin32Errors(AV&&... prefixes) {
  auto last_error = GetLastError();
  LPSTR message_buffer_raw = nullptr;
  auto size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&message_buffer_raw, 0, nullptr);
  std::unique_ptr<char, decltype(&LocalFree)> message_buffer(message_buffer_raw,
                                                             &LocalFree);
  return absl::StrCat(std::forward<AV>(prefixes)...,
                      absl::string_view(message_buffer.get(), size),
                      " (error code ", last_error, ")");
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // _WIN32

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_WIN32_WIN32_HELPERS_H
