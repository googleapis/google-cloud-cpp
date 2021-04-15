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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::Return;
using ms = std::chrono::milliseconds;

/**
 * Test the functions in Storage::Client related to 'Objects: {copy,rewrite}'.
 *
 * In general, this file should include for the APIs listed in:
 *
 * https://cloud.google.com/storage/docs/json_api/v1/objects
 */
class ObjectCopyTest
    : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(ObjectCopyTest, CopyObject) {
  std::string text = R"""({"name": "test-bucket-name/test-object-name/1"})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, CopyObject)
      .WillOnce([&expected](internal::CopyObjectRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.destination_bucket());
        EXPECT_EQ("test-object-name", request.destination_object());
        EXPECT_EQ("source-bucket-name", request.source_bucket());
        EXPECT_EQ("source-object-name", request.source_object());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectMetadata> actual =
      client.CopyObject("source-bucket-name", "source-object-name",
                        "test-bucket-name", "test-object-name");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectCopyTest, CopyObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock_, EXPECT_CALL(*mock_, CopyObject),
      [](Client& client) {
        return client
            .CopyObject("source-bucket-name", "source-object-name",
                        "test-bucket-name", "test-object-name")
            .status();
      },
      [](Client& client) {
        return client
            .CopyObject("source-bucket-name", "source-object-name",
                        "test-bucket-name", "test-object-name",
                        IfGenerationMatch(0))
            .status();
      },
      "CopyObject");
}

TEST_F(ObjectCopyTest, CopyObjectPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      client, EXPECT_CALL(*mock_, CopyObject),
      [](Client& client) {
        return client
            .CopyObject("source-bucket-name", "source-object-name",
                        "test-bucket-name", "test-object-name")
            .status();
      },
      "CopyObject");
}

TEST_F(ObjectCopyTest, ComposeObject) {
  std::string response = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "new-disposition",
      "contentLanguage": "new-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/1",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://storage.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "1",
      "name": "test-object-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z",
      "componentCount": 2
  })""";
  auto expected = internal::ObjectMetadataParser::FromString(response).value();

  EXPECT_CALL(*mock_, ComposeObject)
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce([&expected](internal::ComposeObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        auto actual_payload = nlohmann::json::parse(r.JsonPayload());
        nlohmann::json expected_payload = {
            {"kind", "storage#composeRequest"},
            {"sourceObjects", {{{"name", "object1"}}, {{"name", "object2"}}}}};
        EXPECT_EQ(expected_payload, actual_payload);
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.ComposeObject("test-bucket-name",
                                     {{"object1", {}, {}}, {"object2", {}, {}}},
                                     "test-object-name");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectCopyTest, ComposeObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock_, EXPECT_CALL(*mock_, ComposeObject),
      [](Client& client) {
        return client
            .ComposeObject("test-bucket-name",
                           {{"object1", {}, {}}, {"object2", {}, {}}},
                           "test-object-name")
            .status();
      },
      [](Client& client) {
        return client
            .ComposeObject("test-bucket-name",
                           {{"object1", {}, {}}, {"object2", {}, {}}},
                           "test-object-name", IfGenerationMatch(7))
            .status();
      },
      "ComposeObject");
}

TEST_F(ObjectCopyTest, ComposeObjectPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      client, EXPECT_CALL(*mock_, ComposeObject),
      [](Client& client) {
        return client
            .ComposeObject("test-bucket-name",
                           {{"object1", {}, {}}, {"object2", {}, {}}},
                           "test-object-name")
            .status();
      },
      "ComposeObject");
}

TEST_F(ObjectCopyTest, RewriteObject) {
  EXPECT_CALL(*mock_, RewriteObject)
      .WillOnce(
          Return(StatusOr<internal::RewriteObjectResponse>(TransientError())))
      .WillOnce([](internal::RewriteObjectRequest const& r) {
        EXPECT_EQ("test-source-bucket-name", r.source_bucket());
        EXPECT_EQ("test-source-object-name", r.source_object());
        EXPECT_EQ("test-destination-bucket-name", r.destination_bucket());
        EXPECT_EQ("test-destination-object-name", r.destination_object());
        EXPECT_EQ("", r.rewrite_token());

        std::string response = R"""({
            "kind": "storage#rewriteResponse",
            "totalBytesRewritten": 1048576,
            "objectSize": 10485760,
            "done": false,
            "rewriteToken": "abcd-test-token-0"
        })""";
        return internal::RewriteObjectResponse::FromHttpResponse(response);
      })
      .WillOnce([](internal::RewriteObjectRequest const& r) {
        EXPECT_EQ("test-source-bucket-name", r.source_bucket());
        EXPECT_EQ("test-source-object-name", r.source_object());
        EXPECT_EQ("test-destination-bucket-name", r.destination_bucket());
        EXPECT_EQ("test-destination-object-name", r.destination_object());
        EXPECT_EQ("abcd-test-token-0", r.rewrite_token());

        std::string response = R"""({
            "kind": "storage#rewriteResponse",
            "totalBytesRewritten": 2097152,
            "objectSize": 10485760,
            "done": false,
            "rewriteToken": "abcd-test-token-2"
        })""";
        return internal::RewriteObjectResponse::FromHttpResponse(response);
      })
      .WillOnce([](internal::RewriteObjectRequest const& r) {
        EXPECT_EQ("test-source-bucket-name", r.source_bucket());
        EXPECT_EQ("test-source-object-name", r.source_object());
        EXPECT_EQ("test-destination-bucket-name", r.destination_bucket());
        EXPECT_EQ("test-destination-object-name", r.destination_object());
        EXPECT_EQ("abcd-test-token-2", r.rewrite_token());

        std::string response = R"""({
            "kind": "storage#rewriteResponse",
            "totalBytesRewritten": 10485760,
            "objectSize": 10485760,
            "done": true,
            "rewriteToken": "",
            "resource": {
               "bucket": "test-destination-bucket-name",
               "name": "test-destination-object-name"
            }
        })""";
        return internal::RewriteObjectResponse::FromHttpResponse(response);
      });
  auto client = ClientForMock();
  auto copier = client.RewriteObject(
      "test-source-bucket-name", "test-source-object-name",
      "test-destination-bucket-name", "test-destination-object-name",
      WithObjectMetadata(
          ObjectMetadata().upsert_metadata("test-key", "test-value")));
  auto actual = copier.Iterate();
  ASSERT_STATUS_OK(actual);
  EXPECT_FALSE(actual->done);
  EXPECT_EQ(1048576UL, actual->total_bytes_rewritten);
  EXPECT_EQ(10485760UL, actual->object_size);

  auto current = copier.CurrentProgress();
  ASSERT_STATUS_OK(current);
  EXPECT_FALSE(current->done);
  EXPECT_EQ(1048576UL, current->total_bytes_rewritten);
  EXPECT_EQ(10485760UL, current->object_size);

  actual = copier.Iterate();
  ASSERT_STATUS_OK(actual);
  EXPECT_FALSE(actual->done);
  EXPECT_EQ(2097152UL, actual->total_bytes_rewritten);
  EXPECT_EQ(10485760UL, actual->object_size);

  auto metadata = copier.Result();
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ("test-destination-bucket-name", metadata->bucket());
  EXPECT_EQ("test-destination-object-name", metadata->name());
}

TEST_F(ObjectCopyTest, RewriteObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::RewriteObjectResponse>(
      mock_, EXPECT_CALL(*mock_, RewriteObject),
      [](Client& client) {
        auto rewrite = client.RewriteObject(
            "test-source-bucket-name", "test-source-object",
            "test-dest-bucket-name", "test-dest-object");
        return rewrite.Result().status();
      },
      [](Client& client) {
        return client
            .RewriteObjectBlocking("test-source-bucket-name",
                                   "test-source-object",
                                   "test-dest-bucket-name", "test-dest-object",
                                   IfGenerationMatch(7))
            .status();
      },
      "RewriteObject");
}

TEST_F(ObjectCopyTest, RewriteObjectPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::RewriteObjectResponse>(
      client, EXPECT_CALL(*mock_, RewriteObject),
      [](Client& client) {
        auto rewrite = client.RewriteObject(
            "test-source-bucket-name", "test-source-object",
            "test-dest-bucket-name", "test-dest-object");
        return rewrite.Result().status();
      },
      "RewriteObject");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
