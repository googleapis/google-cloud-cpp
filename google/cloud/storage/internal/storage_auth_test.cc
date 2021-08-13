// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_auth.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::internal::GrpcAuthenticationStrategy;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::MockAuthenticationStrategy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

std::shared_ptr<GrpcAuthenticationStrategy> MakeMockAuth() {
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, ConfigureContext)
      .WillOnce([](grpc::ClientContext&) {
        return Status(StatusCode::kInvalidArgument, "cannot-set-credentials");
      })
      .WillOnce([](grpc::ClientContext& context) {
        context.set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return Status{};
      });
  return auth;
}

std::unique_ptr<StorageStub::ObjectMediaStream> MakeObjectMediaStream(
    std::unique_ptr<grpc::ClientContext>,
    google::storage::v1::GetObjectMediaRequest const&) {
  using ErrorStream = ::google::cloud::internal::StreamingReadRpcError<
      google::storage::v1::GetObjectMediaResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<StorageStub::InsertStream> MakeInsertStream(
    std::unique_ptr<grpc::ClientContext>) {
  using ErrorStream = ::google::cloud::internal::StreamingWriteRpcError<
      google::storage::v1::InsertObjectRequest, google::storage::v1::Object>;
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
  EXPECT_CALL(*mock, GetObjectMedia).WillOnce(MakeObjectMediaStream);

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::GetObjectMediaRequest request;
  auto auth_failure = under_test.GetObjectMedia(
      absl::make_unique<grpc::ClientContext>(), request);
  auto v = auth_failure->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(v));
  EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetObjectMedia(
      absl::make_unique<grpc::ClientContext>(), request);
  v = auth_success->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(v));
  EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, InsertObjectMedia) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, InsertObjectMedia).WillOnce(MakeInsertStream);

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  auto auth_failure =
      under_test.InsertObjectMedia(absl::make_unique<grpc::ClientContext>());
  EXPECT_THAT(auth_failure->Close(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success =
      under_test.InsertObjectMedia(absl::make_unique<grpc::ClientContext>());
  EXPECT_THAT(auth_success->Close(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, StartResumableWrite) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, StartResumableWrite)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::StartResumableWriteRequest request;
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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::QueryWriteStatusRequest request;
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
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
