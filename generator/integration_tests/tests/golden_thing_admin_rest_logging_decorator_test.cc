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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_rest_logging_decorator.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_rest_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::Unused;

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

TEST(LoggingDecoratorRestTest, GetDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  google::test::admin::database::v1::Database database;
  database.set_name("my_database");
  EXPECT_CALL(*mock, GetDatabase).WillOnce(Return(database));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_database")));
}

TEST(LoggingDecoratorRestTest, GetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, GetDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ListDatabases) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, ListDatabases).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListDatabases(
      context, google::test::admin::database::v1::ListDatabasesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabases")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, CreateDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, CreateDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.CreateDatabase(
      context, google::test::admin::database::v1::CreateDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, UpdateDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, UpdateDatabaseDdl).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.UpdateDatabaseDdl(
      context, google::test::admin::database::v1::UpdateDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, DropDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, DropDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DropDatabase(
      context, google::test::admin::database::v1::DropDatabaseRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, GetDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, GetDatabaseDdl).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetDatabaseDdl(
      context, google::test::admin::database::v1::GetDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, SetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, SetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, GetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, GetIamPolicy).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, TestIamPermissions) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, TestIamPermissions).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, CreateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, CreateBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.CreateBackup(
      context, google::test::admin::database::v1::CreateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, GetBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, GetBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GetBackup(
      context, google::test::admin::database::v1::GetBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, UpdateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, UpdateBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.UpdateBackup(
      context, google::test::admin::database::v1::UpdateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, DeleteBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, DeleteBackup).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DeleteBackup(
      context, google::test::admin::database::v1::DeleteBackupRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ListBackups) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, ListBackups).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListBackups(
      context, google::test::admin::database::v1::ListBackupsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackups")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, RestoreDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, RestoreDatabase).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.RestoreDatabase(
      context, google::test::admin::database::v1::RestoreDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, ListDatabaseOperations).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListDatabaseOperations(
      context,
      google::test::admin::database::v1::ListDatabaseOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabaseOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ListBackupOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  testing_util::ScopedLog log;
  EXPECT_CALL(*mock, ListBackupOperations).WillOnce(Return(TransientError()));

  GoldenThingAdminRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListBackupOperations(
      context,
      google::test::admin::database::v1::ListBackupOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackupOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
