// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/grpc_client.h"
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
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockObjectMediaStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::google::storage::v1::GetObjectMediaRequest;
using ::testing::NotNull;

/// @test Verify downloads have a default timeout.
TEST(GrpcClientReadObjectTest, WithDefaultTimeout) {
  auto constexpr kExpectedRequestText = R"pb(bucket: "test-bucket"
                                             object: "test-object")pb";
  google::storage::v1::GetObjectMediaRequest expected_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExpectedRequestText, &expected_request));

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetObjectMedia)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext> context,
                    GetObjectMediaRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        auto const timeout =
            context->deadline() - std::chrono::system_clock::now();
        // The default stall timeout is 2 minutes, but let's be generous to
        // avoid flakiness.
        EXPECT_LT(timeout, std::chrono::minutes(5))
            << ", timeout=" << absl::FromChrono(timeout);
        return absl::make_unique<MockObjectMediaStream>();
      });

  auto client = GrpcClient::CreateMock(mock);
  auto stream =
      client->ReadObject(ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(stream);
  ASSERT_THAT(stream->get(), NotNull());
}

/// @test Verify options can configured a non-default timeout.
TEST(GrpcClientReadObjectTest, WithExplicitTimeout) {
  auto constexpr kExpectedRequestText = R"pb(bucket: "test-bucket"
                                             object: "test-object")pb";
  google::storage::v1::GetObjectMediaRequest expected_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kExpectedRequestText, &expected_request));

  auto const configured_timeout = std::chrono::seconds(3);

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetObjectMedia)
      .WillOnce([&](std::unique_ptr<grpc::ClientContext> context,
                    GetObjectMediaRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        auto const timeout =
            context->deadline() - std::chrono::system_clock::now();
        EXPECT_LE(timeout, configured_timeout)
            << ", timeout=" << absl::FromChrono(timeout);
        return absl::make_unique<MockObjectMediaStream>();
      });

  auto client = GrpcClient::CreateMock(
      mock, Options{}.set<DownloadStallTimeoutOption>(configured_timeout));
  auto stream =
      client->ReadObject(ReadObjectRangeRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(stream);
  ASSERT_THAT(stream->get(), NotNull());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
