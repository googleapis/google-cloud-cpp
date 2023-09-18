// Copyright 2023 Google LLC
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

#include "google/cloud/location.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;

TEST(Location, Basics) {
  Location loc("p1", "l1");
  EXPECT_EQ("p1", loc.project_id());
  EXPECT_EQ("l1", loc.location_id());
  EXPECT_EQ("projects/p1/locations/l1", loc.FullName());

  auto copy = loc;
  EXPECT_EQ(copy, loc);
  EXPECT_EQ("p1", copy.project_id());
  EXPECT_EQ("l1", copy.location_id());
  EXPECT_EQ("projects/p1/locations/l1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, loc);
  EXPECT_EQ("p1", moved.project_id());
  EXPECT_EQ("l1", moved.location_id());
  EXPECT_EQ("projects/p1/locations/l1", moved.FullName());

  Location loc2(Project("p2"), "l2");
  EXPECT_NE(loc2, loc);
  EXPECT_EQ("p2", loc2.project_id());
  EXPECT_EQ("l2", loc2.location_id());
  EXPECT_EQ("projects/p2/locations/l2", loc2.FullName());
}

TEST(Location, OutputStream) {
  Location loc("p1", "l1");
  std::ostringstream os;
  os << loc;
  EXPECT_EQ("projects/p1/locations/l1", os.str());
}

TEST(Location, MakeLocation) {
  auto loc = Location(Project("p1"), "i1");
  EXPECT_EQ(loc, MakeLocation(loc.FullName()).value());

  for (std::string invalid : {
           "",
           "projects/",
           "projects/p1",
           "projects/p1/locations/",
           "/projects/p1/locations/i1",
           "projects/p1/locations/i1/",
           "projects/p1/locations/i1/etc",
       }) {
    auto loc = MakeLocation(invalid);
    EXPECT_THAT(loc, StatusIs(StatusCode::kInvalidArgument,
                              "Improperly formatted Location: " + invalid));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
