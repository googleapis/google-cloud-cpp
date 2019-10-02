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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PARSE_RFC3339_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PARSE_RFC3339_H_

#include "google/cloud/version.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * Parses @p timestamp assuming it is in RFC-3339 format.
 *
 * Google Cloud Storage uses RFC-3339 for timestamps, this function is used to
 * parse them and convert to `std::chrono::system_clock::time_point`, the C++
 * class used to represent timestamps.  Depending on the underlying C++
 * implementation the timestamp may lose precision. C++ does not specify the
 * precision of the system clock, though most implementations have sub-second
 * precision, and nanoseconds is common.  The RFC-3339 spec allows for arbitrary
 * precision in fractional seconds, though it would be surprising to see
 * femtosecond timestamp for Internet events.
 *
 * @see https://tools.ietf.org/html/rfc3339
 */
std::chrono::system_clock::time_point ParseRfc3339(
    std::string const& timestamp);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PARSE_RFC3339_H_
