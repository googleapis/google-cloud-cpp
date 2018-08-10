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

#include "google/cloud/storage/internal/bucket_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;
using ::testing::Not;

TEST(GetBucketMetadataRequestTest, OStreamBasic) {
  GetBucketMetadataRequest request("my-bucket");
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
}

TEST(GetBucketMetadataRequestTest, OStreamParameter) {
  GetBucketMetadataRequest request("my-bucket");
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=my-project"));
}

TEST(ListBucketsRequestTest, Simple) {
  ListBucketsRequest request("my-project");
  EXPECT_EQ("my-project", request.project_id());
  request.set_option(Prefix("foo-"));
}

TEST(ListBucketsRequestTest, OStream) {
  ListBucketsRequest request("project-to-list");
  request.set_multiple_options(UserProject("project-to-bill"),
                               Prefix("foo-bar-baz"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("ListBucketsRequest"));
  EXPECT_THAT(os.str(), HasSubstr("project_id=project-to-list"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=project-to-bill"));
  EXPECT_THAT(os.str(), HasSubstr("prefix=foo-bar-baz"));
}

TEST(ListBucketsResponseTest, Parse) {
  std::string bucket1 = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz-1",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz-1",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  std::string bucket2 = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz-2",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz-2",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  std::string text = R"""({
      "kind": "storage#buckets",
      "nextPageToken": "some-token-42",
      "items":
)""";
  text += "[" + bucket1 + "," + bucket2 + "]}";

  auto b1 = BucketMetadata::ParseFromString(bucket1);
  auto b2 = BucketMetadata::ParseFromString(bucket2);

  auto actual =
      ListBucketsResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_EQ("some-token-42", actual.next_page_token);
  EXPECT_THAT(actual.items, ::testing::ElementsAre(b1, b2));
}

TEST(CreateBucketsRequestTest, Basic) {
  CreateBucketRequest request("project-for-new-bucket",
                              BucketMetadata().set_name("test-bucket"));
  request.set_multiple_options(UserProject("project-to-bill"),
                               PredefinedAcl("projectPrivate"));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("CreateBucketRequest"));
  EXPECT_THAT(actual, HasSubstr("project_id=project-for-new-bucket"));
  EXPECT_THAT(actual, HasSubstr("userProject=project-to-bill"));
  EXPECT_THAT(actual, HasSubstr("predefinedAcl=projectPrivate"));
  EXPECT_THAT(actual, HasSubstr("name=test-bucket"));
}

TEST(DeleteBucketRequestTest, OStream) {
  DeleteBucketRequest request("my-bucket");
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
}

TEST(PatchBucketRequestTest, Diff) {
  BucketMetadata original = BucketMetadata::ParseFromString(R"""({
      "kind": "storage#bucket",
      "id": "test-bucket",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket",
      "projectNumber": "123456789",
      "name": "test-bucket",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""");
  BucketMetadata updated = original;
  updated.set_location("EU").set_storage_class("NEARLINE");
  PatchBucketRequest request("test-bucket", original, updated);
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("NEARLINE"));
  EXPECT_THAT(actual, HasSubstr("EU"));
  EXPECT_THAT(actual, Not(HasSubstr("defaultObjectAcl")));
}

TEST(PatchBucketRequestTest, Builder) {
  PatchBucketRequest request("test-bucket", BucketMetadataPatchBuilder()
                                                .SetStorageClass("NEARLINE")
                                                .ResetDefaultAcl());
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("NEARLINE"));
  EXPECT_THAT(actual, Not(HasSubstr("EU")));
  EXPECT_THAT(actual, HasSubstr("defaultObjectAcl"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
