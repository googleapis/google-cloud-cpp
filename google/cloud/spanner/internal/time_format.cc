// Copyright 2019 Google LLC
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

// GCC did not support std::get_time() or std::put_time() until version 5,
// so we fall back to using POSIX XSI strptime() and strftime() instead.
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE
#endif
#endif

#include "google/cloud/spanner/internal/time_format.h"
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#include <time.h>  // <ctime> doesn't have to declare strptime()
#else
#include <iomanip>
#include <sstream>
#endif

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

std::string FormatTime(char const* fmt, std::tm const& tm) {
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
  std::string s;
  s.resize(64);
  for (;;) {
    if (auto len = strftime(&s[0], s.size(), fmt, &tm)) {
      s.resize(len);
      break;
    }
    s.resize(s.size() * 2);
  }
  return s;
#else
  std::ostringstream output;
  output << std::put_time(&tm, fmt);
  return output.str();
#endif
}

std::size_t ParseTime(char const* fmt, std::string const& s, std::tm* tm) {
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
  char const* const bp = s.c_str();
  if (char* const ep = strptime(bp, fmt, tm)) {
    return ep - bp;
  }
  return std::string::npos;
#else
  std::istringstream input(s);
  input >> std::get_time(tm, fmt);
  if (!input) return std::string::npos;
  auto const pos = input.tellg();
  if (pos >= 0) return pos;
  return s.size();
#endif
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
