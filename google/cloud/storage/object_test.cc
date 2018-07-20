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
  ClientOptions client_options = ClientOptions(CreateInsecureCredentials());
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

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
