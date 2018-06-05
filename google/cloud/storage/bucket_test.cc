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

#include "storage/client/bucket.h"
#include <gmock/gmock.h>

using namespace storage;
using namespace ::testing;

namespace {

class MockClient : public storage::Client {
 public:
  using BucketGetResult = std::pair<storage::Status, storage::BucketMetadata>;
  MOCK_METHOD1(GetBucketMetadata, BucketGetResult(std::string const&));
};

inline Status UNAVAILABLE() { return Status{503, std::string{"try-again"}}; }
inline Status NOT_FOUND() { return Status{404, std::string{"not found"}}; }
}  // anonymous namespace

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
  auto expected = storage::BucketMetadata::ParseFromJson(text);

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata("foo-bar-baz"))
      .WillOnce(Return(std::make_pair(UNAVAILABLE(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(storage::Status(), expected)));
  Bucket bucket(mock, "foo-bar-baz");

  auto actual = bucket.GetMetadata();
  EXPECT_EQ(expected, actual);
}

TEST(BucketTest, GetMetadataTooManyFailures) {
  auto mock = std::make_shared<MockClient>();
  Bucket bucket(mock, "foo-bar-baz");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(UNAVAILABLE(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(UNAVAILABLE(), BucketMetadata{})))
      .WillOnce(Return(std::make_pair(UNAVAILABLE(), BucketMetadata{})));
  EXPECT_THROW(bucket.GetMetadata(), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillRepeatedly(Return(std::make_pair(UNAVAILABLE(), BucketMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(bucket.GetMetadata(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(BucketTest, GetMetadataPermanentFailure) {
  auto mock = std::make_shared<MockClient>();
  Bucket bucket(mock, "foo-bar-baz");

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(std::make_pair(NOT_FOUND(), BucketMetadata{})));
  EXPECT_THROW(bucket.GetMetadata(), std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillRepeatedly(Return(std::make_pair(NOT_FOUND(), BucketMetadata{})));
  EXPECT_DEATH_IF_SUPPORTED(bucket.GetMetadata(), "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
