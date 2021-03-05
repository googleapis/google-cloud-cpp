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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_DATABASE_ADMIN_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_DATABASE_ADMIN_STUB_H

#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

class MockDatabaseAdminStub
    : public google::cloud::spanner_internal::DatabaseAdminStub {
 public:
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateDatabase,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::CreateDatabaseRequest const&),
      (override));

  MOCK_METHOD(StatusOr<google::spanner::admin::database::v1::Database>,
              GetDatabase,
              (grpc::ClientContext&,
               google::spanner::admin::database::v1::GetDatabaseRequest const&),
              (override));

  MOCK_METHOD(
      StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>,
      GetDatabaseDdl,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::GetDatabaseDdlRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, UpdateDatabase,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&),
      (override));

  MOCK_METHOD(
      Status, DropDatabase,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::DropDatabaseRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>,
      ListDatabases,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::ListDatabasesRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, RestoreDatabase,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::RestoreDatabaseRequest const&),
      (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&,
               google::iam::v1::GetIamPolicyRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&,
               google::iam::v1::SetIamPolicyRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&,
               google::iam::v1::TestIamPermissionsRequest const&),
              (override));

  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateBackup,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::CreateBackupRequest const&),
      (override));

  MOCK_METHOD(StatusOr<google::spanner::admin::database::v1::Backup>, GetBackup,
              (grpc::ClientContext&,
               google::spanner::admin::database::v1::GetBackupRequest const&),
              (override));

  MOCK_METHOD(
      Status, DeleteBackup,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::DeleteBackupRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<google::spanner::admin::database::v1::ListBackupsResponse>,
      ListBackups,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::ListBackupsRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<google::spanner::admin::database::v1::Backup>, UpdateBackup,
      (grpc::ClientContext&,
       google::spanner::admin::database::v1::UpdateBackupRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<
          google::spanner::admin::database::v1::ListBackupOperationsResponse>,
      ListBackupOperations,
      (grpc::ClientContext&, google::spanner::admin::database::v1::
                                 ListBackupOperationsRequest const&),
      (override));

  MOCK_METHOD(
      StatusOr<
          google::spanner::admin::database::v1::ListDatabaseOperationsResponse>,
      ListDatabaseOperations,
      (grpc::ClientContext&, google::spanner::admin::database::v1::
                                 ListDatabaseOperationsRequest const&),
      (override));

  MOCK_METHOD(StatusOr<google::longrunning::Operation>, GetOperation,
              (grpc::ClientContext&,
               google::longrunning::GetOperationRequest const&),
              (override));

  MOCK_METHOD(Status, CancelOperation,
              (grpc::ClientContext&,
               google::longrunning::CancelOperationRequest const&),
              (override));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_DATABASE_ADMIN_STUB_H
