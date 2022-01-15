// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_auth.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

std::unique_ptr<google::cloud::internal::StreamingReadRpc<
    google::storage::v2::ReadObjectResponse>>
MakeObjectMediaStream(std::unique_ptr<grpc::ClientContext>,
                      google::storage::v2::ReadObjectRequest const&) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::storage::v2::ReadObjectResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
    google::storage::v2::WriteObjectRequest,
    google::storage::v2::WriteObjectResponse>>
MakeInsertStream(std::unique_ptr<grpc::ClientContext>) {
  using ErrorStream = ::google::cloud::internal::StreamingWriteRpcError<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

// The general pattern of these test is to make two requests, both of which
// return an error. The first one because the auth strategy fails, the second
// because the operation in the mock stub fails.
//
// The first two tests are slightly different because they deal with streaming
// RPCs.

TEST(StorageAuthTest, GetObjectMedia) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ReadObject).WillOnce(MakeObjectMediaStream);

  auto under_test = StorageAuth(MakeTypicalMockAuth(), mock);
  google::storage::v2::ReadObjectRequest request;
  auto auth_failure =
      under_test.ReadObject(absl::make_unique<grpc::ClientContext>(), request);
  auto v = auth_failure->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(v));
  EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success =
      under_test.ReadObject(absl::make_unique<grpc::ClientContext>(), request);
  v = auth_success->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(v));
  EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, WriteObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject).WillOnce(MakeInsertStream);

  auto under_test = StorageAuth(MakeTypicalMockAuth(), mock);
  auto auth_failure =
      under_test.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_THAT(auth_failure->Close(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success =
      under_test.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_THAT(auth_success->Close(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, StartResumableWrite) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, StartResumableWrite)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeTypicalMockAuth(), mock);
  google::storage::v2::StartResumableWriteRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.StartResumableWrite(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.StartResumableWrite(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, QueryWriteStatus) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, QueryWriteStatus)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeTypicalMockAuth(), mock);
  google::storage::v2::QueryWriteStatusRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.QueryWriteStatus(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.QueryWriteStatus(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
