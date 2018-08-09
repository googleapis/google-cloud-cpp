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
  request.set_multiple_options(UserProject("my-project"), Generation(7));
  EXPECT_EQ("my-bucket", request.bucket_name());
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("generation=7"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
}

TEST(BucketAclResponseTest, Simple) {
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

  auto actual =
      ListBucketAclResponse::FromHttpResponse(HttpResponse{200, text, {}});
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

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
