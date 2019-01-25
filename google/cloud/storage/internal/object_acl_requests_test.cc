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

#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

/// @test Verify that we parse JSON objects into ObjectAccessControl objects.
TEST(ObjectAccessControlTest, Parse) {
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
  auto actual = internal::ObjectAccessControlParser::FromString(text).value();

  EXPECT_EQ("foo-bar", actual.bucket());
  EXPECT_EQ("example.com", actual.domain());
  EXPECT_EQ("foobar@example.com", actual.email());
  EXPECT_EQ("user-foobar", actual.entity());
  EXPECT_EQ("user-foobar-id-123", actual.entity_id());
  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ(42, actual.generation());
  EXPECT_EQ("object-foo-bar-baz-acl-234", actual.id());
  EXPECT_EQ("storage#objectAccessControl", actual.kind());
  EXPECT_EQ("baz", actual.object());
  EXPECT_EQ("3456789", actual.project_team().project_number);
  EXPECT_EQ("a-team", actual.project_team().team);
  EXPECT_EQ("OWNER", actual.role());
}

/// @test Verify that the IOStream operator works as expected.
TEST(ObjectAccessControlTest, IOStream) {
  // The iostream operator is mostly there to support EXPECT_EQ() so it is
  // rarely called, and that breaks our code coverage metrics.
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

  auto meta = internal::ObjectAccessControlParser::FromString(text).value();
  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  using ::testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("ObjectAccessControl"));
  EXPECT_THAT(actual, HasSubstr("bucket=foo-bar"));
  EXPECT_THAT(actual, HasSubstr("object=baz"));
  EXPECT_THAT(actual, HasSubstr("id=object-foo-bar-baz-acl-234"));
}

TEST(ObjectAclRequestTest, List) {
  ListObjectAclRequest request("my-bucket", "my-object");
  request.set_multiple_options(UserProject("my-project"), Generation(7));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("generation=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
}

TEST(ObjectAclRequestTest, ListResponse) {
  std::string text = R"""({
      "items": [{
          "bucket": "foo-bar",
          "object": "baz",
          "entity": "user-qux",
          "role": "OWNER"
      }, {
          "bucket": "foo-bar",
          "object": "baz",
          "entity": "user-quux",
          "role": "READER"
      }]})""";

  auto actual =
      ListObjectAclResponse::FromHttpResponse(HttpResponse{200, text, {}})
          .value();
  ASSERT_EQ(2UL, actual.items.size());
  EXPECT_EQ("user-qux", actual.items.at(0).entity());
  EXPECT_EQ("OWNER", actual.items.at(0).role());
  EXPECT_EQ("user-quux", actual.items.at(1).entity());
  EXPECT_EQ("READER", actual.items.at(1).role());

  std::ostringstream os;
  os << actual;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("entity=user-qux"));
  EXPECT_THAT(str, HasSubstr("entity=user-quux"));
  EXPECT_THAT(str, HasSubstr("object=baz"));
  EXPECT_THAT(str, HasSubstr("ListObjectAclResponse={"));
  EXPECT_THAT(str, HasSubstr("ObjectAccessControl={"));
}

TEST(ObjectAclRequestTest, ListResponseParseFailure) {
  std::string text = R"""({123)""";

  StatusOr<ListObjectAclResponse> actual =
      ListObjectAclResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_FALSE(actual.ok());
}

TEST(ObjectAclRequestTest, ListResponseParseFailureElements) {
  std::string text = R"""({"items": ["invalid-item"]})""";

  StatusOr<ListObjectAclResponse> actual =
      ListObjectAclResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_FALSE(actual.ok());
}

TEST(ObjectAclRequestTest, Get) {
  GetObjectAclRequest request("my-bucket", "my-object", "user-test-user");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());
  EXPECT_EQ("user-test-user", request.entity());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-test-user"));
}

TEST(ObjectAclRequestTest, Delete) {
  DeleteObjectAclRequest request("my-bucket", "my-object", "user-test-user");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());
  EXPECT_EQ("user-test-user", request.entity());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-test-user"));
}

TEST(ObjectAclRequestTest, Create) {
  CreateObjectAclRequest request("my-bucket", "my-object", "user-testuser",
                                 "READER");
  request.set_multiple_options(UserProject("my-project"), Generation(7));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());
  EXPECT_EQ("user-testuser", request.entity());
  EXPECT_EQ("READER", request.role());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("generation=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("user-testuser"));
  EXPECT_THAT(str, HasSubstr("READER"));
}

TEST(ObjectAclRequestTest, Update) {
  UpdateObjectAclRequest request("my-bucket", "my-object", "user-testuser",
                                 "READER");
  request.set_multiple_options(UserProject("my-project"), Generation(7));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());
  EXPECT_EQ("user-testuser", request.entity());
  EXPECT_EQ("READER", request.role());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("generation=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("user-testuser"));
  EXPECT_THAT(str, HasSubstr("READER"));
}

ObjectAccessControl CreateObjectAccessControlForTest() {
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
  return internal::ObjectAccessControlParser::FromString(text).value();
}

TEST(ObjectAclRequestTest, PatchDiff) {
  ObjectAccessControl original = CreateObjectAccessControlForTest();
  ObjectAccessControl new_acl =
      CreateObjectAccessControlForTest().set_role("READER");

  PatchObjectAclRequest request("my-bucket", "my-object", "user-test-user",
                                original, new_acl);
  nl::json expected = {
      {"role", "READER"},
  };
  nl::json actual = nl::json::parse(request.payload());
  EXPECT_EQ(expected, actual);
}

TEST(ObjectAclRequestTest, PatchBuilder) {
  PatchObjectAclRequest request(
      "my-bucket", "my-object", "user-test-user",
      ObjectAccessControlPatchBuilder().set_role("READER").delete_entity());
  nl::json expected = {{"role", "READER"}, {"entity", nullptr}};
  nl::json actual = nl::json::parse(request.payload());
  EXPECT_EQ(expected, actual);
}

TEST(PatchObjectAclRequestTest, PatchStream) {
  ObjectAccessControl original = CreateObjectAccessControlForTest();
  ObjectAccessControl new_acl =
      CreateObjectAccessControlForTest().set_role("READER");

  PatchObjectAclRequest request("my-bucket", "my-object", "user-test-user",
                                original, new_acl);
  request.set_multiple_options(UserProject("my-project"), Generation(7),
                               IfMatchEtag("ABC="));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("If-Match: ABC="));
  EXPECT_THAT(str, HasSubstr("generation=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("user-test-user"));
  EXPECT_THAT(str, HasSubstr(request.payload()));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
