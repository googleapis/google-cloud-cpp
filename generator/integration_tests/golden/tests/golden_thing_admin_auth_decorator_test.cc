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
using ::testing::IsNull;

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

// The general pattern of these test is to make two requests, both of which
// return an error. The first one because the auth strategy fails, the second
// because the operation in the mock stub fails.

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabases) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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

TEST(GoldenThingAdminAuthDecoratorTest, CreateDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, CreateDatabase)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.CreateDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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

TEST(GoldenThingAdminAuthDecoratorTest, UpdateDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, UpdateDatabaseDdl)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.UpdateDatabaseDdl(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.UpdateDatabaseDdl(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, DropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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

TEST(GoldenThingAdminAuthDecoratorTest, CreateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, CreateBackup)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::test::admin::database::v1::CreateBackupRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.CreateBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateBackup(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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

TEST(GoldenThingAdminAuthDecoratorTest, RestoreDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, RestoreDatabase)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.RestoreDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.RestoreDatabase(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

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

TEST(GoldenThingAdminAuthDecoratorTest, GetOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetOperation)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::longrunning::GetOperationRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GetOperation(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetOperation(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenThingAdminAuthDecoratorTest, CancelOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, CancelOperation)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenThingAdminAuth(MakeMockAuth(), mock);
  google::longrunning::CancelOperationRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.CancelOperation(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CancelOperation(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
