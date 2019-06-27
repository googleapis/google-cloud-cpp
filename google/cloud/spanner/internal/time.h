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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TIME_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TIME_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * Convert a std::chrono::nanoseconds to a google::protobuf::Duration.
 */
google::protobuf::Duration ToProto(std::chrono::nanoseconds ns);

/**
 * Convert a google::protobuf::Duration to a std::chrono::nanoseconds.
 */
std::chrono::nanoseconds FromProto(google::protobuf::Duration const& proto);

/**
 * Convert a system_clock::time_point to a google::protobuf::Timestamp.
 */
google::protobuf::Timestamp ToProto(std::chrono::system_clock::time_point tp);

/**
 * Convert a google::protobuf::Timestamp to a system_clock::time_point.
 */
std::chrono::system_clock::time_point FromProto(
    google::protobuf::Timestamp const& proto);

/**
 * Convert a system_clock::time_point to an RFC3339 "date-time".
 */
std::string TimestampToString(std::chrono::system_clock::time_point tp);

/**
 * Convert an RFC3339 "date-time" to a system_clock::time_point.
 *
 * Returns a a non-OK Status if the input cannot be parsed.
 */
StatusOr<std::chrono::system_clock::time_point> TimestampFromString(
    std::string const& s);

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_TIME_H_
