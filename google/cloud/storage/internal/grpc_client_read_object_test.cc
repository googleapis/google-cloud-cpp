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
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce([&](google::cloud::CompletionQueue const&,
                    std::unique_ptr<grpc::ClientContext>,
                    ReadObjectRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        auto stream = absl::make_unique<MockObjectMediaStream>();
        EXPECT_CALL(*stream, Start)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        return stream;
      });

  auto client = GrpcClient::CreateMock(mock);
  auto stream =
      client->ReadObject(ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(stream);
  ASSERT_THAT(stream->get(), NotNull());
  auto* reader = dynamic_cast<GrpcObjectReadSource*>(stream->get());
  ASSERT_THAT(reader, NotNull());
  EXPECT_GE(reader->download_stall_timeout(), std::chrono::minutes(2));
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
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce([&](google::cloud::CompletionQueue const&,
                    std::unique_ptr<grpc::ClientContext>,
                    ReadObjectRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        auto stream = absl::make_unique<MockObjectMediaStream>();
        EXPECT_CALL(*stream, Start)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        return stream;
      });

  auto client = GrpcClient::CreateMock(
      mock, Options{}.set<TransferStallTimeoutOption>(configured_timeout));
  auto stream =
      client->ReadObject(ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(stream);
  ASSERT_THAT(stream->get(), NotNull());
  auto* reader = dynamic_cast<GrpcObjectReadSource*>(stream->get());
  ASSERT_THAT(reader, NotNull());
  EXPECT_EQ(reader->download_stall_timeout(), configured_timeout);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
