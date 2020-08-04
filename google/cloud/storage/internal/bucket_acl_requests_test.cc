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

#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(BucketAclRequestTest, List) {
  ListBucketAclRequest request("my-bucket");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
}

TEST(BucketAclRequestTest, ListResponse) {
  std::string text = R"""({
      "items": [{
          "bucket": "test-bucket-name",
          "entity": "user-test-user-1",
          "role": "OWNER"
      }, {
          "bucket": "test-bucket-name",
          "entity": "user-test-user-2",
          "role": "READER"
      }]})""";

  auto actual = ListBucketAclResponse::FromHttpResponse(text).value();
  ASSERT_EQ(2UL, actual.items.size());
  EXPECT_EQ("user-test-user-1", actual.items.at(0).entity());
  EXPECT_EQ("OWNER", actual.items.at(0).role());
  EXPECT_EQ("user-test-user-2", actual.items.at(1).entity());
  EXPECT_EQ("READER", actual.items.at(1).role());

  std::ostringstream os;
  os << actual;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("entity=user-test-user-1"));
  EXPECT_THAT(str, HasSubstr("entity=user-test-user-2"));
  EXPECT_THAT(str, HasSubstr("ListBucketAclResponse={"));
  EXPECT_THAT(str, HasSubstr("BucketAccessControl={"));
}

TEST(BucketAclRequestTest, ListResponseParseFailure) {
  std::string text = "{123";

  StatusOr<ListBucketAclResponse> actual =
      ListBucketAclResponse::FromHttpResponse(text);
  EXPECT_FALSE(actual.ok());
}

TEST(BucketAclRequestTest, ListResponseParseFailureElements) {
  std::string text = R"""({"items": ["invalid-item"]})""";

  StatusOr<ListBucketAclResponse> actual =
      ListBucketAclResponse::FromHttpResponse(text);
  EXPECT_FALSE(actual.ok());
}

TEST(BucketAclRequestTest, Get) {
  GetBucketAclRequest request("my-bucket", "user-test-user");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("user-test-user", request.entity());
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-test-user"));
}

TEST(BucketAclRequestTest, Delete) {
  DeleteBucketAclRequest request("my-bucket", "user-test-user");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("user-test-user", request.entity());
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-test-user"));
}

TEST(CreateBucketAclRequestTest, Create) {
  CreateBucketAclRequest request("my-bucket", "user-testuser", "READER");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("user-testuser", request.entity());
  EXPECT_EQ("READER", request.role());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-testuser"));
  EXPECT_THAT(str, HasSubstr("READER"));
}

TEST(BucketAclRequestTest, Update) {
  UpdateBucketAclRequest request("my-bucket", "user-testuser", "READER");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("user-testuser", request.entity());
  EXPECT_EQ("READER", request.role());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-testuser"));
  EXPECT_THAT(str, HasSubstr("READER"));
}

BucketAccessControl CreateBucketAccessControlForTest() {
  std::string text = R"""({
      "bucket": "foo-bar",
      "domain": "example.com",
      "email": "foobar@example.com",
      "entity": "user-foobar",
      "entityId": "user-foobar-id-123",
      "etag": "XYZ=",
      "generation": 42,
      "id": "object-foo-bar-baz-acl-234",
      "kind": "storage#bucketAccessControl",
      "object": "baz",
      "projectTeam": {
        "projectNumber": "3456789",
        "team": "a-team"
      },
      "role": "OWNER"
})""";
  return internal::BucketAccessControlParser::FromString(text).value();
}

TEST(BucketAclRequestTest, PatchDiff) {
  BucketAccessControl original = CreateBucketAccessControlForTest();
  BucketAccessControl new_acl =
      CreateBucketAccessControlForTest().set_role("READER");

  PatchBucketAclRequest request("my-bucket", "user-test-user", original,
                                new_acl);
  nlohmann::json expected = {
      {"role", "READER"},
  };
  auto actual = nlohmann::json::parse(request.payload());
  EXPECT_EQ(expected, actual);
}

TEST(BucketAclRequestTest, PatchBuilder) {
  PatchBucketAclRequest request(
      "my-bucket", "user-test-user",
      BucketAccessControlPatchBuilder().set_role("READER").delete_entity());
  auto expected = nlohmann::json{{"role", "READER"}, {"entity", nullptr}};
  auto actual = nlohmann::json::parse(request.payload());
  EXPECT_EQ(expected, actual);
}

TEST(PatchObjectAclRequestTest, PatchStream) {
  BucketAccessControl original = CreateBucketAccessControlForTest();
  BucketAccessControl new_acl =
      CreateBucketAccessControlForTest().set_role("READER");

  PatchBucketAclRequest request("my-bucket", "user-test-user", original,
                                new_acl);
  request.set_multiple_options(UserProject("my-project"));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-test-user"));
  EXPECT_THAT(str, HasSubstr(request.payload()));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
