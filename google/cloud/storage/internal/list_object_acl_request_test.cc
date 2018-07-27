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

#include "google/cloud/storage/internal/list_object_acl_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(ListObjectAclRequestTest, Simple) {
  ListObjectAclRequest request("my-bucket", "my-object");
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());
}

using ::testing::HasSubstr;

TEST(ListObjectAclRequestTest, Stream) {
  ListObjectAclRequest request("my-bucket", "my-object");
  request.set_multiple_options(UserProject("my-project"), Generation(7));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("generation=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
}

TEST(ListObjectAclResponseTest, Parse) {
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
      ListObjectAclResponse::FromHttpResponse(HttpResponse{200, text, {}});
  ASSERT_EQ(2UL, actual.items.size());
  EXPECT_EQ("user-qux", actual.items.at(0).entity());
  EXPECT_EQ("OWNER", actual.items.at(0).role());
  EXPECT_EQ("user-quux", actual.items.at(1).entity());
  EXPECT_EQ("READER", actual.items.at(1).role());
}

TEST(ListObjectAclResponseTest, Stream) {
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

  auto list =
      ListObjectAclResponse::FromHttpResponse(HttpResponse{200, text, {}});
  std::ostringstream os;
  os << list;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("entity=user-qux"));
  EXPECT_THAT(str, HasSubstr("entity=user-quux"));
  EXPECT_THAT(str, HasSubstr("object=baz"));
  EXPECT_THAT(str, HasSubstr("ListObjectAclResponse={"));
  EXPECT_THAT(str, HasSubstr("ObjectAccessControl={"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
