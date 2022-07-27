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
#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockObjectMediaStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::google::storage::v2::ReadObjectRequest;
using ::testing::ByMove;
using ::testing::NotNull;
using ::testing::Return;

/// @test Verify downloads have a default timeout.
TEST(GrpcClientReadObjectTest, WithDefaultTimeout) {
  auto constexpr kExpectedRequestText =
      R"pb(bucket: "projects/_/buckets/test-bucket" object: "test-object")pb";
  ReadObjectRequest expected_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExpectedRequestText, &expected_request));

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>,
                    ReadObjectRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        auto stream = absl::make_unique<MockObjectMediaStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status{}));
        EXPECT_CALL(*stream, GetRequestMetadata).Times(1);
        return stream;
      });
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);
  auto cq = CompletionQueue(mock_cq);

  auto client = GrpcClient::CreateMock(
      mock, Options{}
                .set<DownloadStallTimeoutOption>(std::chrono::seconds(0))
                .set<GrpcCompletionQueueOption>(cq));
  // Normally the span is created by `storage::Client`, but we bypass that code
  // in this test.
  google::cloud::internal::OptionsSpan const span(client->options());
  auto stream =
      client->ReadObject(ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(stream);
  ASSERT_THAT(stream->get(), NotNull());
  std::vector<char> unused(1024);
  auto response = (*stream)->Read(unused.data(), unused.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bytes_received, 0);
}

/// @test Verify options can configured a non-default timeout.
TEST(GrpcClientReadObjectTest, WithExplicitTimeout) {
  auto constexpr kExpectedRequestText =
      R"pb(bucket: "projects/_/buckets/test-bucket" object: "test-object")pb";
  ReadObjectRequest expected_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExpectedRequestText, &expected_request));

  auto const configured_timeout = std::chrono::seconds(3);

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext>,
                    ReadObjectRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        auto stream = absl::make_unique<MockObjectMediaStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status{}));
        EXPECT_CALL(*stream, Cancel).Times(1);
        return stream;
      });
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(configured_timeout)))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  auto cq = CompletionQueue(mock_cq);

  auto client = GrpcClient::CreateMock(
      mock, Options{}
                .set<DownloadStallTimeoutOption>(configured_timeout)
                .set<GrpcCompletionQueueOption>(cq));
  // Normally the span is created by `storage::Client`, but we bypass that code
  // in this test.
  google::cloud::internal::OptionsSpan const span(client->options());
  auto stream =
      client->ReadObject(ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(stream);
  ASSERT_THAT(stream->get(), NotNull());
  std::vector<char> unused(1024);
  auto response = (*stream)->Read(unused.data(), unused.size());
  EXPECT_THAT(response, StatusIs(StatusCode::kDeadlineExceeded));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
