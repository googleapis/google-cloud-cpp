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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_CLOUD_REGION_OR_ZONE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_CLOUD_REGION_OR_ZONE_H

#include "google/cloud/version.h"
#include "google/cloud/pubsublite/cloud_region.h"
#include "google/cloud/pubsublite/cloud_zone.h"
#include "absl/types/variant.h"

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class CloudRegionOrZone {
 public:
  explicit CloudRegionOrZone(CloudRegion region) : value_{std::move(region)} {}

  explicit CloudRegionOrZone(CloudZone zone) : value_{std::move(zone)} {}

  bool HasCloudRegion() const { return absl::holds_alternative<CloudRegion>(value_); }

  bool HasCloudZone() const { return absl::holds_alternative<CloudZone>(value_); }

  CloudRegion const& GetRegion() const { return absl::get<CloudRegion>(value_); }

  CloudZone const& GetZone() const { return absl::get<CloudZone>(value_); }

 private:
  absl::variant<CloudRegion, CloudZone> const value_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_CLOUD_REGION_OR_ZONE_H
