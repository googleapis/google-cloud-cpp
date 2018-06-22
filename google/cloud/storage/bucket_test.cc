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

#include "google/cloud/storage/bucket.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace storage = google::cloud::storage;
using storage::Bucket;
using storage::BucketMetadata;
using storage::ObjectMetadata;
using namespace ::testing;
using storage::internal::GetBucketMetadataRequest;
using storage::internal::InsertObjectMediaRequest;
using storage::testing::MockClient;
using namespace storage::testing::canonical_errors;

TEST(BucketTest, GetMetadata) {
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
  auto expected = google::cloud::storage::BucketMetadata::ParseFromJson(text);

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})))
      .WillOnce(Invoke([&expected](GetBucketMetadataRequest const& r) {
        EXPECT_EQ("foo-bar-baz", r.bucket_name());
        return std::make_pair(storage::Status(), expected);
      }));
  Bucket bucket(mock, "foo-bar-baz");

  auto actual = bucket.GetMetadata();
  EXPECT_EQ(expected, actual);
}

TEST(BucketTest, GetMetadataTooManyFailures) {
  auto mock = std::make_shared<MockClient>();
  Bucket bucket(mock, "foo-bar-baz");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), BucketMetadata{})));
  EXPECT_THROW(bucket.GetMetadata(), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), BucketMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(bucket.GetMetadata(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(BucketTest, GetMetadataPermanentFailure) {
  auto mock = std::make_shared<MockClient>();
  Bucket bucket(mock, "foo-bar-baz");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(PermanentError(), BucketMetadata{})));
  EXPECT_THROW(bucket.GetMetadata(), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), BucketMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(bucket.GetMetadata(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(BucketTest, InsertObjectMedia) {
  std::string text = R"""({
      "name": "foo-bar/baz/1"
})""";
  auto expected = storage::ObjectMetadata::ParseFromJson(text);

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Invoke([&expected](InsertObjectMediaRequest const& request) {
        EXPECT_EQ("foo-bar", request.bucket_name());
        EXPECT_EQ("baz", request.object_name());
        EXPECT_EQ("blah blah", request.contents());
        return std::make_pair(storage::Status(), expected);
      }));
  Bucket bucket(mock, "foo-bar");

  auto actual = bucket.InsertObject("baz", "blah blah");
  EXPECT_EQ(expected, actual);
}

TEST(BucketTest, InsertObjectMediaTooManyFailures) {
  auto mock = std::make_shared<MockClient>();
  Bucket bucket(mock, "foo-bar");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_THROW(bucket.InsertObject("baz", "blah blah"), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(bucket.InsertObject("baz", "blah blah"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(BucketTest, InsertObjectMediaPermanentFailure) {
  auto mock = std::make_shared<MockClient>();
  Bucket bucket(mock, "foo-bar");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_THROW(bucket.InsertObject("baz", "blah blah"), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), ObjectMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(bucket.InsertObject("baz", "blah blah"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
