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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
using namespace testing::canonical_errors;
namespace {
using namespace ::testing;
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
  ClientOptions client_options = ClientOptions(CreateInsecureCredentials());
};

TEST_F(ObjectTest, InsertObjectMedia) {
  std::string text = R"""({
      "name": "foo-bar/baz/1"
})""";
  auto expected = storage::ObjectMetadata::ParseFromJson(text);

  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Invoke(
          [&expected](internal::InsertObjectMediaRequest const& request) {
            EXPECT_EQ("foo-bar", request.bucket_name());
            EXPECT_EQ("baz", request.object_name());
            EXPECT_EQ("blah blah", request.contents());
            return std::make_pair(storage::Status(), expected);
          }));

  auto actual = client->InsertObject("foo-bar", "baz", "blah blah");
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, InsertObjectMediaTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_THROW(
      try {
        client.InsertObject("foo-bar", "baz", "blah blah");
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Retry policy exhausted"));
        EXPECT_THAT(ex.what(), HasSubstr("InsertObjectMedia"));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client.InsertObject("foo-bar", "baz", "blah blah"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectTest, InsertObjectMediaPermanentFailure) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_THROW(
      try {
        client->InsertObject("foo-bar", "baz", "blah blah");
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Permanent error"));
        EXPECT_THAT(ex.what(), HasSubstr("InsertObjectMedia"));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client->InsertObject("foo-bar", "baz", "blah blah"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectTest, GetObjectMetadata) {
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
  auto expected = ObjectMetadata::ParseFromJson(text);

  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(
          Invoke([&expected](internal::GetObjectMetadataRequest const& r) {
            EXPECT_EQ("foo-bar", r.bucket_name());
            EXPECT_EQ("baz", r.object_name());
            return std::make_pair(Status(), expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto actual = client.GetObjectMetadata("foo-bar", "baz");
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectTest, GetObjectMetadataTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_THROW(try { client.GetObjectMetadata("foo-bar", "baz"); } catch (
                   std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("Retry policy exhausted"));
    EXPECT_THAT(ex.what(), HasSubstr("GetObjectMetadata"));
    throw;
  },
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client.GetObjectMetadata("foo-bar", "baz"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectTest, GetObjectMetadataPermanentFailure) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillOnce(Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_THROW(try { client->GetObjectMetadata("foo-bar", "baz"); } catch (
                   std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("Permanent error"));
    EXPECT_THAT(ex.what(), HasSubstr("GetObjectMetadata"));
    throw;
  },
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetObjectMetadata(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client->GetObjectMetadata("foo-bar", "baz"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectTest, DeleteObject) {
  EXPECT_CALL(*mock, DeleteObject(_))
      .WillOnce(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})))
      .WillOnce(Invoke([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("foo-bar", r.bucket_name());
        EXPECT_EQ("baz", r.object_name());
        return std::make_pair(Status(), internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

  client.DeleteObject("foo-bar", "baz");
  SUCCEED();
}

TEST_F(ObjectTest, DeleteObjectTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, DeleteObject(_))
      .WillOnce(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})))
      .WillOnce(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})))
      .WillOnce(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})));
  EXPECT_THROW(try { client.DeleteObject("foo-bar", "baz"); } catch (
                   std::runtime_error const& ex) {
    EXPECT_THAT(ex.what(), HasSubstr("Retry policy exhausted"));
    EXPECT_THAT(ex.what(), HasSubstr("DeleteObject"));
    throw;
  },
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, DeleteObject(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})));
  EXPECT_DEATH_IF_SUPPORTED(client.DeleteObject("foo-bar", "baz"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
