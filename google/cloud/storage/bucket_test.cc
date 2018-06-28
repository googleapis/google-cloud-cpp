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

class BucketTest : public ::testing::Test {
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

TEST_F(BucketTest, GetBucketMetadata) {
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
  auto expected = BucketMetadata::ParseFromJson(text);

  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})))
      .WillOnce(
          Invoke([&expected](internal::GetBucketMetadataRequest const& r) {
            EXPECT_EQ("foo-bar-baz", r.bucket_name());
            return std::make_pair(Status(), expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

  auto actual = client.GetBucketMetadata("foo-bar-baz");
  EXPECT_EQ(expected, actual);
}

TEST_F(BucketTest, GetMetadataTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})));
  EXPECT_THROW(client.GetBucketMetadata("foo-bar-baz"), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), BucketMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client.GetBucketMetadata("foo-bar-baz"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketTest, GetMetadataPermanentFailure) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(PermanentError(), BucketMetadata{})));
  EXPECT_THROW(client->GetBucketMetadata("foo-bar-baz"), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), BucketMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client.GetBucketMetadata("foo-bar-baz"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketTest, InsertObjectMedia) {
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

TEST_F(BucketTest, InsertObjectMediaTooManyFailures) {
  using ms = std::chrono::milliseconds;
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(ms(100), ms(500), 2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_THROW(client.InsertObject("foo-bar", "baz", "blah blah"),
               std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(client->InsertObject("foo-bar", "baz", "blah blah"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketTest, InsertObjectMediaPermanentFailure) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_THROW(client->InsertObject("foo-bar", "baz", "blah blah"),
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
}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
