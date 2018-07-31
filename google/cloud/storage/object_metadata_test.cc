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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

ObjectMetadata CreateObjectMetadataForTest() {
  // This metadata object has some impossible combination of fields in it. The
  // goal is to fully test the parsing, not to simulate valid objects.
  std::string text = R"""({
      "acl": [{
        "kind": "storage#objectAccessControl",
        "id": "acl-id-0",
        "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/o/baz/acl/user-qux",
        "bucket": "foo-bar",
        "object": "foo",
        "generation": 12345,
        "entity": "user-qux",
        "role": "OWNER",
        "email": "qux@example.com",
        "entityId": "user-qux-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "owners"
        },
        "etag": "AYX="
      }, {
        "kind": "storage#objectAccessControl",
        "id": "acl-id-1",
        "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/o/baz/acl/user-quux",
        "bucket": "foo-bar",
        "object": "foo",
        "generation": 12345,
        "entity": "user-quux",
        "role": "READER",
        "email": "qux@example.com",
        "entityId": "user-quux-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "viewers"
        },
        "etag": "AYX="
      }
      ],
      "bucket": "foo-bar",
      "cacheControl": "no-cache",
      "componentCount": 7,
      "contentDisposition": "a-disposition",
      "contentEncoding": "an-encoding",
      "contentLanguage": "a-language",
      "contentType": "application/octet-stream",
      "crc32c": "deadbeef",
      "customerEncryption": {
        "encryptionAlgorithm": "some-algo",
        "keySha256": "abc123"
      },
      "etag": "XYZ=",
      "generation": "12345",
      "id": "foo-bar/baz/12345",
      "kind": "storage#object",
      "kmsKeyName": "/foo/bar/baz/key",
      "md5Hash": "deaderBeef=",
      "mediaLink": "https://www.googleapis.com/storage/v1/b/foo-bar/o/baz?generation=12345&alt=media",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "baz",
      "owner": {
        "entity": "user-qux",
        "entityId": "user-qux-id-123"
      },
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar/o/baz",
      "size": 102400,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  return ObjectMetadata::ParseFromString(text);
}

/// @test Verify that we parse JSON objects into ObjectMetadata objects.
TEST(ObjectMetadataTest, Parse) {
  auto actual = CreateObjectMetadataForTest();

  EXPECT_EQ(2U, actual.acl().size());
  EXPECT_EQ("acl-id-0", actual.acl().at(0).id());
  EXPECT_EQ("foo-bar", actual.bucket());
  EXPECT_EQ("no-cache", actual.cache_control());
  EXPECT_EQ(7, actual.component_count());
  EXPECT_EQ("a-disposition", actual.content_disposition());
  EXPECT_EQ("an-encoding", actual.content_encoding());
  EXPECT_EQ("application/octet-stream", actual.content_type());
  EXPECT_EQ("deadbeef", actual.crc32c());
  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ(12345, actual.generation());
  EXPECT_EQ("foo-bar/baz/12345", actual.id());
  EXPECT_EQ("storage#object", actual.kind());
  EXPECT_EQ("/foo/bar/baz/key", actual.kms_key_name());
  EXPECT_EQ("deaderBeef=", actual.md5_hash());
  EXPECT_EQ(
      "https://www.googleapis.com/storage/v1/b/foo-bar/o/"
      "baz?generation=12345&alt=media",
      actual.media_link());
  EXPECT_EQ(2U, actual.metadata_count());
  EXPECT_TRUE(actual.has_metadata("foo"));
  EXPECT_EQ("bar", actual.metadata("foo"));
  EXPECT_EQ(4, actual.metageneration());
  EXPECT_EQ("baz", actual.name());
  EXPECT_EQ("user-qux", actual.owner().entity);
  EXPECT_EQ("user-qux-id-123", actual.owner().entity_id);
  EXPECT_EQ("https://www.googleapis.com/storage/v1/b/foo-bar/o/baz",
            actual.self_link());
  EXPECT_EQ(102400U, actual.size());
  EXPECT_EQ("STANDARD", actual.storage_class());
  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  auto magic_timestamp = 1526758274L;
  using std::chrono::duration_cast;
  EXPECT_EQ(magic_timestamp, duration_cast<std::chrono::seconds>(
                                 actual.time_created().time_since_epoch())
                                 .count());
  // +70 seconds from magic.
  EXPECT_EQ(magic_timestamp + 70, duration_cast<std::chrono::seconds>(
                                      actual.time_deleted().time_since_epoch())
                                      .count());
  // +20 seconds from magic
  EXPECT_EQ(magic_timestamp + 20,
            duration_cast<std::chrono::seconds>(
                actual.time_storage_class_updated().time_since_epoch())
                .count());
  // +10 seconds from magic
  EXPECT_EQ(magic_timestamp + 10, duration_cast<std::chrono::seconds>(
                                      actual.updated().time_since_epoch())
                                      .count());
}

/// @test Verify that the IOStream operator works as expected.
TEST(ObjectMetadataTest, IOStream) {
  auto meta = CreateObjectMetadataForTest();
  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  using ::testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("ObjectMetadata={"));
  EXPECT_THAT(actual, HasSubstr("acl-id-0"));
  EXPECT_THAT(actual, HasSubstr("name=baz"));
  EXPECT_THAT(actual, HasSubstr("metadata.foo=bar"));
  EXPECT_THAT(actual, HasSubstr("size=102400"));
}

/// @test Verify we can make changes to one Acl in ObjectMetadata.
TEST(ObjectMetadataTest, MutableAcl) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_acl().at(0).set_role(ObjectAccessControl::ROLE_READER());
  copy.mutable_acl().at(1).set_role(ObjectAccessControl::ROLE_OWNER());
  EXPECT_EQ("READER", copy.acl().at(0).role());
  EXPECT_EQ("OWNER", copy.acl().at(1).role());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the full acl in ObjectMetadata.
TEST(ObjectMetadataTest, SetAcl) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  auto acl = expected.acl();
  acl.at(0).set_role(ObjectAccessControl::ROLE_READER());
  acl.at(1).set_role(ObjectAccessControl::ROLE_OWNER());
  copy.set_acl(std::move(acl));
  EXPECT_NE(expected, copy);
  EXPECT_EQ("READER", copy.acl().at(0).role());
}

/// @test Verify we can change the cacheControl field.
TEST(ObjectMetadataTest, SetCacheControl) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  copy.set_cache_control("fancy-cache-control");
  EXPECT_EQ("fancy-cache-control", copy.cache_control());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the contentDisposition field.
TEST(ObjectMetadataTest, SetContentDisposition) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  copy.set_content_disposition("some-other-disposition");
  EXPECT_EQ("some-other-disposition", copy.content_disposition());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the contentEncoding field.
TEST(ObjectMetadataTest, SetContentEncoding) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  copy.set_content_encoding("some-other-encoding");
  EXPECT_EQ("some-other-encoding", copy.content_encoding());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the contentLanguage field.
TEST(ObjectMetadataTest, SetContentLanguage) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  copy.set_content_language("some-other-language");
  EXPECT_EQ("some-other-language", copy.content_language());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the contentType field.
TEST(ObjectMetadataTest, SetContentType) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  copy.set_content_type("some-other-type");
  EXPECT_EQ("some-other-type", copy.content_type());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can delete metadata fields.
TEST(ObjectMetadataTest, DeleteMetadata) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.has_metadata("foo"));
  copy.delete_metadata("foo");
  EXPECT_FALSE(copy.has_metadata("foo"));
  EXPECT_FALSE(copy.has_metadata("not-there"));
  copy.delete_metadata("not-there");
  EXPECT_FALSE(copy.has_metadata("not-there"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change metadata existing metadata fields.
TEST(ObjectMetadataTest, ChangeMetadata) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.has_metadata("foo"));
  copy.upsert_metadata("foo", "some-new-value");
  EXPECT_EQ("some-new-value", copy.metadata("foo"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change insert new metadata fields.
TEST(ObjectMetadataTest, InsertMetadata) {
  auto expected = CreateObjectMetadataForTest();
  auto copy = expected;
  EXPECT_FALSE(copy.has_metadata("not-there"));
  copy.upsert_metadata("not-there", "now-it-is");
  EXPECT_EQ("now-it-is", copy.metadata("not-there"));
  EXPECT_NE(expected, copy);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
