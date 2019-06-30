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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_FORMAT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_FORMAT_H_

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <cstddef>
#include <ctime>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
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

/**
 * Convert a system_clock::time_point to an RFC3339 "date-time".
 */
std::string TimestampToString(std::chrono::system_clock::time_point tp);

/**
 * Convert an RFC3339 "date-time" to a system_clock::time_point.
 *
 * Returns a a non-OK Status if the input cannot be parsed. Only accepts strings
 * with a "Z" timezone offset.
 */
StatusOr<std::chrono::system_clock::time_point> TimestampFromStringZ(
    std::string const& s);

/**
 * Convert an RFC3339 "date-time" to a system_clock::time_point.
 *
 * Returns a a non-OK Status if the input cannot be parsed. Accepts strings
 * with arbitrary timezone offsets, including "Z".
 */
StatusOr<std::chrono::system_clock::time_point> TimestampFromString(
    std::string const& s);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_FORMAT_H_
