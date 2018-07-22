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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

BucketMetadata CreateBucketMetadataForTest() {
  // This metadata object has some impossible combination of fields in it. The
  // goal is to fully test the parsing, not to simulate valid objects.
  std::string text = R"""({
      "acl": [{
        "kind": "storage#bucketAccessControl",
        "id": "acl-id-0",
        "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket/acl/user-test-user",
        "bucket": "test-bucket",
        "entity": "user-test-user",
        "role": "OWNER",
        "email": "test-user@example.com",
        "entityId": "user-test-user-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "owners"
        },
        "etag": "AYX="
      }, {
        "kind": "storage#objectAccessControl",
        "id": "acl-id-1",
        "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket/acl/user-test-user2",
        "bucket": "test-bucket",
        "entity": "user-test-user2",
        "role": "READER",
        "email": "test-user2@example.com",
        "entityId": "user-test-user2-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "viewers"
        },
        "etag": "AYX="
      }
      ],
      "billing": {
        "requesterPays": false
      },
      "etag": "XYZ=",
      "id": "test-bucket",
      "kind": "storage#bucket",
      "labels": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "test-bucket",
      "projectNumber": "123456789",
      "location": "US",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  return BucketMetadata::ParseFromString(text);
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataTest, Parse) {
  auto actual = CreateBucketMetadataForTest();

  EXPECT_EQ(2U, actual.acl().size());
  EXPECT_EQ("acl-id-0", actual.acl().at(0).id());
  EXPECT_EQ("acl-id-1", actual.acl().at(1).id());
  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ("test-bucket", actual.id());
  EXPECT_EQ("storage#bucket", actual.kind());
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

  EXPECT_EQ("US", actual.location());
  EXPECT_EQ(4, actual.metageneration());
  EXPECT_EQ("test-bucket", actual.name());
  EXPECT_EQ(123456789, actual.project_number());
  EXPECT_EQ("https://www.googleapis.com/storage/v1/b/test-bucket",
            actual.self_link());
  EXPECT_EQ(BucketMetadata::STORAGE_CLASS_STANDARD, actual.storage_class());
  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  auto magic_timestamp = 1526758274L;
  using std::chrono::duration_cast;
  EXPECT_EQ(magic_timestamp, duration_cast<std::chrono::seconds>(
                             actual.time_created().time_since_epoch())
                             .count());
  EXPECT_EQ(magic_timestamp + 10, duration_cast<std::chrono::seconds>(
                             actual.updated().time_since_epoch())
                             .count());

}

/// @test Verify that the IOStream operator works as expected.
TEST(BucketMetadataTest, IOStream) {
  auto meta = CreateBucketMetadataForTest();

  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  using ::testing::HasSubstr;
  EXPECT_THAT(actual, HasSubstr("BucketMetadata"));
  EXPECT_THAT(actual, HasSubstr("acl-id-0"));
  EXPECT_THAT(actual, HasSubstr("acl-id-1"));
  EXPECT_THAT(actual, HasSubstr("bucket=test-bucket"));
  EXPECT_THAT(actual, HasSubstr("name=test-bucket"));
  EXPECT_THAT(actual, HasSubstr("labels.foo=bar"));
}

/// @test Verify we can make changes to one Acl in BucketMetadata.
TEST(BucketMetadataTest, MutableAcl) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_acl().at(0).set_role(BucketAccessControl::ROLE_READER());
  copy.mutable_acl().at(1).set_role(BucketAccessControl::ROLE_OWNER());
  EXPECT_EQ("READER", copy.acl().at(0).role());
  EXPECT_EQ("OWNER", copy.acl().at(1).role());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the full acl in BucketMetadata.
TEST(BucketMetadataTest, SetAcl) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  auto acl = expected.acl();
  acl.at(0).set_role(BucketAccessControl::ROLE_READER());
  acl.at(1).set_role(BucketAccessControl::ROLE_OWNER());
  copy.set_acl(std::move(acl));
  EXPECT_NE(expected, copy);
  EXPECT_EQ("READER", copy.acl().at(0).role());
}

/// @test Verify we can change the billing configuration in BucketMetadata.
TEST(BucketMetadataTest, SetBilling) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  auto billing = copy.billing();
  billing.requester_pays = not billing.requester_pays;
  copy.set_billing(billing);
  EXPECT_NE(expected, copy);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
