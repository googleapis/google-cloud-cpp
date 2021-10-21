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
  using ErrorStream = google::cloud::internal::StreamingReadRpcError<
      google::storage::v1::GetObjectMediaResponse>;
  return absl::make_unique<ErrorStream>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
}

std::unique_ptr<StorageStub::InsertStream> MakeInsertStream(
    std::unique_ptr<grpc::ClientContext>) {
  using ErrorStream = google::cloud::internal::StreamingWriteRpcError<
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

TEST(StorageAuthTest, DeleteBucketAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteBucketAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::DeleteBucketAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, GetBucketAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetBucketAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::GetBucketAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, InsertBucketAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, InsertBucketAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::InsertBucketAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.InsertBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.InsertBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, ListBucketAccessControls) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ListBucketAccessControls)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::ListBucketAccessControlsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListBucketAccessControls(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListBucketAccessControls(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, UpdateBucketAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateBucketAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::UpdateBucketAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.UpdateBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateBucketAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, DeleteBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteBucket)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::DeleteBucketRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, GetBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::GetBucketRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, InsertBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, InsertBucket)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::InsertBucketRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.InsertBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.InsertBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, ListBuckets) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::ListBucketsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListBuckets(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListBuckets(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, GetBucketIamPolicy) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetBucketIamPolicy)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::GetIamPolicyRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetBucketIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetBucketIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, SetBucketIamPolicy) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, SetBucketIamPolicy)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::SetIamPolicyRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.SetBucketIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.SetBucketIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, TestBucketIamPermissions) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, TestBucketIamPermissions)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::TestIamPermissionsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.TestBucketIamPermissions(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.TestBucketIamPermissions(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, UpdateBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::UpdateBucketRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.UpdateBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateBucket(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, DeleteDefaultObjectAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteDefaultObjectAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::DeleteDefaultObjectAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, GetDefaultObjectAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetDefaultObjectAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::GetDefaultObjectAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, InsertDefaultObjectAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, InsertDefaultObjectAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::InsertDefaultObjectAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.InsertDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.InsertDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, ListDefaultObjectAccessControls) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ListDefaultObjectAccessControls)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::ListDefaultObjectAccessControlsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListDefaultObjectAccessControls(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListDefaultObjectAccessControls(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, UpdateDefaultObjectAccessControl) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateDefaultObjectAccessControl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::UpdateDefaultObjectAccessControlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.UpdateDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateDefaultObjectAccessControl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, DeleteNotification) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteNotification)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::DeleteNotificationRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteNotification(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteNotification(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, GetNotification) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetNotification)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::GetNotificationRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetNotification(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetNotification(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, InsertNotification) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, InsertNotification)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::InsertNotificationRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.InsertNotification(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.InsertNotification(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, ListNotifications) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ListNotifications)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::ListNotificationsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListNotifications(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListNotifications(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(StorageAuthTest, DeleteObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = StorageAuth(MakeMockAuth(), mock);
  google::storage::v1::DeleteObjectRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteObject(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteObject(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
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
