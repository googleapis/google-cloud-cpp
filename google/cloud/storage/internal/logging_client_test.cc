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
#include "google/cloud/log.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;

class MockLogBackend : public google::cloud::LogBackend {
 public:
  // Cannot use `override` here because if we do the build breaks: the compiler
  // on macOS then expects *all* overriden functions to have it, and the
  // MOCK_METHOD1() macro does not use override.
  void Process(LogRecord const& lr) { ProcessWithOwnership(lr); }

  MOCK_METHOD1(ProcessWithOwnership, void(LogRecord));
  // For the purposes of testing we just need one of the member functions.
};

class LoggingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend = std::make_shared<MockLogBackend>();
    log_backend_id = google::cloud::LogSink::Instance().AddBackend(log_backend);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id);
    log_backend_id = 0;
    log_backend.reset();
  }

  std::shared_ptr<MockLogBackend> log_backend = nullptr;
  long log_backend_id = 0;
};

TEST_F(LoggingClientTest, GetBucketMetadata) {
  std::string text = R"""({
      "kind": "storage#bucket",
      "id": "my-bucket",
      "location": "US",
      "name": "my-bucket"
})""";

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(
          Return(internal::BucketMetadataParser::FromString(text).value()));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend, ProcessWithOwnership(_))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("GetBucketMetadataRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      }))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("US"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      }));

  LoggingClient client(mock);
  client.GetBucketMetadata(GetBucketMetadataRequest("my-bucket"));
}

TEST_F(LoggingClientTest, GetBucketMetadataWithError) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend, ProcessWithOwnership(_))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("GetBucketMetadataRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      }))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("status={"));
      }));

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
  EXPECT_CALL(*mock, InsertObjectMedia(_))
      .WillOnce(
          Return(internal::ObjectMetadataParser::FromString(text).value()));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend, ProcessWithOwnership(_))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("InsertObjectMediaRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("foo-bar"));
        EXPECT_THAT(lr.message, HasSubstr("baz"));
        EXPECT_THAT(lr.message, HasSubstr("the contents"));
      }))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("foo-bar"));
        EXPECT_THAT(lr.message, HasSubstr("baz"));
      }));

  LoggingClient client(mock);
  client.InsertObjectMedia(
      InsertObjectMediaRequest("foo-bar", "baz", "the contents"));
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
  EXPECT_CALL(*mock, ListObjects(_))
      .WillOnce(Return(make_status_or(ListObjectsResponse{"a-token", items})));

  // We want to test that the key elements are logged, but do not want a
  // "change detection test", so this is intentionally not exhaustive.
  EXPECT_CALL(*log_backend, ProcessWithOwnership(_))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr("ListObjectsRequest={"));
        EXPECT_THAT(lr.message, HasSubstr("my-bucket"));
      }))
      .WillOnce(Invoke([](LogRecord lr) {
        EXPECT_THAT(lr.message, HasSubstr(" >> "));
        EXPECT_THAT(lr.message, HasSubstr("payload={"));
        EXPECT_THAT(lr.message, HasSubstr("ListObjectsResponse={"));
        EXPECT_THAT(lr.message, HasSubstr("a-token"));
        EXPECT_THAT(lr.message, HasSubstr("response-object-o1"));
        EXPECT_THAT(lr.message, HasSubstr("response-object-o2"));
      }));

  LoggingClient client(mock);
  client.ListObjects(ListObjectsRequest("my-bucket"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
