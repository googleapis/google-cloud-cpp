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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_CLOUD_REGION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_CLOUD_REGION_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/str_split.h"
#include <string>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

/**
 * A wrapped string representing a Google Cloud region.
 */
struct CloudRegion {
  static StatusOr<CloudRegion> Parse(std::string const& region) {
    std::vector<std::string> splits = absl::StrSplit(region, '-');
    if (splits.size() != 2) {
      return Status{StatusCode::kInvalidArgument, "Invalid region name"};
    }
    return CloudRegion{region};
  }

  std::string ToString() const { return region; }

  std::string region;
};

inline bool operator==(CloudRegion const& a, CloudRegion const& b) {
  return a.region == b.region;
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_CLOUD_REGION_H
