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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_LOCATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_LOCATION_H

#include "google/cloud/pubsublite/cloud_region.h"
#include "google/cloud/pubsublite/cloud_zone.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class Location {
 public:
  explicit Location(CloudRegion region) : value_{std::move(region)} {}

  explicit Location(CloudZone zone) : value_{std::move(zone)} {}

  bool HasCloudRegion() const {
    return absl::holds_alternative<CloudRegion>(value_);
  }

  bool HasCloudZone() const {
    return absl::holds_alternative<CloudZone>(value_);
  }

  CloudRegion const& GetCloudRegion() const {
    return absl::get<CloudRegion>(value_);
  }

  CloudZone const& GetCloudZone() const { return absl::get<CloudZone>(value_); }

  std::string ToString() const {
    if (HasCloudRegion()) {
      return absl::get<CloudRegion>(value_).GetRegion();
    }
    CloudZone cloud_zone = absl::get<CloudZone>(value_);
    return absl::StrCat(cloud_zone.GetCloudRegion().GetRegion(), "-",
                        std::string(1, cloud_zone.GetZoneId()));
  }

 private:
  absl::variant<CloudRegion, CloudZone> const value_;
};

Location ParseLocation(std::string const& location) {
  auto possible_zone = ParseCloudZone(location);
  if (possible_zone.ok()) {
    return Location{*possible_zone};
  }
  return Location{CloudRegion{location}};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_LOCATION_H
