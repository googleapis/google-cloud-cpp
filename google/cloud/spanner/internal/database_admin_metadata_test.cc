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

#include "google/cloud/spanner/internal/database_admin_metadata.h"
#include "google/cloud/spanner/testing/mock_database_admin_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsContextMDValid;
namespace gcsa = ::google::spanner::admin::database::v1;

class DatabaseAdminMetadataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockDatabaseAdminStub>();
    DatabaseAdminMetadata stub(mock_);
    expected_api_client_header_ = google::cloud::internal::ApiClientHeader();
  }

  void TearDown() override {}

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockDatabaseAdminStub> mock_;
  std::string expected_api_client_header_;
};

TEST_F(DatabaseAdminMetadataTest, CreateDatabase) {
  EXPECT_CALL(*mock_, AsyncCreateDatabase)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gcsa::CreateDatabaseRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "CreateDatabase",
                             expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminMetadata stub(mock_);
  CompletionQueue cq;
  gcsa::CreateDatabaseRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(DatabaseAdminMetadataTest, UpdateDatabase) {
  EXPECT_CALL(*mock_, AsyncUpdateDatabaseDdl)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gcsa::UpdateDatabaseDdlRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "UpdateDatabaseDdl",
                             expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminMetadata stub(mock_);
  CompletionQueue cq;
  gcsa::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  auto response = stub.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(DatabaseAdminMetadataTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::DropDatabaseRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "DropDatabase",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  auto status = stub.DropDatabase(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(DatabaseAdminMetadataTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::ListDatabasesRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "ListDatabases",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::ListDatabasesRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.ListDatabases(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, RestoreDatabase) {
  EXPECT_CALL(*mock_, AsyncRestoreDatabase)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gcsa::RestoreDatabaseRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "RestoreDatabase",
                             expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminMetadata stub(mock_);
  CompletionQueue cq;
  gcsa::RestoreDatabaseRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(DatabaseAdminMetadataTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::GetIamPolicyRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "GetIamPolicy",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  auto response = stub.GetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::SetIamPolicyRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "SetIamPolicy",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  google::iam::v1::Policy policy;
  auto& binding = *policy.add_bindings();
  binding.set_role("roles/spanner.databaseReader");
  *binding.add_members() = "user:foobar@example.com";
  *request.mutable_policy() = std::move(policy);

  auto response = stub.SetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::TestIamPermissionsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "TestIamPermissions",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  auto response = stub.TestIamPermissions(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, CreateBackup) {
  EXPECT_CALL(*mock_, AsyncCreateBackup)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       gcsa::CreateBackupRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "CreateBackup",
                             expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminMetadata stub(mock_);
  CompletionQueue cq;
  gcsa::CreateBackupRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(DatabaseAdminMetadataTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup)
      .WillOnce(
          [this](grpc::ClientContext& context, gcsa::GetBackupRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.spanner.admin.database.v1.DatabaseAdmin."
                "GetBackup",
                expected_api_client_header_));
            return TransientError();
          });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::GetBackupRequest request;
  request.set_name(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "backups/test-backup-id");
  auto status = stub.GetBackup(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(DatabaseAdminMetadataTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::DeleteBackupRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "DeleteBackup",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::DeleteBackupRequest request;
  request.set_name(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "backups/test-backup");
  auto status = stub.DeleteBackup(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(DatabaseAdminMetadataTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::ListBackupsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "ListBackups",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::ListBackupsRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.ListBackups(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::UpdateBackupRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "UpdateBackup",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::UpdateBackupRequest request;
  request.mutable_backup()->set_name(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "backups/test-backup-id");
  auto status = stub.UpdateBackup(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(DatabaseAdminMetadataTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::ListBackupOperationsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "ListBackupOperations",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::ListBackupOperationsRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.ListBackupOperations(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations)
      .WillOnce([this](grpc::ClientContext& context,
                       gcsa::ListDatabaseOperationsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.spanner.admin.database.v1.DatabaseAdmin."
                             "ListDatabaseOperations",
                             expected_api_client_header_));
        return TransientError();
      });

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::ListDatabaseOperationsRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto response = stub.ListDatabaseOperations(context, request);
  EXPECT_EQ(TransientError(), response.status());
}

TEST_F(DatabaseAdminMetadataTest, GetOperation) {
  EXPECT_CALL(*mock_, AsyncGetOperation)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::longrunning::GetOperationRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.longrunning.Operations.GetOperation",
            expected_api_client_header_));
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      });

  DatabaseAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::longrunning::GetOperationRequest request;
  request.set_name("operations/fake-operation-name");
  auto response = stub.AsyncGetOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), response.get().status());
}

TEST_F(DatabaseAdminMetadataTest, CancelOperation) {
  EXPECT_CALL(*mock_, AsyncCancelOperation)
      .WillOnce([this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::longrunning::CancelOperationRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.longrunning.Operations.CancelOperation",
            expected_api_client_header_));
        return make_ready_future(TransientError());
      });

  DatabaseAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::longrunning::CancelOperationRequest request;
  request.set_name("operations/fake-operation-name");
  auto status = stub.AsyncCancelOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
