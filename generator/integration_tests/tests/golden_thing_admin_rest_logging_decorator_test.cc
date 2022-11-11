// Copyright 2022 Google LLC
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

#include "generator/integration_tests/golden/internal/golden_thing_admin_rest_logging_decorator.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_rest_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::Unused;

class LoggingDecoratorRestTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<MockGoldenThingAdminRestStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<MockGoldenThingAdminRestStub> mock_;
  testing_util::ScopedLog log_;
};

TEST_F(LoggingDecoratorRestTest, GetDatabaseSuccess) {
  google::test::admin::database::v1::Database database;
  database.set_name("my_database");
  EXPECT_CALL(*mock_, GetDatabase).WillOnce(Return(database));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_database")));
}

TEST_F(LoggingDecoratorRestTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListDatabases(
      context, google::test::admin::database::v1::ListDatabasesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabases")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, CreateDatabase) {
  EXPECT_CALL(*mock_, CreateDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.CreateDatabase(
      context, google::test::admin::database::v1::CreateDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, UpdateDatabaseDdl) {
  EXPECT_CALL(*mock_, UpdateDatabaseDdl).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.UpdateDatabaseDdl(
      context, google::test::admin::database::v1::UpdateDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DropDatabase(
      context, google::test::admin::database::v1::DropDatabaseRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetDatabaseDdl(
      context, google::test::admin::database::v1::GetDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, CreateBackup) {
  EXPECT_CALL(*mock_, CreateBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.CreateBackup(
      context, google::test::admin::database::v1::CreateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetBackup(
      context, google::test::admin::database::v1::GetBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.UpdateBackup(
      context, google::test::admin::database::v1::UpdateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DeleteBackup(
      context, google::test::admin::database::v1::DeleteBackupRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListBackups(
      context, google::test::admin::database::v1::ListBackupsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackups")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, RestoreDatabase) {
  EXPECT_CALL(*mock_, RestoreDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.RestoreDatabase(
      context, google::test::admin::database::v1::RestoreDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations)
      .WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListDatabaseOperations(
      context,
      google::test::admin::database::v1::ListDatabaseOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabaseOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListBackupOperations(
      context,
      google::test::admin::database::v1::ListBackupOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackupOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
