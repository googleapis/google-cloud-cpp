// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::TransferStallTimeoutOption;
using ::google::cloud::storage::internal::ConstBuffer;
using ::google::cloud::storage::internal::CreateNullHashFunction;
using ::google::cloud::storage::internal::UploadChunkRequest;
using ::google::cloud::storage::testing::MockInsertStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;

auto constexpr kExpectedWriteSize = static_cast<std::size_t>(
    google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
static_assert(
    kExpectedWriteSize % UploadChunkRequest::kChunkSizeQuantum == 0,
    "expected the write size to be a multiple of the upload chunk quantum");

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutWrite) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject).WillOnce([&] {
    ::testing::InSequence sequence;
    auto stream = std::make_unique<MockInsertStream>();
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Write).WillOnce(Return(false));
    EXPECT_CALL(*stream, Close)
        .WillOnce(
            Return(make_status_or(google::storage::v2::WriteObjectResponse{})));
    return stream;
  });

  auto const expected = std::chrono::seconds(42);
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(std::chrono::nanoseconds(expected)))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  auto cq = CompletionQueue(mock_cq);

  std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> unused;
  auto client =
      std::make_unique<GrpcStub>(mock, unused,
                                 Options{}
                                     .set<TransferStallTimeoutOption>(expected)
                                     .set<GrpcCompletionQueueOption>(cq));
  google::cloud::internal::OptionsSpan const span(
      Options{}.set<TransferStallTimeoutOption>(expected));
  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto context = rest_internal::RestContext{};
  auto response = client->UploadChunk(
      context, client->options(),
      UploadChunkRequest("test-only-upload-id", /*offset=*/0,
                         {ConstBuffer{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Write()")));
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutWritesDone) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject).WillOnce([&] {
    ::testing::InSequence sequence;
    auto stream = std::make_unique<MockInsertStream>();
    EXPECT_CALL(*stream, Write).WillOnce(Return(true));
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Write).WillOnce(Return(false));
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

  std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> unused;
  auto client =
      std::make_unique<GrpcStub>(mock, unused,
                                 Options{}
                                     .set<TransferStallTimeoutOption>(expected)
                                     .set<GrpcCompletionQueueOption>(cq));
  google::cloud::internal::OptionsSpan const span(
      Options{}.set<TransferStallTimeoutOption>(expected));
  auto const payload = std::string(
      kExpectedWriteSize + UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto context = rest_internal::RestContext{};
  auto response = client->UploadChunk(
      context, client->options(),
      UploadChunkRequest("test-only-upload-id", /*offset=*/0,
                         {ConstBuffer{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Write()")));
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutClose) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject).WillOnce([&] {
    ::testing::InSequence sequence;
    auto stream = std::make_unique<MockInsertStream>();
    EXPECT_CALL(*stream, Write).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Close)
        .WillOnce(
            Return(make_status_or(google::storage::v2::WriteObjectResponse{})));
    return stream;
  });

  auto const expected = std::chrono::seconds(42);
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(std::chrono::nanoseconds(expected)))
      .WillOnce(Return(ByMove(
          make_ready_future(StatusOr<std::chrono::system_clock::time_point>(
              Status{StatusCode::kCancelled, "test-only"})))))
      .WillOnce(Return(ByMove(
          make_ready_future(StatusOr<std::chrono::system_clock::time_point>(
              Status{StatusCode::kCancelled, "test-only"})))))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  auto cq = CompletionQueue(mock_cq);

  std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> unused;
  auto client =
      std::make_unique<GrpcStub>(mock, unused,
                                 Options{}
                                     .set<TransferStallTimeoutOption>(expected)
                                     .set<GrpcCompletionQueueOption>(cq));
  google::cloud::internal::OptionsSpan const span(
      Options{}.set<TransferStallTimeoutOption>(expected));
  auto const payload = std::string(
      kExpectedWriteSize + UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto context = rest_internal::RestContext{};
  auto response = client->UploadChunk(
      context, client->options(),
      UploadChunkRequest("test-only-upload-id", /*offset=*/0,
                         {ConstBuffer{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Close()")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
