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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_UTILS_H

#include "google/cloud/version.h"
#include "absl/time/time.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// These functions convert between an `absl::Time` and a
// `google::protobuf::Timestamp` proto. The required format for the Timestamp
// proto is documented in this file:
// https://github.com/protocolbuffers/protobuf/blob/master/src/google/protobuf/timestamp.proto
//
// In particular, the Timestamp proto must:
// * be in the range ["0001-01-01T00:00:00Z", "9999-12-31T23:59:59.999999999Z"]
// * have a non-negative nanos() field, even for times before the Unix epoch
//
// `absl::Time` has a greater range and precision than the proto. Any
// `absl::Time` values outside of the proto's supported range will be capped at
// the min/max proto value. Any additional precision will be floored.
//
absl::Time ToAbslTime(google::protobuf::Timestamp const& proto);
google::protobuf::Timestamp ToProtoTimestamp(absl::Time t);

// Same as above, but converts to/from `std::chrono::system_clock::time_point`
// and `google::protobuf::Timestamp`.
std::chrono::system_clock::time_point ToChronoTimePoint(
    google::protobuf::Timestamp const& proto);
google::protobuf::Timestamp ToProtoTimestamp(
    std::chrono::system_clock::time_point const& tp);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_UTILS_H
