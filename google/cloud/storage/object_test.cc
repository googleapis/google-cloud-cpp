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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;

/**
 * Test the functions in Storage::Client related to 'Objects: *'.
 *
 * In general, this file should include for the APIs listed in:
 *
 * https://cloud.google.com/storage/docs/json_api/v1/objects
 */
class ObjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{std::shared_ptr<internal::RawClient>(mock)});
  }
  void TearDown() override {
    client.reset();
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
  std::unique_ptr<Client> client;
  ClientOptions client_options =
      ClientOptions(oauth2::CreateAnonymousCredentials());
};

TEST_F(ObjectTest, InsertObjectMedia) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Invoke(
          [&expected](internal::InsertObjectMediaRequest const& request) {
            EXPECT_EQ("test-bucket-name", request.bucket_name());
            EXPECT_EQ("test-object-name", request.object_name());
            EXPECT_EQ("test object contents", request.contents());
            return make_status_or(expected);
          }));

  auto actual = client->InsertObject("test-bucket-name", "test-object-name",
                                     "test object contents");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, InsertObjectMediaTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, InsertObjectMedia(_)),
      [](Client& client) {
        return client
            .InsertObject("test-bucket-name", "test-object-name",
                          "test object contents")
            .status();
      },
      [](Client& client) {
        return client
            .InsertObject("test-bucket-name", "test-object-name",
                          "test object contents", IfGenerationMatch(0))
            .status();
      },
      "InsertObjectMedia");
}

TEST_F(ObjectTest, InsertObjectMediaPermanentFailure) {
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, InsertObjectMedia(_)),
      [](Client& client) {
        return client
            .InsertObject("test-bucket-name", "test-object-name",
                          "test object contents")
            .status();
      },
      "InsertObjectMedia");
}

TEST_F(ObjectTest, GetObjectMetadata) {
  std::string text = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "a-disposition",
      "contentLanguage": "a-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://www.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "4",
      "name": "test-object-name",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce(
          Invoke([&expected](internal::GetObjectMetadataRequest const& r) {
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_EQ("test-object-name", r.object_name());
            return make_status_or(expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto actual =
      client.GetObjectMetadata("test-bucket-name", "test-object-name");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, GetObjectMetadataTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, GetObjectMetadata(_)),
      [](Client& client) {
        return client.GetObjectMetadata("test-bucket-name", "test-object-name")
            .status();
      },
      "GetObjectMetadata");
}

TEST_F(ObjectTest, GetObjectMetadataPermanentFailure) {
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, GetObjectMetadata(_)),
      [](Client& client) {
        return client.GetObjectMetadata("test-bucket-name", "test-object-name")
            .status();
      },
      "GetObjectMetadata");
}

TEST_F(ObjectTest, DeleteObject) {
  EXPECT_CALL(*mock, DeleteObject(_))
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce(Invoke([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

  auto status = client.DeleteObject("test-bucket-name", "test-object-name");
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, DeleteObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteObject(_)),
      [](Client& client) {
        return client.DeleteObject("test-bucket-name", "test-object-name");
      },
      [](Client& client) {
        return client.DeleteObject("test-bucket-name", "test-object-name",
                                   IfGenerationMatch(7));
      },
      "DeleteObject");
}

TEST_F(ObjectTest, DeleteObjectPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteObject(_)),
      [](Client& client) {
        return client.DeleteObject("test-bucket-name", "test-object-name");
      },
      "DeleteObject");
}

TEST_F(ObjectTest, UpdateObject) {
  std::string text = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "new-disposition",
      "contentLanguage": "new-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://www.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "4",
      "name": "test-object-name",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, UpdateObject(_))
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce(Invoke([&expected](internal::UpdateObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        internal::nl::json actual_payload =
            internal::nl::json::parse(r.json_payload());
        internal::nl::json expected_payload = {
            {"acl",
             internal::nl::json{
                 {{"entity", "user-test-user"}, {"role", "READER"}},
             }},
            {"cacheControl", "no-cache"},
            {"contentDisposition", "new-disposition"},
            {"contentEncoding", "new-encoding"},
            {"contentLanguage", "new-language"},
            {"contentType", "new-type"},
            {"eventBasedHold", false},
            {"metadata",
             internal::nl::json{
                 {"test-label", "test-value"},
             }},
        };
        EXPECT_EQ(expected_payload, actual_payload);
        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  ObjectMetadata update;
  update.mutable_acl().push_back(
      ObjectAccessControl().set_entity("user-test-user").set_role("READER"));
  update.set_cache_control("no-cache")
      .set_content_disposition("new-disposition")
      .set_content_encoding("new-encoding")
      .set_content_language("new-language")
      .set_content_type("new-type");
  update.mutable_metadata().emplace("test-label", "test-value");
  auto actual =
      client.UpdateObject("test-bucket-name", "test-object-name", update);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, UpdateObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, UpdateObject(_)),
      [](Client& client) {
        return client
            .UpdateObject("test-bucket-name", "test-object-name",
                          ObjectMetadata().set_content_language("new-language"))
            .status();
      },
      [](Client& client) {
        return client
            .UpdateObject("test-bucket-name", "test-object-name",
                          ObjectMetadata().set_content_language("new-language"),
                          IfMetagenerationMatch(42))
            .status();
      },
      "UpdateObject");
}

TEST_F(ObjectTest, UpdateObjectPermanentFailure) {
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, UpdateObject(_)),
      [](Client& client) {
        return client
            .UpdateObject("test-bucket-name", "test-object-name",
                          ObjectMetadata().set_content_language("new-language"))
            .status();
      },
      "UpdateObject");
}

TEST_F(ObjectTest, PatchObject) {
  std::string text = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "new-disposition",
      "contentLanguage": "new-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://www.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "4",
      "name": "test-object-name",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, PatchObject(_))
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce(Invoke([&expected](internal::PatchObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        EXPECT_THAT(r.payload(), HasSubstr("new-disposition"));
        EXPECT_THAT(r.payload(), HasSubstr("x-made-up-lang"));
        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto actual = client.PatchObject("test-bucket-name", "test-object-name",
                                   ObjectMetadataPatchBuilder()
                                       .SetContentDisposition("new-disposition")
                                       .SetContentLanguage("x-made-up-lang"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, PatchObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, PatchObject(_)),
      [](Client& client) {
        return client
            .PatchObject(
                "test-bucket-name", "test-object-name",
                ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"))
            .status();
      },
      [](Client& client) {
        return client
            .PatchObject(
                "test-bucket-name", "test-object-name",
                ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"),
                IfMetagenerationMatch(42))
            .status();
      },
      "PatchObject");
}

TEST_F(ObjectTest, PatchObjectPermanentFailure) {
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, PatchObject(_)),
      [](Client& client) {
        return client
            .PatchObject(
                "test-bucket-name", "test-object-name",
                ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"))
            .status();
      },
      "PatchObject");
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
