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

namespace storage = google::cloud::storage;

/// @test Verify that we parse JSON objects into ObjectMetadata objects.
TEST(ObjectMetadataTest, Parse) {
  std::string text = R"""({
      "bucket": "foo-bar",
      "contentDisposition": "a-disposition",
      "contentLanguage": "a-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "foo-bar/baz/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://www.googleapis.com/download/storage/v1/b/foo-bar/o/baz?generation=12345&alt=media",
      "metageneration": "4",
      "name": "baz",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/o/baz",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto actual = storage::ObjectMetadata::ParseFromJson(text);

  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ("foo-bar/baz/12345", actual.id());
  EXPECT_EQ("storage#object", actual.kind());
  EXPECT_EQ(0U, actual.metadata_count());
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
TEST(ObjectMetadataTest, ParseWithMetadata) {
  std::string text = R"""({
     "etag": "XYZ=",
      "id": "foo-bar-baz",
      "kind": "storage#object",
      "location": "US",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "foo-bar-baz",
      "projectNumber": "123456789",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto actual = storage::ObjectMetadata::ParseFromJson(text);

  EXPECT_EQ(2U, actual.metadata_count());
  EXPECT_TRUE(actual.has_metadata("foo"));
  EXPECT_EQ("bar", actual.metadata("foo"));
  EXPECT_FALSE(actual.has_metadata("qux"));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(actual.metadata("qux"), std::exception);
#else
  // We accept any output here because the actual output depends on the
  // C++ library implementation.
  EXPECT_DEATH_IF_SUPPORTED(actual.metadata("qux"), "");
#endif  // GOOGLE_CLOUD_CPP_EXCEPTIONS
}

/// @test Verify that the IOStream operator works as expected.
TEST(ObjectMetadataTest, IOStream) {
  // The iostream operator is mostly there to support EXPECT_EQ() so it is
  // rarely called, and that breaks our code coverage metrics.
  std::string text = R"""({
     "etag": "XYZ=",
      "id": "foo-bar-baz",
      "kind": "storage#object",
      "location": "US",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "foo-bar-baz",
      "projectNumber": "123456789",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";

  auto meta = storage::ObjectMetadata::ParseFromJson(text);
  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  using ::testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("name=foo-bar-baz"));
  EXPECT_THAT(actual, HasSubstr("metadata.foo=bar"));
}
