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

#include "google/cloud/storage/internal/default_object_acl_requests.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(DefaultObjectAclRequestTest, List) {
  ListDefaultObjectAclRequest request("my-bucket");
  request.set_multiple_options(UserProject("my-project"),
                               IfMetagenerationMatch(42),
                               IfMetagenerationNotMatch(7));
  EXPECT_EQ("my-bucket", request.bucket_name());
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("ifMetagenerationMatch=42"));
  EXPECT_THAT(str, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
}

TEST(DefaultObjectAclRequestTest, ListResponse) {
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

  auto actual = ListDefaultObjectAclResponse::FromHttpResponse(text).value();
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
  EXPECT_THAT(str, HasSubstr("ListDefaultObjectAclResponse={"));
  EXPECT_THAT(str, HasSubstr("ObjectAccessControl={"));
}

TEST(DefaultObjectAclRequestTest, ListResponseFailure) {
  std::string text = R"""({123)""";

  StatusOr<ListDefaultObjectAclResponse> actual =
      ListDefaultObjectAclResponse::FromHttpResponse(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(DefaultObjectAclRequestTest, ListResponseParseFailureElements) {
  std::string text = R"""({"items": ["invalid-item"]})""";

  StatusOr<ListDefaultObjectAclResponse> actual =
      ListDefaultObjectAclResponse::FromHttpResponse(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(DefaultObjectAclRequestTest, Get) {
  GetDefaultObjectAclRequest request("my-bucket", "user-testuser");
  request.set_multiple_options(UserProject("my-project"), IfMatchEtag("ABC="));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("user-testuser", request.entity());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-testuser"));
  EXPECT_THAT(str, HasSubstr("If-Match: ABC="));
}

TEST(DefaultObjectAclRequestTest, Delete) {
  DeleteDefaultObjectAclRequest request("my-bucket", "user-testuser");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("user-testuser", request.entity());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("user-testuser"));
}

TEST(DefaultObjectAclRequestTest, Create) {
  CreateDefaultObjectAclRequest request("my-bucket", "user-testuser", "READER");
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

TEST(DefaultObjectAclRequestTest, Update) {
  UpdateDefaultObjectAclRequest request("my-bucket", "user-testuser", "READER");
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

ObjectAccessControl CreateDefaultObjectAccessControlForTest() {
  std::string text = R"""({
      "bucket": "test-bucket",
      "domain": "example.com",
      "email": "test-user@example.com",
      "entity": "user-test-user",
      "entityId": "user-test-user-id-123",
      "etag": "XYZ=",
      "id": "test-id-123",
      "kind": "storage#objectAccessControl",
      "projectTeam": {
        "projectNumber": "3456789",
        "team": "a-team"
      },
      "role": "OWNER"
})""";
  return internal::ObjectAccessControlParser::FromString(text).value();
}

TEST(DefaultObjectAclRequestTest, PatchDiff) {
  ObjectAccessControl original = CreateDefaultObjectAccessControlForTest();
  ObjectAccessControl new_acl =
      CreateDefaultObjectAccessControlForTest().set_role("READER");

  PatchDefaultObjectAclRequest request("my-bucket", "user-test-user", original,
                                       new_acl);
  nlohmann::json expected = {
      {"role", "READER"},
  };
  auto actual = nlohmann::json::parse(request.payload());
  EXPECT_EQ(expected, actual);
}

TEST(DefaultObjectAclRequestTest, PatchBuilder) {
  PatchDefaultObjectAclRequest request(
      "my-bucket", "user-test-user",
      ObjectAccessControlPatchBuilder().set_role("READER").delete_entity());
  nlohmann::json expected = {{"role", "READER"}, {"entity", nullptr}};
  auto actual = nlohmann::json::parse(request.payload());
  EXPECT_EQ(expected, actual);
}

TEST(DefaultObjectAclRequestTest, PatchStream) {
  ObjectAccessControl original = CreateDefaultObjectAccessControlForTest();
  ObjectAccessControl new_acl =
      CreateDefaultObjectAccessControlForTest().set_role("READER");

  PatchDefaultObjectAclRequest request("my-bucket", "user-test-user", original,
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
