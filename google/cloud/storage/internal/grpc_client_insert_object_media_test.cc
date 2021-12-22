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
#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.h"
#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/grpc_options.h"
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

using ::google::cloud::storage::testing::MockInsertStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::_;
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
        auto stream = absl::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Write(IsProtoEqual(write_request), _))
            .WillOnce(Return(true));
        EXPECT_CALL(*stream, Close).WillOnce(Return(make_status_or(response)));
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
