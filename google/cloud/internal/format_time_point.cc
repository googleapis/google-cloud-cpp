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

#include "google/cloud/internal/format_time_point.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::string FormatRfc3339(std::chrono::system_clock::time_point tp) {
  auto constexpr kFormat = "%E4Y-%m-%dT%H:%M:%E*SZ";
  auto const t = absl::FromChrono(tp);
  return absl::FormatTime(kFormat, t, absl::UTCTimeZone());
}

std::string FormatUtcDate(std::chrono::system_clock::time_point tp) {
  auto constexpr kFormat = "%E4Y-%m-%d";
  auto const t = absl::FromChrono(tp);
  return absl::FormatTime(kFormat, t, absl::UTCTimeZone());
}

std::string FormatV4SignedUrlTimestamp(
    std::chrono::system_clock::time_point tp) {
  auto constexpr kFormat = "%E4Y%m%dT%H%M%SZ";
  auto const t = absl::FromChrono(tp);
  return absl::FormatTime(kFormat, t, absl::UTCTimeZone());
}

std::string FormatV4SignedUrlScope(std::chrono::system_clock::time_point tp) {
  auto constexpr kFormat = "%E4Y%m%d";
  auto const t = absl::FromChrono(tp);
  return absl::FormatTime(kFormat, t, absl::UTCTimeZone());
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
