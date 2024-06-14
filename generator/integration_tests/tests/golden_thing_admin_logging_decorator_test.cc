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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_logging_decorator.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_stub.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
  LongrunningTransientError() {
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
  auto status = stub.GetDatabase(context, Options{}, {});
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_database")));
}

TEST_F(LoggingDecoratorTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetDatabase(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListDatabases(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabases")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, AsyncCreateDatabase) {
  EXPECT_CALL(*mock_, AsyncCreateDatabase).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncCreateDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::test::admin::database::v1::CreateDatabaseRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CreateDatabase) {
  google::longrunning::Operation operation;
  operation.set_name("my_operation");
  EXPECT_CALL(*mock_, CreateDatabase).WillOnce(Return(operation));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.CreateDatabase(context, Options{}, {});
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_operation")));
}

TEST_F(LoggingDecoratorTest, AsyncUpdateDatabaseDdl) {
  EXPECT_CALL(*mock_, AsyncUpdateDatabaseDdl)
      .WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncUpdateDatabaseDdl(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::test::admin::database::v1::UpdateDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, UpdateDatabaseDdl) {
  google::longrunning::Operation operation;
  operation.set_name("my_operation");
  EXPECT_CALL(*mock_, UpdateDatabaseDdl).WillOnce(Return(operation));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.UpdateDatabaseDdl(context, Options{}, {});
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_operation")));
}

TEST_F(LoggingDecoratorTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.DropDatabase(context, Options{}, {});
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetDatabaseDdl(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.SetIamPolicy(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetIamPolicy(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.TestIamPermissions(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, AsyncCreateBackup) {
  EXPECT_CALL(*mock_, AsyncCreateBackup).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncCreateBackup(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::test::admin::database::v1::CreateBackupRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CreateBackup) {
  google::longrunning::Operation operation;
  operation.set_name("my_operation");
  EXPECT_CALL(*mock_, CreateBackup).WillOnce(Return(operation));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.CreateBackup(context, Options{}, {});
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_operation")));
}

TEST_F(LoggingDecoratorTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GetBackup(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.UpdateBackup(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.DeleteBackup(context, Options{}, {});
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListBackups(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackups")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, AsyncRestoreDatabase) {
  EXPECT_CALL(*mock_, AsyncRestoreDatabase).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncRestoreDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::test::admin::database::v1::RestoreDatabaseRequest());
  EXPECT_EQ(TransientError(), status.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, RestoreDatabase) {
  google::longrunning::Operation operation;
  operation.set_name("my_operation");
  EXPECT_CALL(*mock_, RestoreDatabase).WillOnce(Return(operation));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.RestoreDatabase(context, Options{}, {});
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_operation")));
}

TEST_F(LoggingDecoratorTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations)
      .WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListDatabaseOperations(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabaseOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations).WillOnce(Return(TransientError()));

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListBackupOperations(context, Options{}, {});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackupOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, AsyncGetDatabase) {
  EXPECT_CALL(*mock_, AsyncGetDatabase).WillOnce([] {
    return make_ready_future<
        StatusOr<google::test::admin::database::v1::Database>>(
        TransientError());
  });

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status = stub.AsyncGetDatabase(
                        cq, std::make_shared<grpc::ClientContext>(),
                        internal::MakeImmutableOptions({}),
                        google::test::admin::database::v1::GetDatabaseRequest())
                    .get();
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncGetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, AsyncDropDatabase) {
  EXPECT_CALL(*mock_, AsyncDropDatabase).WillOnce([] {
    return make_ready_future(TransientError());
  });

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status =
      stub.AsyncDropDatabase(
              cq, std::make_shared<grpc::ClientContext>(),
              internal::MakeImmutableOptions({}),
              google::test::admin::database::v1::DropDatabaseRequest())
          .get();
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncDropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetOperation) {
  EXPECT_CALL(*mock_, AsyncGetOperation).WillOnce(LongrunningTransientError);

  GoldenThingAdminLogging stub(mock_, TracingOptions{}, {});
  CompletionQueue cq;
  auto status =
      stub.AsyncGetOperation(cq, std::make_shared<grpc::ClientContext>(),
                             internal::MakeImmutableOptions({}),
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
      stub.AsyncCancelOperation(cq, std::make_shared<grpc::ClientContext>(),
                                internal::MakeImmutableOptions({}),
                                google::longrunning::CancelOperationRequest());
  EXPECT_EQ(TransientError(), status.get());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CancelOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
