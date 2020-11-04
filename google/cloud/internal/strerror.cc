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

#include "google/cloud/internal/strerror.h"
#include <cstring>
#include <sstream>
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

// Depending on the platform `strerror_r()` returns
// - `int` on macOS (and Linux with the XSI compilation flags)
// - `char*` on Linux with the GNU flags.
// Meanwhile `strerror_s()` (on WIN32) returns `errno_t`.
//
// We use overload resolution to handle all cases, and SFINAE to avoid the
// "unused function" warnings.
template <typename T,
          typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
std::string handle_strerror_r_error(char const* msg, int errnum, T result) {
  if (result == 0) return msg;
  std::ostringstream os;
  os << "Cannot get error message for errno=" << errnum << ", result=" << result
     << ", errno=" << errno;
  return std::move(os).str();
}

template <typename T,
          typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
std::string handle_strerror_r_error(char const*, int errnum, T result) {
  if (result != nullptr) return result;
  std::ostringstream os;
  os << "Cannot get error message for errno=" << errnum << ", result=nullptr"
     << ", errno=" << errno;
  return std::move(os).str();
}

}  // namespace

std::string strerror(int errnum) {
  auto constexpr kMaxErrorMessageLength = 1024;
  char error_msg[kMaxErrorMessageLength];
#ifdef _WIN32
  auto const result = strerror_s(error_msg, sizeof(error_msg) - 1, errnum);
#else
  // Cannot use `auto const*` because on macOS (and with the right settings on
  // Linux) this can return `int`.
  // NOLINTNEXTLINE(readability-qualified-auto)
  auto const result = strerror_r(errnum, error_msg, sizeof(error_msg) - 1);
#endif  // _WIN32
  return handle_strerror_r_error(error_msg, errnum, result);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
