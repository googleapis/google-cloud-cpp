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

#include "google/cloud/storage/internal/access_control_common.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
/// @test Verify the well-known values defined in AccessControlCommon.
TEST(ccessControlCommonTest, WellKnownValues) {
  EXPECT_EQ("OWNER", AccessControlCommon::ROLE_OWNER());
  EXPECT_EQ("READER", AccessControlCommon::ROLE_READER());

  EXPECT_EQ("editors", AccessControlCommon::TEAM_EDITORS());
  EXPECT_EQ("owners", AccessControlCommon::TEAM_OWNERS());
  EXPECT_EQ("viewers", AccessControlCommon::TEAM_VIEWERS());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
