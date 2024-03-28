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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/string_view.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string FormatWin32ErrorsImpl(
    absl::FunctionRef<std::string(absl::string_view, unsigned long)>);

/**
 * Formats the last Win32 error into a human-readable string.
 *
 * @param prefixes A list of string-like objects to prepend to the error
 * message.
 */
template <typename... AV>
std::string FormatWin32Errors(AV&&... prefixes) {
  return FormatWin32ErrorsImpl([&](absl::string_view msg, unsigned long ec) {
    return absl::StrCat(std::forward<AV>(prefixes)..., msg, " (error code ", ec,
                        ")");
  });
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // _WIN32

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_WIN32_WIN32_HELPERS_H
