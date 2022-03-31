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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_LOCATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_LOCATION_H

#include "google/cloud/pubsublite/internal/cloud_region.h"
#include "google/cloud/pubsublite/internal/cloud_zone.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <algorithm>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

/**
 * A wrapper around a Google Cloud Location which can be either a `CloudRegion`
 * or `CloudZone`.
 */
class Location {
 public:
  explicit Location(CloudRegion region) : value_{std::move(region)} {}

  explicit Location(CloudZone zone) : value_{std::move(zone)} {}

  CloudRegion const& GetCloudRegion() const {
    if (absl::holds_alternative<CloudRegion>(value_)) {
      return absl::get<CloudRegion>(value_);
    }
    return absl::get<CloudZone>(value_).region;
  }

  std::string ToString() const {
    if (absl::holds_alternative<CloudRegion>(value_)) {
      return absl::get<CloudRegion>(value_).ToString();
    }
    return absl::get<CloudZone>(value_).ToString();
  }

 private:
  absl::variant<CloudRegion, CloudZone> const value_;
};

/**
 * Attempts to parse a `CloudZone` or `CloudRegion` from `location`.
 */
StatusOr<Location> MakeLocation(std::string const& location);

inline bool operator==(Location const& a, Location const& b) {
  return a.ToString() == b.ToString();
}

inline bool operator!=(Location const& a, Location const& b) {
  return !(a == b);
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_LOCATION_H
