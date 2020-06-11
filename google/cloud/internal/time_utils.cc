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

std::chrono::system_clock::time_point ToChronoTimePoint(
    google::protobuf::Timestamp const& ts) {
  return std::chrono::system_clock::from_time_t(ts.seconds()) +
         std::chrono::duration_cast<std::chrono::system_clock::duration>(
             std::chrono::nanoseconds(ts.nanos()));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
