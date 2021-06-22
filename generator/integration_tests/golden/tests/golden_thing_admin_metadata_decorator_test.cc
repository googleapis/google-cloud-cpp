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

#include "generator/integration_tests/golden/internal/golden_thing_admin_metadata_decorator.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "generator/integration_tests/golden/mocks/mock_golden_thing_admin_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::golden_internal::MockGoldenThingAdminStub;
using ::google::cloud::testing_util::IsContextMDValid;

class MetadataDecoratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    expected_api_client_header_ = google::cloud::internal::ApiClientHeader();
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
  std::string expected_api_client_header_;
};

TEST_F(MetadataDecoratorTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(context,
                                              "google.test.admin.database.v1."
                                              "GoldenThingAdmin.GetDatabase",
                                              expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetDatabase(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases)
      .WillOnce(
          [this](
              grpc::ClientContext& context,
              google::test::admin::database::v1::ListDatabasesRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(context,
                                              "google.test.admin.database.v1."
                                              "GoldenThingAdmin.ListDatabases",
                                              expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListDatabases(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, CreateDatabase) {
  EXPECT_CALL(*mock_, AsyncCreateDatabase)
      .WillOnce(
          [this](
              google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext> context,
              google::test::admin::database::v1::CreateDatabaseRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(*context,
                                              "google.test.admin.database.v1."
                                              "GoldenThingAdmin.CreateDatabase",
                                              expected_api_client_header_));
            return LongrunningTransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::test::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST_F(MetadataDecoratorTest, UpdateDatabaseDdl) {
  EXPECT_CALL(*mock_, AsyncUpdateDatabaseDdl)
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::test::admin::database::v1::
                           UpdateDatabaseDdlRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(*context,
                                          "google.test.admin.database.v1."
                                          "GoldenThingAdmin.UpdateDatabaseDdl",
                                          expected_api_client_header_));
        return LongrunningTransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST_F(MetadataDecoratorTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase)
      .WillOnce(
          [this](
              grpc::ClientContext& context,
              google::test::admin::database::v1::DropDatabaseRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenThingAdmin.DropDatabase",
                expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.DropDatabase(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(MetadataDecoratorTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl)
      .WillOnce(
          [this](
              grpc::ClientContext& context,
              google::test::admin::database::v1::GetDatabaseDdlRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenThingAdmin.GetDatabaseDdl",
                expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetDatabaseDdl(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, SetIamPolicy) {
  EXPECT_CALL(*mock_, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::SetIamPolicyRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenThingAdmin.SetIamPolicy",
            expected_api_client_header_));
        return TransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.SetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, GetIamPolicy) {
  EXPECT_CALL(*mock_, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::GetIamPolicyRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenThingAdmin.GetIamPolicy",
            expected_api_client_header_));
        return TransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetIamPolicy(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, TestIamPermissions) {
  EXPECT_CALL(*mock_, TestIamPermissions)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::TestIamPermissionsRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(context,
                                          "google.test.admin.database.v1."
                                          "GoldenThingAdmin.TestIamPermissions",
                                          expected_api_client_header_));
        return TransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.TestIamPermissions(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, CreateBackup) {
  EXPECT_CALL(*mock_, AsyncCreateBackup)
      .WillOnce(
          [this](
              google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext> context,
              google::test::admin::database::v1::CreateBackupRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                *context,
                "google.test.admin.database.v1.GoldenThingAdmin.CreateBackup",
                expected_api_client_header_));
            return LongrunningTransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::test::admin::database::v1::CreateBackupRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST_F(MetadataDecoratorTest, GetBackup) {
  EXPECT_CALL(*mock_, GetBackup)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::test::admin::database::v1::GetBackupRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenThingAdmin.GetBackup",
                expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto status = stub.GetBackup(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, UpdateBackup) {
  EXPECT_CALL(*mock_, UpdateBackup)
      .WillOnce(
          [this](
              grpc::ClientContext& context,
              google::test::admin::database::v1::UpdateBackupRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenThingAdmin.UpdateBackup",
                expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto status = stub.UpdateBackup(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, DeleteBackup) {
  EXPECT_CALL(*mock_, DeleteBackup)
      .WillOnce(
          [this](
              grpc::ClientContext& context,
              google::test::admin::database::v1::DeleteBackupRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenThingAdmin.DeleteBackup",
                expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::DeleteBackupRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto status = stub.DeleteBackup(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(MetadataDecoratorTest, ListBackups) {
  EXPECT_CALL(*mock_, ListBackups)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::test::admin::database::v1::ListBackupsRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenThingAdmin.ListBackups",
                expected_api_client_header_));
            return TransientError();
          });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupsRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListBackups(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, RestoreDatabase) {
  EXPECT_CALL(*mock_, AsyncRestoreDatabase)
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::test::admin::database::v1::
                           RestoreDatabaseRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenThingAdmin.RestoreDatabase",
            expected_api_client_header_));
        return LongrunningTransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST_F(MetadataDecoratorTest, ListDatabaseOperations) {
  EXPECT_CALL(*mock_, ListDatabaseOperations)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ListDatabaseOperationsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.test.admin.database.v1.GoldenThingAdmin."
                             "ListDatabaseOperations",
                             expected_api_client_header_));
        return TransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListDatabaseOperations(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListBackupOperations) {
  EXPECT_CALL(*mock_, ListBackupOperations)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ListBackupOperationsRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.test.admin.database.v1."
                             "GoldenThingAdmin.ListBackupOperations",
                             expected_api_client_header_));
        return TransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListBackupOperations(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, GetOperation) {
  EXPECT_CALL(*mock_, AsyncGetOperation)
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::longrunning::GetOperationRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.longrunning.Operations.GetOperation",
            expected_api_client_header_));
        return LongrunningTransientError();
      });

  GoldenThingAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::longrunning::GetOperationRequest request;
  request.set_name("operations/my_operation");
  auto status = stub.AsyncGetOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST_F(MetadataDecoratorTest, CancelOperation) {
  EXPECT_CALL(*mock_, AsyncCancelOperation)
      .WillOnce([this](google::cloud::CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::longrunning::CancelOperationRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.longrunning.Operations.CancelOperation",
            expected_api_client_header_));
        return make_ready_future(TransientError());
      });

  GoldenThingAdminMetadata stub(mock_);
  CompletionQueue cq;
  google::longrunning::CancelOperationRequest request;
  request.set_name("operations/my_operation");
  auto status = stub.AsyncCancelOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_EQ(TransientError(), status.get());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
