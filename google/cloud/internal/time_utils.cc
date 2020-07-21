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

#include "google/cloud/internal/time_utils.h"
#include <google/protobuf/timestamp.pb.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

absl::Time ToAbslTime(google::protobuf::Timestamp const& proto) {
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

google::protobuf::Timestamp ToProtoTimestamp(absl::Time t) {
  // The min/max values that are allowed to be encoded in a Timestamp proto:
  // ["0001-01-01T00:00:00Z", "9999-12-31T23:59:59.999999999Z"]
  // Note: These values can be computed with `date +%s --date="YYYY-MM-...Z"`
  auto constexpr kMinTime = absl::FromUnixSeconds(-62135596800);
  auto const kMaxTime =  // NOLINT(readability-identifier-naming)
      absl::FromUnixSeconds(253402300799) + absl::Nanoseconds(999999999);
  if (t < kMinTime) {
    t = kMinTime;
  } else if (t > kMaxTime) {
    t = kMaxTime;
  }
  auto const s = absl::ToUnixSeconds(t);
  auto const ns = absl::ToInt64Nanoseconds(t - absl::FromUnixSeconds(s));
  google::protobuf::Timestamp proto;
  proto.set_seconds(s);
  proto.set_nanos(static_cast<std::int32_t>(ns));
  return proto;
}

std::chrono::system_clock::time_point ToChronoTimePoint(
    google::protobuf::Timestamp const& proto) {
  return absl::ToChronoTime(ToAbslTime(proto));
}

google::protobuf::Timestamp ToProtoTimestamp(
    std::chrono::system_clock::time_point const& tp) {
  return ToProtoTimestamp(absl::FromChrono(tp));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
