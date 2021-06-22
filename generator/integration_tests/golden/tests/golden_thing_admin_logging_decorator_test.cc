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

#include "generator/integration_tests/golden/internal/golden_thing_admin_logging_decorator.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/golden/mocks/mock_golden_thing_admin_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::Unused;

class LoggingDecoratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<MockGoldenThingAdminStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  static future<StatusOr<google::longrunning::Operation>>
  LongrunningTransientError(Unused, Unused, Unused) {
    return make_ready_future(
        StatusOr<google::longrunning::Operation>(TransientError()));
  }

  std::shared_ptr<MockGoldenThingAdminStub> mock_;
  testing_util::ScopedLog log_;
};

TEST_F(LoggingDecoratorTest, GetDatabaseSuccess) {
  google::test::admin::database::v1::Database database;
  database.set_name("my_database");
  EXPECT_CALL(*mock_, GetDatabase).WillOnce(Return(database));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_database")));
}

TEST_F(LoggingDecoratorTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListDatabases(
      context, google::test::admin::database::v1::ListDatabasesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabases")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CreateDatabase) {
  EXPECT_CALL(*mock_, AsyncCreateDatabase).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(),
      google::test::admin::database::v1::CreateDatabaseRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, UpdateDatabaseDdl) {
  EXPECT_CALL(*mock_, AsyncUpdateDatabaseDdl)
      .WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(),
      google::test::admin::database::v1::UpdateDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.DropDatabase(
      context, google::test::admin::database::v1::DropDatabaseRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetDatabaseDdl(
      context, google::test::admin::database::v1::GetDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CreateBackup) {
  EXPECT_CALL(*mock_, AsyncCreateBackup).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(),
      google::test::admin::database::v1::CreateBackupRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetBackup(
      context, google::test::admin::database::v1::GetBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.UpdateBackup(
      context, google::test::admin::database::v1::UpdateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.DeleteBackup(
      context, google::test::admin::database::v1::DeleteBackupRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListBackups(
      context, google::test::admin::database::v1::ListBackupsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackups")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, RestoreDatabase) {
  EXPECT_CALL(*mock_, AsyncRestoreDatabase).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(),
      google::test::admin::database::v1::RestoreDatabaseRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations)
      .WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListDatabaseOperations(
      context,
      google::test::admin::database::v1::ListDatabaseOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabaseOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListBackupOperations(
      context,
      google::test::admin::database::v1::ListBackupOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackupOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetOperation) {
  EXPECT_CALL(*mock_, AsyncGetOperation).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status =
      stub.AsyncGetOperation(cq, absl::make_unique<grpc::ClientContext>(),
                             google::longrunning::GetOperationRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CancelOperation) {
  EXPECT_CALL(*mock_, AsyncCancelOperation)
      .WillOnce(Return(ByMove(make_ready_future(TransientError()))));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status =
      stub.AsyncCancelOperation(cq, absl::make_unique<grpc::ClientContext>(),
                                google::longrunning::CancelOperationRequest());
  EXPECT_EQ(TransientError(), status.get());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CancelOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
