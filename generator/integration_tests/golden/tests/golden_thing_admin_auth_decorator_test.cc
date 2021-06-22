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

#include "generator/integration_tests/golden/internal/golden_thing_admin_auth_decorator.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/golden/mocks/mock_golden_thing_admin_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::internal::GrpcAuthenticationStrategy;
using ::google::cloud::testing_util::MockAuthenticationStrategy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::Unused;

std::shared_ptr<GrpcAuthenticationStrategy> MakeAsyncMockAuth() {
  using ReturnType = StatusOr<std::unique_ptr<grpc::ClientContext>>;
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        return make_ready_future(ReturnType(
            Status(StatusCode::kInvalidArgument, "cannot-set-credentials")));
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context) {
        context->set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return make_ready_future(make_status_or(std::move(context)));
      });
  return auth;
}

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

future<StatusOr<google::longrunning::Operation>> LongrunningError(Unused,
                                                                  Unused,
                                                                  Unused) {
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeAsyncMockAuth(), mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeAsyncMockAuth(), mock);
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, DropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeAsyncMockAuth(), mock);
  google::test::admin::database::v1::CreateBackupRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeAsyncMockAuth(), mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListBackupOperations(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListBackupOperations(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncGetOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminAuth(MakeAsyncMockAuth(), mock);
  google::longrunning::GetOperationRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncGetOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncGetOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCancelOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));

  auto under_test = GoldenThingAdminAuth(MakeAsyncMockAuth(), mock);
  google::longrunning::CancelOperationRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncCancelOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncCancelOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
