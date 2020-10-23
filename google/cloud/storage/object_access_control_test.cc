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

#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

/// @test Verify ObjectAccessControl::set_entity() works as expected.
TEST(ObjectAccessControlTest, SetEntity) {
  ObjectAccessControl tested;

  EXPECT_TRUE(tested.entity().empty());
  tested.set_entity("user-foo");
  EXPECT_EQ("user-foo", tested.entity());
}

/// @test Verify ObjectAccessControl::set_role() works as expected.
TEST(ObjectAccessControlTest, SetRole) {
  ObjectAccessControl tested;

  EXPECT_TRUE(tested.role().empty());
  tested.set_role(ObjectAccessControl::ROLE_READER());
  EXPECT_EQ("READER", tested.role());
}

/// @test Verify that comparison operators work as expected.
TEST(ObjectAccessControlTest, Compare) {
  std::string text = R"""({
      "bucket": "foo-bar",
      "domain": "example.com",
      "email": "foobar@example.com",
      "entity": "user-foobar",
      "entityId": "user-foobar-id-123",
      "etag": "XYZ=",
      "generation": 42,
      "id": "object-foo-bar-baz-acl-234",
      "kind": "storage#objectAccessControl",
      "object": "baz",
      "projectTeam": {
        "projectNumber": "3456789",
        "team": "a-team"
      },
      "role": "OWNER"
})""";
  auto original = internal::ObjectAccessControlParser::FromString(text).value();
  EXPECT_EQ(original, original);

  auto modified = original;
  modified.set_role(ObjectAccessControl::ROLE_READER());
  EXPECT_NE(original, modified);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
