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

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockInsertStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutStart) {
  // The mock will satisfy this promise when `Cancel()` is called.
  promise<void> hold_response;

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce([&](google::cloud::CompletionQueue const&,
                    std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return hold_response.get_future().then(
              [](future<void>) { return false; });
        });
        EXPECT_CALL(*stream, Cancel).WillOnce([&] {
          hold_response.set_value();
        });
        EXPECT_CALL(*stream, Finish)
            .WillOnce(Return(ByMove(make_ready_future(
                make_status_or(google::storage::v2::WriteObjectResponse{})))));
        return stream;
      });
  auto client = GrpcClient::CreateMock(
      mock, Options{}.set<TransferStallTimeoutOption>(std::chrono::seconds(1)));
  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", /*offset=*/0, {ConstBuffer{payload}}));
  EXPECT_THAT(response,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Start()")));
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutWrite) {
  // The mock will satisfy this promise when `Cancel()` is called.
  promise<void> hold_response;

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce([&](google::cloud::CompletionQueue const&,
                    std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Start)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, Write).WillOnce([&] {
          return hold_response.get_future().then(
              [](future<void>) { return false; });
        });
        EXPECT_CALL(*stream, Cancel).WillOnce([&] {
          hold_response.set_value();
        });
        EXPECT_CALL(*stream, Finish)
            .WillOnce(Return(ByMove(make_ready_future(
                make_status_or(google::storage::v2::WriteObjectResponse{})))));
        return stream;
      });
  auto client = GrpcClient::CreateMock(
      mock, Options{}.set<TransferStallTimeoutOption>(std::chrono::seconds(1)));
  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", /*offset=*/0, {ConstBuffer{payload}}));
  EXPECT_THAT(response,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Write()")));
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutWritesDone) {
  // The mock will satisfy this promise when `Cancel()` is called.
  promise<void> hold_response;

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce([&](google::cloud::CompletionQueue const&,
                    std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Start)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, WritesDone).WillOnce([&] {
          return hold_response.get_future().then(
              [](future<void>) { return false; });
        });
        EXPECT_CALL(*stream, Cancel).WillOnce([&] {
          hold_response.set_value();
        });
        EXPECT_CALL(*stream, Finish)
            .WillOnce(Return(ByMove(make_ready_future(
                make_status_or(google::storage::v2::WriteObjectResponse{})))));
        return stream;
      });
  auto client = GrpcClient::CreateMock(
      mock, Options{}.set<TransferStallTimeoutOption>(std::chrono::seconds(1)));
  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", /*offset=*/0, {ConstBuffer{payload}}));
  EXPECT_THAT(response, StatusIs(StatusCode::kDeadlineExceeded,
                                 HasSubstr("WritesDone()")));
}

/// @verify that stall timeouts are reported correctly.
TEST(GrpcClientUploadChunkTest, StallTimeoutFinish) {
  // The mock will satisfy this promise when `Cancel()` is called.
  promise<void> hold_response;

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce([&](google::cloud::CompletionQueue const&,
                    std::unique_ptr<grpc::ClientContext>) {
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Start)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, WritesDone)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return hold_response.get_future().then([](future<void>) {
            return make_status_or(google::storage::v2::WriteObjectResponse{});
          });
        });
        EXPECT_CALL(*stream, Cancel).WillOnce([&] {
          hold_response.set_value();
        });
        return stream;
      });
  auto client = GrpcClient::CreateMock(
      mock, Options{}.set<TransferStallTimeoutOption>(std::chrono::seconds(1)));
  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'A');
  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", /*offset=*/0, {ConstBuffer{payload}}));
  EXPECT_THAT(response,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("Finish()")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
