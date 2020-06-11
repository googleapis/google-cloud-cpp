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
#include <google/protobuf/timestamp.pb.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::chrono::system_clock::time_point ToChronoTimePoint(
    google::protobuf::Timestamp const& ts);

template <typename Duration>
google::protobuf::Timestamp ToProtoTimestamp(
    std::chrono::time_point<std::chrono::system_clock, Duration> const& tp) {
  auto d = tp.time_since_epoch();
  using std::chrono::duration_cast;
  auto seconds = duration_cast<std::chrono::seconds>(d);
  auto nanos = duration_cast<std::chrono::nanoseconds>(d - seconds);
  google::protobuf::Timestamp ts;
  ts.set_seconds(seconds.count());
  // Some platforms use 64 bit integers to represent nanos.count(). The proto
  // field is only 32 bits. It is safe here to perform this narrowing cast
  // because the arithmetic used to compute nanos precludes it from having a
  // value > 1,000,000,000.
  ts.set_nanos(static_cast<std::int32_t>(nanos.count()));
  return ts;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIME_UTILS_H
