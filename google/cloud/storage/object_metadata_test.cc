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

#include "google/cloud/storage/object_metadata.h"
#include <gmock/gmock.h>

/// @test Verify that we parse JSON objects into ObjectMetadata objects.
TEST(ObjectMetadataTest, Parse) {
  std::string text = R"""({
      "kind": "storage#object",
      "id": "foo-bar/baz/12345",
      "name": "baz",
      "bucket": "foo-bar",
      "generation": "12345",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/o/baz",
      "mediaLink": "https://www.googleapis.com/download/storage/v1/b/foo-bar/o/baz?generation=12345&alt=media",
      "contentType": "application/octet-stream",
      "contentDisposition": "a-disposition",
      "contentLanguage": "a-language",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "metageneration": "4",
      "storageClass": "STANDARD",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "size": 1024,
      "md5Hash": "xa1b2c3==",
      "crc32c": "d1e2f3",
      "etag": "XYZ="
})""";
  auto actual = storage::ObjectMetadata::ParseFromJson(text);

  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ("foo-bar/baz/12345", actual.id());
  EXPECT_EQ("storage#object", actual.kind());
  EXPECT_EQ(0U, actual.label_count());
  EXPECT_EQ(4, actual.metageneration());
  EXPECT_EQ("baz", actual.name());
  EXPECT_EQ("foo-bar", actual.bucket());
  EXPECT_EQ(12345, actual.generation());
  EXPECT_EQ("https://www.googleapis.com/storage/v1/b/foo-bar/o/baz",
            actual.self_link());
  EXPECT_EQ("STANDARD", actual.storage_class());
  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  using std::chrono::duration_cast;
  EXPECT_EQ(1526758274L, duration_cast<std::chrono::seconds>(
                             actual.time_created().time_since_epoch())
                             .count());
  EXPECT_EQ(1526758284L, duration_cast<std::chrono::seconds>(
                             actual.updated().time_since_epoch())
                             .count());
}

/// @test Verify that we parse JSON objects into ObjectMetadata objects.
TEST(ObjectMetadataTest, ParseWithLabels) {
  std::string text = R"""({
      "kind": "storage#object",
      "id": "foo-bar-baz",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ=",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      }
})""";
  auto actual = storage::ObjectMetadata::ParseFromJson(text);

  EXPECT_EQ(2U, actual.label_count());
  EXPECT_TRUE(actual.has_label("foo"));
  EXPECT_EQ("bar", actual.label("foo"));
  EXPECT_FALSE(actual.has_label("qux"));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(actual.label("qux"), std::exception);
#else
  // We accept any output here because the actual output depends on the
  // C++ library implementation.
  EXPECT_DEATH_IF_SUPPORTED(actual.label("qux"), "");
#endif  // GOOGLE_CLOUD_CPP_EXCEPTIONS
}

/// @test Verify that the IOStream operator works as expected.
TEST(ObjectMetadataTest, IOStream) {
  // The iostream operator is mostly there to support EXPECT_EQ() so it is
  // rarely called, and that breaks our code coverage metrics.
  std::string text = R"""({
      "kind": "storage#object",
      "id": "foo-bar-baz",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ=",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      }
})""";

  auto meta = storage::ObjectMetadata::ParseFromJson(text);
  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  using ::testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("name=foo-bar-baz"));
  EXPECT_THAT(actual, HasSubstr("metadata.foo=bar"));
}
