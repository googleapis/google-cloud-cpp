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
#include "generator/integration_tests/golden/internal/database_admin_logging_decorator.gcpcxx.pb.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class MockGoldenStub
    : public google::cloud::golden_internal::DatabaseAdminStub {
 public:
  ~MockGoldenStub() override = default;
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListDatabasesResponse>,
      ListDatabases,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListDatabasesRequest const&
           request),
      (override));

  MOCK_METHOD(StatusOr<::google::longrunning::Operation>, CreateDatabase,
              (grpc::ClientContext & context,
               ::google::test::admin::database::v1::CreateDatabaseRequest const&
                   request),
              (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::Database>, GetDatabase,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GetDatabaseRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::longrunning::Operation>, UpdateDatabaseDdl,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&
           request),
      (override));

  MOCK_METHOD(
      Status, DropDatabase,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::DropDatabaseRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GetDatabaseDdlResponse>,
      GetDatabaseDdl,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GetDatabaseDdlRequest const&
           request),
      (override));

  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext & context,
               ::google::iam::v1::SetIamPolicyRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext & context,
               ::google::iam::v1::GetIamPolicyRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<::google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext & context,
               ::google::iam::v1::TestIamPermissionsRequest const& request),
              (override));

  MOCK_METHOD(
      StatusOr<::google::longrunning::Operation>, CreateBackup,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::CreateBackupRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::Backup>, GetBackup,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GetBackupRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::Backup>, UpdateBackup,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::UpdateBackupRequest const& request),
      (override));

  MOCK_METHOD(
      Status, DeleteBackup,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::DeleteBackupRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListBackupsResponse>,
      ListBackups,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListBackupsRequest const& request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::longrunning::Operation>, RestoreDatabase,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>,
      ListDatabaseOperations,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request),
      (override));

  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>,
      ListBackupOperations,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&
           request),
      (override));

  /// Poll a long-running operation.
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, GetOperation,
              (grpc::ClientContext & client_context,
               google::longrunning::GetOperationRequest const& request),
              (override));

  /// Cancel a long-running operation.
  MOCK_METHOD(Status, CancelOperation,
              (grpc::ClientContext & client_context,
               google::longrunning::CancelOperationRequest const& request),
              (override));
};

class LoggingDecoratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
    mock_ = std::make_shared<MockGoldenStub>();
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::vector<std::string> ClearLogLines() { return backend_->ClearLogLines(); }

  std::shared_ptr<MockGoldenStub> mock_;

 private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0;  // NOLINT
};

TEST_F(LoggingDecoratorTest, GetDatabaseSuccess) {
  google::test::admin::database::v1::Database database;
  database.set_name("my_database");
  EXPECT_CALL(*mock_, GetDatabase(_, _)).WillOnce(Return(database));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("my_database")));
}

TEST_F(LoggingDecoratorTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GetDatabase(
      context, google::test::admin::database::v1::GetDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.ListDatabases(
      context, google::test::admin::database::v1::ListDatabasesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabases")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CreateDatabase) {
  EXPECT_CALL(*mock_, CreateDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.CreateDatabase(
      context, google::test::admin::database::v1::CreateDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, UpdateDatabaseDdl) {
  EXPECT_CALL(*mock_, UpdateDatabaseDdl(_, _))
      .WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.UpdateDatabaseDdl(
      context, google::test::admin::database::v1::UpdateDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.DropDatabase(
      context, google::test::admin::database::v1::DropDatabaseRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GetDatabaseDdl(
      context, google::test::admin::database::v1::GetDatabaseDdlRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions(_, _))
      .WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CreateBackup) {
  EXPECT_CALL(*mock_, CreateBackup(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.CreateBackup(
      context, google::test::admin::database::v1::CreateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GetBackup(
      context, google::test::admin::database::v1::GetBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.UpdateBackup(
      context, google::test::admin::database::v1::UpdateBackupRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.DeleteBackup(
      context, google::test::admin::database::v1::DeleteBackupRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.ListBackups(
      context, google::test::admin::database::v1::ListBackupsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackups")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, RestoreDatabase) {
  EXPECT_CALL(*mock_, RestoreDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.RestoreDatabase(
      context, google::test::admin::database::v1::RestoreDatabaseRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations(_, _))
      .WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.ListDatabaseOperations(
      context,
      google::test::admin::database::v1::ListDatabaseOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabaseOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations(_, _))
      .WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.ListBackupOperations(
      context,
      google::test::admin::database::v1::ListBackupOperationsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackupOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GetOperation) {
  EXPECT_CALL(*mock_, GetOperation(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.GetOperation(context, google::longrunning::GetOperationRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, CancelOperation) {
  EXPECT_CALL(*mock_, CancelOperation(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.CancelOperation(
      context, google::longrunning::CancelOperationRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CancelOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
