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

#include "google/cloud/storage/internal/list_objects_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

TEST(ListObjectsRequestTest, Simple) {
  ListObjectsRequest request("my-bucket");

  EXPECT_EQ("my-bucket", request.bucket_name());

  request.set_option(Prefix("foo/"));
}

using ::testing::HasSubstr;

TEST(ListObjectsRequestTest, OStreamBasic) {
  ListObjectsRequest request("my-bucket");
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
}

TEST(ListObjectsRequestTest, OStreamParameter) {
  ListObjectsRequest request("my-bucket");
  request.set_multiple_options(UserProject("my-project"),
                               Prefix("foo-bar-baz/"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("userProject=my-project"));
  EXPECT_THAT(os.str(), HasSubstr("prefix=foo-bar-baz/"));
}

TEST(ListObjectsResponseTest, Parse) {
  std::string object1 = R"""({
      "bucket": "foo-bar",
      "etag": "XYZ=",
      "id": "baz",
      "kind": "storage#object",
      "generation": 1,
      "location": "US",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "foo-bar-baz",
      "projectNumber": "123456789",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/baz/1",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  std::string object2 = R"""({
      "bucket": "foo-bar",
      "etag": "XYZ=",
      "id": "qux",
      "kind": "storage#object",
      "generation": "7",
      "location": "US",
      "metadata": {
        "lbl1": "bar",
        "lbl2": "qux"
      },
      "metageneration": "4",
      "name": "qux",
      "projectNumber": "123456789",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/qux/7",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  std::string text = R"""({
      "kind": "storage#buckets",
      "nextPageToken": "some-token-42",
      "items":
)""";
  text += "[" + object1 + "," + object2 + "]}";

  auto o1 = ObjectMetadata::ParseFromString(object1);
  auto o2 = ObjectMetadata::ParseFromString(object2);

  auto actual =
      ListObjectsResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_EQ("some-token-42", actual.next_page_token);
  EXPECT_THAT(actual.items, ::testing::ElementsAre(o1, o2));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
