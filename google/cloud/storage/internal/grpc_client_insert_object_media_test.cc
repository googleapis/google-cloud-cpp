// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::TransferStallTimeoutOption;
using ::google::cloud::storage::internal::InsertObjectMediaRequest;
using ::google::cloud::storage::testing::MockInsertStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;

/// @verify that small objects are inserted with a single Write() call.
TEST(GrpcClientInsertObjectMediaTest, Small) {
  auto constexpr kResponseText =
      R"pb(resource {
             bucket: "test-bucket"
             name: "test-object"
             generation: 12345
           })pb";
  google::storage::v2::WriteObjectResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(kResponseText, &response));

  auto constexpr kWriteRequestText = R"pb(
    write_object_spec {
      resource: { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
    }
    checksummed_data {
      content: "The quick brown fox jumps over the lazy dog"
      # grpc_client_object_request_test.cc documents this magic value
      crc32c: 0x22620404
      # MD5 is disabled by default
    }
    object_checksums {
      crc32c: 0x22620404
      # MD5 is disabled by default
    }
    finish_write: true
  )pb";
  google::storage::v2::WriteObjectRequest write_request;
  ASSERT_TRUE(TextFormat::ParseFromString(kWriteRequestText, &write_request));

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Write(IsProtoEqual(write_request), _))
            .WillOnce(Return(true));
        EXPECT_CALL(*stream, Close).WillOnce(Return(response));
        return stream;
      });

  auto client = GrpcClient::CreateMock(mock);
  auto metadata = client->InsertObjectMedia(
      InsertObjectMediaRequest("test-bucket", "test-object",
                               "The quick brown fox jumps over the lazy dog"));
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(metadata->bucket(), "test-bucket");
  EXPECT_EQ(metadata->name(), "test-object");
  EXPECT_EQ(metadata->generation(), 12345);
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientInsertObjectMediaTest, StallTimeoutWrite) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Close)
            .WillOnce(Return(google::storage::v2::WriteObjectResponse{}));
        return stream;
      });

  auto const expected = std::chrono::seconds(42);
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(std::chrono::nanoseconds(expected)))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  auto cq = CompletionQueue(mock_cq);

  auto client = GrpcClient::CreateMock(
      mock, Options{}
                .set<TransferStallTimeoutOption>(expected)
                .set<GrpcCompletionQueueOption>(cq));
  google::cloud::internal::OptionsSpan const span(
      Options{}.set<TransferStallTimeoutOption>(expected));
  auto metadata = client->InsertObjectMedia(
      InsertObjectMediaRequest("test-bucket", "test-object",
                               "The quick brown fox jumps over the lazy dog"));
  EXPECT_THAT(metadata,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Write()")));
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientInsertObjectMediaTest, StallTimeoutClose) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Close)
            .WillOnce(Return(google::storage::v2::WriteObjectResponse{}));
        return stream;
      });

  auto const expected = std::chrono::seconds(42);
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(std::chrono::nanoseconds(expected)))
      .WillOnce(Return(ByMove(
          make_ready_future(StatusOr<std::chrono::system_clock::time_point>(
              Status{StatusCode::kCancelled, "test-only"})))))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  auto cq = CompletionQueue(mock_cq);

  auto client = GrpcClient::CreateMock(
      mock, Options{}
                .set<TransferStallTimeoutOption>(expected)
                .set<GrpcCompletionQueueOption>(cq));
  google::cloud::internal::OptionsSpan const span(
      Options{}.set<TransferStallTimeoutOption>(expected));
  auto metadata = client->InsertObjectMedia(
      InsertObjectMediaRequest("test-bucket", "test-object",
                               "The quick brown fox jumps over the lazy dog"));
  EXPECT_THAT(metadata,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Finish()")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
