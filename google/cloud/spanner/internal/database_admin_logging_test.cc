// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/database_admin_logging.h"
#include "google/cloud/spanner/testing/mock_database_admin_stub.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
namespace gcsa = ::google::spanner::admin::database::v1;

class DatabaseAdminLoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockDatabaseAdminStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockDatabaseAdminStub> mock_;
  testing_util::ScopedLog log_;
};

TEST_F(DatabaseAdminLoggingTest, CreateDatabase) {
  EXPECT_CALL(*mock_, AsyncCreateDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateDatabaseRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncCreateDatabase(cq, absl::make_unique<grpc::ClientContext>(),
                               gcsa::CreateDatabaseRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.GetDatabase(context, gcsa::GetDatabaseRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.GetDatabaseDdl(context, gcsa::GetDatabaseDdlRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetDatabaseDdl")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, UpdateDatabase) {
  EXPECT_CALL(*mock_, AsyncUpdateDatabaseDdl)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::UpdateDatabaseDdlRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncUpdateDatabaseDdl(cq, absl::make_unique<grpc::ClientContext>(),
                                  gcsa::UpdateDatabaseDdlRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto status = stub.DropDatabase(context, gcsa::DropDatabaseRequest{});
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DropDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.ListDatabases(context, gcsa::ListDatabasesRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabases")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, RestoreDatabase) {
  EXPECT_CALL(*mock_, AsyncRestoreDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::RestoreDatabaseRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncRestoreDatabase(cq, absl::make_unique<grpc::ClientContext>(),
                                gcsa::RestoreDatabaseRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("RestoreDatabase")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.GetIamPolicy(context, google::iam::v1::GetIamPolicyRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.SetIamPolicy(context, google::iam::v1::SetIamPolicyRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SetIamPolicy")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.TestIamPermissions(
      context, google::iam::v1::TestIamPermissionsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TestIamPermissions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, CreateBackup) {
  EXPECT_CALL(*mock_, AsyncCreateBackup)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateBackupRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncCreateBackup(cq, absl::make_unique<grpc::ClientContext>(),
                             gcsa::CreateBackupRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto status = stub.GetBackup(context, gcsa::GetBackupRequest{});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto status = stub.DeleteBackup(context, gcsa::DeleteBackupRequest{});
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.ListBackups(context, gcsa::ListBackupsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackups")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto status = stub.UpdateBackup(context, gcsa::UpdateBackupRequest{});
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("UpdateBackup")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response =
      stub.ListBackupOperations(context, gcsa::ListBackupOperationsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListBackupOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations)
      .WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  grpc::ClientContext context;
  auto response = stub.ListDatabaseOperations(
      context, gcsa::ListDatabaseOperationsRequest{});
  EXPECT_EQ(TransientError(), response.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListDatabaseOperations")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, GetOperation) {
  EXPECT_CALL(*mock_, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const&) {
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto response =
      stub.AsyncGetOperation(cq, absl::make_unique<grpc::ClientContext>(),
                             google::longrunning::GetOperationRequest{});
  EXPECT_EQ(TransientError(), response.get().status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(DatabaseAdminLoggingTest, CancelOperation) {
  EXPECT_CALL(*mock_, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::CancelOperationRequest const&) {
        return make_ready_future(TransientError());
      });

  DatabaseAdminLogging stub(mock_, TracingOptions{});

  CompletionQueue cq;
  auto status =
      stub.AsyncCancelOperation(cq, absl::make_unique<grpc::ClientContext>(),
                                google::longrunning::CancelOperationRequest{});
  EXPECT_EQ(TransientError(), status.get());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CancelOperation")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
