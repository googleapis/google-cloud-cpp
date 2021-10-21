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

#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::HasSubstr;
using ::testing::Return;

class MockLogBackend : public google::cloud::LogBackend {
 public:
  void Process(LogRecord const& lr) override { ProcessWithOwnership(lr); }

  MOCK_METHOD(void, ProcessWithOwnership, (google::cloud::LogRecord),
              (override));
  // For the purposes of testing we just need one of the member functions.
};

class LoggingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend_ = std::make_shared<MockLogBackend>();
    log_backend_id_ =
        google::cloud::LogSink::Instance().AddBackend(log_backend_);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id_);
    log_backend_id_ = 0;
    log_backend_.reset();
  }

  std::shared_ptr<MockLogBackend> log_backend_ = nullptr;
  long log_backend_id_ = 0;  // NOLINT(google-runtime-int)
};

TEST_F(LoggingClientTest, GetBucketMetadata) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "my-bucket",
      "location": "US",
      "name": "my-bucket"
})""";

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata)
      .WillOnce(
          Return(internal::BucketMetadataParser::FromString(text).value()));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("GetBucketMetadataRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("US"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      });

  LoggingClient client(mock);
  client.GetBucketMetadata(GetBucketMetadataRequest("my-bucket"));
}

TEST_F(LoggingClientTest, GetBucketMetadataWithError) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("GetBucketMetadataRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("status={"));
      });

  LoggingClient client(mock);
  client.GetBucketMetadata(GetBucketMetadataRequest("my-bucket"));
}

TEST_F(LoggingClientTest, InsertObjectMedia) {
  std::string text = R"""({
      "bucket": "foo-bar",
      "metageneration": "4",
      "name": "baz"
})""";

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce(
          Return(internal::ObjectMetadataParser::FromString(text).value()));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("InsertObjectMediaRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("foo-bar"));
        EXPECT_THAT(lr.message, HasSubstr("baz"));
        EXPECT_THAT(lr.message, HasSubstr("the contents"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("foo-bar"));
        EXPECT_THAT(lr.message, HasSubstr("baz"));
      });

  LoggingClient client(mock);
  client.InsertObjectMedia(
      InsertObjectMediaRequest("foo-bar", "baz", "the contents"));
}

TEST_F(LoggingClientTest, ListBuckets) {
  std::vector<BucketMetadata> items = {
      internal::BucketMetadataParser::FromString(
          R"""({
            "name": "response-bucket-b1",
            "id": "response-bucket-b1",
            "kind": "storage#bucket",
            "location": "US"
})""")
          .value(),
      internal::BucketMetadataParser::FromString(
          R"""({
            "name": "response-bucket-b2",
            "id": "response-bucket-b2",
            "kind": "storage#bucket",
            "location": "CN"
})""")
          .value(),
  };
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce(Return(make_status_or(ListBucketsResponse{"a-token", items})));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("ListBucketsRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("ListBucketsResponse={"));
        EXPECT_THAT(lr.message, HasSubstr("a-token"));
        EXPECT_THAT(lr.message, HasSubstr("response-bucket-b1"));
        EXPECT_THAT(lr.message, HasSubstr("US"));
        EXPECT_THAT(lr.message, HasSubstr("response-bucket-b2"));
        EXPECT_THAT(lr.message, HasSubstr("storage#bucket"));
        EXPECT_THAT(lr.message, HasSubstr("CN"));
      });

  LoggingClient client(mock);
  client.ListBuckets(ListBucketsRequest("my-bucket"));
}

TEST_F(LoggingClientTest, ListObjects) {
  std::vector<ObjectMetadata> items = {
      internal::ObjectMetadataParser::FromString(
          R""({"name": "response-object-o1"})"")
          .value(),
      internal::ObjectMetadataParser::FromString(
          R""({"name": "response-object-o2"})"")
          .value(),
  };
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(
          Return(make_status_or(ListObjectsResponse{"a-token", items, {}})));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("ListObjectsRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("ListObjectsResponse={"));
        EXPECT_THAT(lr.message, HasSubstr("a-token"));
        EXPECT_THAT(lr.message, HasSubstr("response-object-o1"));
        EXPECT_THAT(lr.message, HasSubstr("response-object-o2"));
      });

  LoggingClient client(mock);
  client.ListObjects(ListObjectsRequest("my-bucket"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
