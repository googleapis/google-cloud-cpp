// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_CLOUD_ZONE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_CLOUD_ZONE_H

#include "google/cloud/pubsublite/internal/cloud_region.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/str_split.h"
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

/**
 * A representation of a Google Cloud zone.
 */
struct CloudZone {
  /**
   * Construct a CloudZone from a valid zone string. `zone` must be formatted
   * as: <location>-<direction><number>-<letter>
   */
  static StatusOr<CloudZone> Parse(std::string const& zone) {
    std::vector<std::string> splits = absl::StrSplit(zone, '-');
    if (splits.size() != 3 || splits[2].length() != 1) {
      return Status{StatusCode::kInvalidArgument, "Invalid zone name"};
    }
    return CloudZone{CloudRegion{absl::StrCat(splits[0], "-", splits[1])},
                     splits[2][0]};
  }

  std::string ToString() const {
    return absl::StrCat(region.ToString(), "-", std::string(1, zone_id));
  }

  CloudRegion const region;
  char const zone_id;
};

inline bool operator==(CloudZone const& a, CloudZone const& b) {
  return a.region == b.region && a.zone_id == b.zone_id;
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_CLOUD_ZONE_H
