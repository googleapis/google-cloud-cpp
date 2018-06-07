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

#include "google/cloud/storage/bucket_metadata.h"
#include <gmock/gmock.h>

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataTest, Parse) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  auto actual = storage::BucketMetadata::ParseFromJson(text);

  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ("foo-bar-baz", actual.id());
  EXPECT_EQ("storage#bucket", actual.kind());
  EXPECT_EQ(0U, actual.label_count());
  EXPECT_EQ("US", actual.location());
  EXPECT_EQ(4, actual.metadata_generation());
  EXPECT_EQ("foo-bar-baz", actual.name());
  EXPECT_EQ(123456789, actual.project_number());
  EXPECT_EQ("https://www.googleapis.com/storage/v1/b/foo-bar-baz",
            actual.self_link());
  EXPECT_EQ(storage::BucketMetadata::STORAGE_CLASS_STANDARD,
            actual.storage_class());
  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  using std::chrono::duration_cast;
  EXPECT_EQ(1526758274L, duration_cast<std::chrono::seconds>(
                             actual.time_created().time_since_epoch())
                             .count());
  EXPECT_EQ(1526758284L, duration_cast<std::chrono::seconds>(
                             actual.time_updated().time_since_epoch())
                             .count());
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataTest, ParseWithLabels) {
  std::string text = R"""({
      "kind": "storage#bucket",
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
      "labels": {
        "foo": "bar",
        "baz": "qux"
      }
})""";
  auto actual = storage::BucketMetadata::ParseFromJson(text);

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
TEST(BucketMetadataTest, IOStream) {
  // The iostream operator is mostly there to support EXPECT_EQ() so it is
  // rarely called, and that breaks our code coverage metrics.
  std::string text = R"""({
      "kind": "storage#bucket",
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
      "labels": {
        "foo": "bar",
        "baz": "qux"
      }
})""";

  auto meta = storage::BucketMetadata::ParseFromJson(text);
  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  using ::testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("name=foo-bar-baz"));
  EXPECT_THAT(actual, HasSubstr("labels.foo=bar"));
}
