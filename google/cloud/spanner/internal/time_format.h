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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_TIME_FORMAT_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_TIME_FORMAT_H_

#include "google/cloud/spanner/version.h"
#include <cstddef>
#include <ctime>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * Format the date/time information from `tm` into a string according to
 * format string `fmt`.
 */
std::string FormatTime(char const* fmt, std::tm const& tm);

/**
 * Parse the date/time string `s` according to format string `fmt`, storing
 * the result in the std::tm addressed by `tm`. Returns std::string::npos if
 * the format string could not be matched. Otherwise returns the position of
 * the first character not consumed (s.size() if the entire string matched).
 */
std::size_t ParseTime(char const* fmt, std::string const& s, std::tm* tm);

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_TIME_FORMAT_H_
