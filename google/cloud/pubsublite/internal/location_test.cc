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

#include "google/cloud/pubsublite/internal/location.h"
#include <gmock/gmock.h>
#include <deque>
#include <memory>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using google::cloud::pubsublite_internal::MakeCloudRegion;
using google::cloud::pubsublite_internal::MakeCloudZone;
using google::cloud::pubsublite_internal::MakeLocation;

TEST(CloudRegion, ValidRegion) {
  std::string str = "first-second";
  auto region = MakeCloudRegion(str);
  EXPECT_TRUE(region.ok());
  EXPECT_EQ(*region, (CloudRegion{str}));
  EXPECT_EQ(region->ToString(), str);
}

TEST(CloudRegion, InvalidRegionNoDash) {
  std::string str = "firstsecond";
  auto region = MakeCloudRegion(str);
  EXPECT_FALSE(region.ok());
  EXPECT_EQ(region.status(),
            Status(StatusCode::kInvalidArgument, "Invalid region name"));
}

TEST(CloudRegion, InvalidRegionTooManyDashes) {
  std::string str = "first-second-third";
  auto region = MakeCloudRegion(str);
  EXPECT_FALSE(region.ok());
  EXPECT_EQ(region.status(),
            Status(StatusCode::kInvalidArgument, "Invalid region name"));
}

TEST(CloudZone, ValidZone) {
  std::string region = "first-second";
  char zone_id = 't';
  std::string formatted_zone = region + "-" + zone_id;
  auto zone = MakeCloudZone(formatted_zone);
  EXPECT_TRUE(zone.ok());
  EXPECT_EQ(*zone, (CloudZone{CloudRegion{region}, zone_id}));
  EXPECT_EQ(zone->ToString(), formatted_zone);
}

TEST(CloudZone, InvalidZoneNoDash) {
  std::string str = "firstsecond";
  auto zone = MakeCloudZone(str);
  EXPECT_FALSE(zone.ok());
  EXPECT_EQ(zone.status(),
            Status(StatusCode::kInvalidArgument, "Invalid zone name"));
}

TEST(CloudZone, InvalidZoneNoTerminalLetter) {
  std::string str = "first-second-notaletter";
  auto zone = MakeCloudZone(str);
  EXPECT_FALSE(zone.ok());
  EXPECT_EQ(zone.status(),
            Status(StatusCode::kInvalidArgument, "Invalid zone name"));
}

TEST(CloudZone, InvalidZoneTooManyDashes) {
  std::string str = "first-second-t-t";
  auto zone = MakeCloudZone(str);
  EXPECT_FALSE(zone.ok());
  EXPECT_EQ(zone.status(),
            Status(StatusCode::kInvalidArgument, "Invalid zone name"));
}

TEST(Location, ValidCloudRegion) {
  std::string str = "first-second";
  auto location = MakeLocation(str);
  EXPECT_TRUE(location.ok());
  EXPECT_EQ(location->GetCloudRegion(), (CloudRegion{str}));
  EXPECT_EQ(location->ToString(), str);
  EXPECT_EQ(*location, (Location{CloudRegion{str}}));
}

TEST(Location, InvalidCloudRegion) {
  std::string str = "firstsecond";
  auto location = MakeLocation(str);
  EXPECT_FALSE(location.ok());
  EXPECT_EQ(location.status(),
            Status(StatusCode::kInvalidArgument, "Invalid location"));
}

TEST(Location, ValidCloudZone) {
  std::string region = "first-second";
  char zone_id = 't';
  auto location = MakeLocation(region + "-" + zone_id);
  EXPECT_TRUE(location.ok());
  EXPECT_EQ(location->GetCloudRegion(), (CloudRegion{region}));
  EXPECT_EQ(location->ToString(), region + "-" + zone_id);
  EXPECT_EQ(*location, (Location{CloudZone{CloudRegion{region}, zone_id}}));
}

TEST(Location, InvalidCloudZone) {
  std::string str = "first-second-notaletter";
  auto location = MakeLocation(str);
  EXPECT_FALSE(location.ok());
  EXPECT_EQ(location.status(),
            Status(StatusCode::kInvalidArgument, "Invalid location"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
