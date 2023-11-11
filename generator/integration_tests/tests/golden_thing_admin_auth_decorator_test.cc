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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_auth_decorator.h"
#include "generator/integration_tests/test.pb.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_stub.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MakeTypicalAsyncMockAuth;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::Unused;

future<StatusOr<google::longrunning::Operation>> LongrunningError() {
  return make_ready_future(StatusOr<google::longrunning::Operation>(
      Status(StatusCode::kPermissionDenied, "uh-oh")));
}

// The general pattern of these test is to make two requests, both of which
// return an error. The first one because the auth strategy fails, the second
// because the operation in the mock stub fails.

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabases) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::ListDatabasesRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListDatabases(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListDatabases(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCreateDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCreateDatabase).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncCreateDatabase(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncCreateDatabase(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::GetDatabaseRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncUpdateDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncUpdateDatabaseDdl(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncUpdateDatabaseDdl(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, DropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::DropDatabaseRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DropDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DropDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetDatabaseDdl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetDatabaseDdl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, SetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::iam::v1::SetIamPolicyRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.SetIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.SetIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::iam::v1::GetIamPolicyRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetIamPolicy(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, TestIamPermissions) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::iam::v1::TestIamPermissionsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.TestIamPermissions(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.TestIamPermissions(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCreateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCreateBackup).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::test::admin::database::v1::CreateBackupRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncCreateBackup(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncCreateBackup(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::GetBackupRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, UpdateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::UpdateBackupRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.UpdateBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, DeleteBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::DeleteBackupRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListBackups) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListBackups)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::ListBackupsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListBackups(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListBackups(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncRestoreDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncRestoreDatabase).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncRestoreDatabase(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncRestoreDatabase(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListDatabaseOperations(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListDatabaseOperations(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListBackupOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeTypicalMockAuth(), mock);
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListBackupOperations(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListBackupOperations(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncGetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetDatabase).WillOnce([] {
    return make_ready_future(
        StatusOr<google::test::admin::database::v1::Database>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
  });

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::test::admin::database::v1::GetDatabaseRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncGetDatabase(
      cq, std::make_shared<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncGetDatabase(
      cq, std::make_shared<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncDropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncDropDatabase).WillOnce([] {
    return make_ready_future(Status(StatusCode::kPermissionDenied, "uh-oh"));
  });

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::test::admin::database::v1::DropDatabaseRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncDropDatabase(
      cq, std::make_shared<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncDropDatabase(
      cq, std::make_shared<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncGetOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::longrunning::GetOperationRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncGetOperation(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncGetOperation(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCancelOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));

  auto under_test = GoldenThingAdminAuth(MakeTypicalAsyncMockAuth(), mock);
  google::longrunning::CancelOperationRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncCancelOperation(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncCancelOperation(
      cq, std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
