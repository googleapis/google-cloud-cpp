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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;
using testing::canonical_errors::TransientError;

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
  auto expected = storage::ObjectMetadata::ParseFromString(text);

  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Invoke(
          [&expected](internal::InsertObjectMediaRequest const& request) {
            EXPECT_EQ("test-bucket-name", request.bucket_name());
            EXPECT_EQ("test-object-name", request.object_name());
            EXPECT_EQ("test object contents", request.contents());
            return std::make_pair(storage::Status(), expected);
          }));

  auto actual = client->InsertObject("test-bucket-name", "test-object-name",
                                     "test object contents");
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, InsertObjectMediaTooManyFailures) {
  testing::TooManyFailuresTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, InsertObjectMedia(_)),
      [](Client& client) {
        client.InsertObject("test-bucket-name", "test-object-name",
                            "test object contents");
      },
      "InsertObjectMedia");
}

TEST_F(ObjectTest, InsertObjectMediaPermanentFailure) {
  testing::PermanentFailureTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, InsertObjectMedia(_)),
      [](Client& client) {
        client.InsertObject("test-bucket-name", "test-object-name",
                            "test object contents");
      },
      "InsertObjectMedia");
}

TEST_F(ObjectTest, CopyObject) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = storage::ObjectMetadata::ParseFromString(text);

  EXPECT_CALL(*mock, CopyObject(_))
      .WillOnce(Invoke([&expected](internal::CopyObjectRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.destination_bucket());
        EXPECT_EQ("test-object-name", request.destination_object());
        EXPECT_EQ("source-bucket-name", request.source_bucket());
        EXPECT_EQ("source-object-name", request.source_object());
        EXPECT_THAT(request.json_payload(), HasSubstr("text/plain"));
        return std::make_pair(storage::Status(), expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  ObjectMetadata actual = client.CopyObject(
      "source-bucket-name", "source-object-name", "test-bucket-name",
      "test-object-name", ObjectMetadata().set_content_type("text/plain"));
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, CopyObjectTooManyFailures) {
  testing::TooManyFailuresTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, CopyObject(_)),
      [](Client& client) {
        client.CopyObject("source-bucket-name", "source-object-name",
                          "test-bucket-name", "test-object-name",
                          ObjectMetadata());
      },
      "CopyObject");
}

TEST_F(ObjectTest, CopyObjectPermanentFailure) {
  testing::PermanentFailureTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, CopyObject(_)),
      [](Client& client) {
        client.CopyObject("source-bucket-name", "source-object-name",
                          "test-bucket-name", "test-object-name",
                          ObjectMetadata());
      },
      "CopyObject");
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
  auto expected = ObjectMetadata::ParseFromString(text);

  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(
          Invoke([&expected](internal::GetObjectMetadataRequest const& r) {
            EXPECT_EQ("test-bucket-name", r.bucket_name());
            EXPECT_EQ("test-object-name", r.object_name());
            return std::make_pair(Status(), expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto actual =
      client.GetObjectMetadata("test-bucket-name", "test-object-name");
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, GetObjectMetadataTooManyFailures) {
  testing::TooManyFailuresTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, GetObjectMetadata(_)),
      [](Client& client) {
        client.GetObjectMetadata("test-bucket-name", "test-object-name");
      },
      "GetObjectMetadata");
}

TEST_F(ObjectTest, GetObjectMetadataPermanentFailure) {
  testing::PermanentFailureTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, GetObjectMetadata(_)),
      [](Client& client) {
        client.GetObjectMetadata("test-bucket-name", "test-object-name");
      },
      "GetObjectMetadata");
}

TEST_F(ObjectTest, DeleteObject) {
  EXPECT_CALL(*mock, DeleteObject(_))
      .WillOnce(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})))
      .WillOnce(Invoke([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return std::make_pair(Status(), internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

  client.DeleteObject("test-bucket-name", "test-object-name");
  SUCCEED();
}

TEST_F(ObjectTest, DeleteObjectTooManyFailures) {
  testing::TooManyFailuresTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteObject(_)),
      [](Client& client) {
        client.DeleteObject("test-bucket-name", "test-object-name");
      },
      "DeleteObject");
}

TEST_F(ObjectTest, DeleteObjectPermanentFailure) {
  testing::PermanentFailureTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteObject(_)),
      [](Client& client) {
        client.DeleteObject("test-bucket-name", "test-object-name");
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
  auto expected = ObjectMetadata::ParseFromString(text);

  EXPECT_CALL(*mock, UpdateObject(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
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
        return std::make_pair(Status(), expected);
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
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, UpdateObjectTooManyFailures) {
  testing::TooManyFailuresTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, UpdateObject(_)),
      [](Client& client) {
        client.UpdateObject(
            "test-bucket-name", "test-object-name",
            ObjectMetadata().set_content_language("new-language"));
      },
      "UpdateObject");
}

TEST_F(ObjectTest, UpdateObjectPermanentFailure) {
  testing::PermanentFailureTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, UpdateObject(_)),
      [](Client& client) {
        client.UpdateObject(
            "test-bucket-name", "test-object-name",
            ObjectMetadata().set_content_language("new-language"));
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
  auto expected = ObjectMetadata::ParseFromString(text);

  EXPECT_CALL(*mock, PatchObject(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Invoke([&expected](internal::PatchObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        EXPECT_THAT(r.payload(), HasSubstr("new-disposition"));
        EXPECT_THAT(r.payload(), HasSubstr("x-made-up-lang"));
        return std::make_pair(Status(), expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto actual = client.PatchObject("test-bucket-name", "test-object-name",
                                   ObjectMetadataPatchBuilder()
                                       .SetContentDisposition("new-disposition")
                                       .SetContentLanguage("x-made-up-lang"));
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, PatchObjectTooManyFailures) {
  testing::TooManyFailuresTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, PatchObject(_)),
      [](Client& client) {
        client.PatchObject(
            "test-bucket-name", "test-object-name",
            ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"));
      },
      "PatchObject");
}

TEST_F(ObjectTest, PatchObjectPermanentFailure) {
  testing::PermanentFailureTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, PatchObject(_)),
      [](Client& client) {
        client.PatchObject(
            "test-bucket-name", "test-object-name",
            ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"));
      },
      "PatchObject");
}

TEST_F(ObjectTest, ComposeObject) {
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
      "mediaLink": "https://www.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "1",
      "name": "test-object-name",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z",
      "componentCount": 2
})""";
  auto expected = ObjectMetadata::ParseFromString(response);

  EXPECT_CALL(*mock, ComposeObject(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Invoke([&expected](internal::ComposeObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        internal::nl::json actual_payload =
            internal::nl::json::parse(r.json_payload());
        internal::nl::json expected_payload = {
            {"kind", "storage#composeRequest"},
            {"sourceObjects", {{{"name", "object1"}}, {{"name", "object2"}}}}};
        EXPECT_EQ(expected_payload, actual_payload);
        return std::make_pair(Status(), expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto actual =
      client.ComposeObject("test-bucket-name", {{"object1"}, {"object2"}},
                           "test-object-name", ObjectMetadata());
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, ComposeObjectTooManyFailures) {
  testing::TooManyFailuresTest<ObjectMetadata>(
      mock, EXPECT_CALL(*mock, ComposeObject(_)),
      [](Client& client) {
        client.ComposeObject("test-bucket-name", {{"object1"}, {"object2"}},
                             "test-object-name", ObjectMetadata());
      },
      "ComposeObject");
}

TEST_F(ObjectTest, ComposeObjectPermanentFailure) {
  testing::PermanentFailureTest<ObjectMetadata>(
      *client, EXPECT_CALL(*mock, ComposeObject(_)),
      [](Client& client) {
        client.ComposeObject("test-bucket-name", {{"object1"}, {"object2"}},
                             "test-object-name", ObjectMetadata());
      },
      "ComposeObject");
}

TEST_F(ObjectTest, RewriteObject) {
  EXPECT_CALL(*mock, RewriteObject(_))
      .WillOnce(Return(
          std::make_pair(TransientError(), internal::RewriteObjectResponse{})))
      .WillOnce(Invoke([](internal::RewriteObjectRequest const& r) {
        EXPECT_EQ("test-source-bucket-name", r.source_bucket());
        EXPECT_EQ("test-source-object-name", r.source_object());
        EXPECT_EQ("test-destination-bucket-name", r.destination_bucket());
        EXPECT_EQ("test-destination-object-name", r.destination_object());
        EXPECT_EQ("", r.rewrite_token());

        EXPECT_THAT(r.json_payload(), HasSubstr("test-key"));
        EXPECT_THAT(r.json_payload(), HasSubstr("test-value"));
        std::string response = R"""({
            "kind": "storage#rewriteResponse",
            "totalBytesRewritten": 1048576,
            "objectSize": 10485760,
            "done": false,
            "rewriteToken": "abcd-test-token-0"
        })""";
        return std::make_pair(Status(),
                              internal::RewriteObjectResponse::FromHttpResponse(
                                  internal::HttpResponse{200, response, {}}));
      }))
      .WillOnce(Invoke([](internal::RewriteObjectRequest const& r) {
        EXPECT_EQ("test-source-bucket-name", r.source_bucket());
        EXPECT_EQ("test-source-object-name", r.source_object());
        EXPECT_EQ("test-destination-bucket-name", r.destination_bucket());
        EXPECT_EQ("test-destination-object-name", r.destination_object());
        EXPECT_EQ("abcd-test-token-0", r.rewrite_token());

        EXPECT_THAT(r.json_payload(), HasSubstr("test-key"));
        EXPECT_THAT(r.json_payload(), HasSubstr("test-value"));
        std::string response = R"""({
            "kind": "storage#rewriteResponse",
            "totalBytesRewritten": 2097152,
            "objectSize": 10485760,
            "done": false,
            "rewriteToken": "abcd-test-token-2"
        })""";
        return std::make_pair(Status(),
                              internal::RewriteObjectResponse::FromHttpResponse(
                                  internal::HttpResponse{200, response, {}}));
      }))
      .WillOnce(Invoke([](internal::RewriteObjectRequest const& r) {
        EXPECT_EQ("test-source-bucket-name", r.source_bucket());
        EXPECT_EQ("test-source-object-name", r.source_object());
        EXPECT_EQ("test-destination-bucket-name", r.destination_bucket());
        EXPECT_EQ("test-destination-object-name", r.destination_object());
        EXPECT_EQ("abcd-test-token-2", r.rewrite_token());

        EXPECT_THAT(r.json_payload(), HasSubstr("test-key"));
        EXPECT_THAT(r.json_payload(), HasSubstr("test-value"));
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
        return std::make_pair(Status(),
                              internal::RewriteObjectResponse::FromHttpResponse(
                                  internal::HttpResponse{200, response, {}}));
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto copier = client.RewriteObject(
      "test-source-bucket-name", "test-source-object-name",
      "test-destination-bucket-name", "test-destination-object-name",
      ObjectMetadata().upsert_metadata("test-key", "test-value"));
  auto actual = copier.Iterate();
  EXPECT_FALSE(actual.done);
  EXPECT_EQ(1048576UL, actual.total_bytes_rewritten);
  EXPECT_EQ(10485760UL, actual.object_size);

  auto current = copier.CurrentProgress();
  EXPECT_FALSE(current.done);
  EXPECT_EQ(1048576UL, current.total_bytes_rewritten);
  EXPECT_EQ(10485760UL, current.object_size);

  actual = copier.Iterate();
  EXPECT_FALSE(actual.done);
  EXPECT_EQ(2097152UL, actual.total_bytes_rewritten);
  EXPECT_EQ(10485760UL, actual.object_size);

  auto metadata = copier.Result();
  EXPECT_EQ("test-destination-bucket-name", metadata.bucket());
  EXPECT_EQ("test-destination-object-name", metadata.name());
}

TEST_F(ObjectTest, RewriteObjectTooManyFailures) {
  testing::TooManyFailuresTest<internal::RewriteObjectResponse>(
      mock, EXPECT_CALL(*mock, RewriteObject(_)),
      [](Client& client) {
        auto rewrite = client.RewriteObject(
            "test-source-bucket-name", "test-source-object",
            "test-dest-bucket-name", "test-dest-object", ObjectMetadata());
        rewrite.Result();
      },
      "RewriteObject");
}

TEST_F(ObjectTest, RewriteObjectPermanentFailure) {
  testing::PermanentFailureTest<internal::RewriteObjectResponse>(
      *client, EXPECT_CALL(*mock, RewriteObject(_)),
      [](Client& client) {
        auto rewrite = client.RewriteObject(
            "test-source-bucket-name", "test-source-object",
            "test-dest-bucket-name", "test-dest-object", ObjectMetadata());
        rewrite.Result();
      },
      "RewriteObject");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
