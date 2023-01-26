// Copyright 2023 Google LLC
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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_tracing_stub.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_stub.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::Unused;

future<StatusOr<google::longrunning::Operation>> LongrunningError(Unused,
                                                                  Unused,
                                                                  Unused) {
  return make_ready_future(
      StatusOr<google::longrunning::Operation>(internal::AbortedError("fail")));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabases) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  auto result = under_test.ListDatabases(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCreateDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCreateDatabase).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  auto result = under_test.GetDatabase(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncUpdateDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, DropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  auto result = under_test.DropDatabase(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  auto result = under_test.GetDatabaseDdl(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, SetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  auto result = under_test.SetIamPolicy(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  auto result = under_test.GetIamPolicy(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, TestIamPermissions) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  auto result = under_test.TestIamPermissions(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCreateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCreateBackup).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::CreateBackupRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, GetBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  auto result = under_test.GetBackup(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, UpdateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::UpdateBackupRequest request;
  auto result = under_test.UpdateBackup(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, DeleteBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::DeleteBackupRequest request;
  auto result = under_test.DeleteBackup(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListBackups) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListBackups)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupsRequest request;
  auto result = under_test.ListBackups(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncRestoreDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncRestoreDatabase).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  auto result = under_test.ListDatabaseOperations(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, ListBackupOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  auto result = under_test.ListBackupOperations(context, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncGetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetDatabase)
      .WillOnce(Return(ByMove(make_ready_future(
          StatusOr<google::test::admin::database::v1::Database>(
              internal::AbortedError("fail"))))));

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::GetDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncGetDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncDropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncDropDatabase)
      .WillOnce(
          Return(ByMove(make_ready_future(internal::AbortedError("fail")))));

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::DropDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncDropDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncGetOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::longrunning::GetOperationRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncGetOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

TEST(GoldenThingAdminAuthDecoratorTest, AsyncCancelOperation) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce(
          Return(ByMove(make_ready_future(internal::AbortedError("fail")))));

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::longrunning::CancelOperationRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncCancelOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
