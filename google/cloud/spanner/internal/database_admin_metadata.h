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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_METADATA_H

#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/version.h"
#include <grpcpp/grpcpp.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * Implements the metadata Decorator for DatabaseAdminStub.
 */
class DatabaseAdminMetadata : public DatabaseAdminStub {
 public:
  explicit DatabaseAdminMetadata(std::shared_ptr<DatabaseAdminStub> child);

  ~DatabaseAdminMetadata() override = default;

  //@{
  /**
   * @name Override the functions from `DatabaseAdminStub`.
   */
  future<StatusOr<google::longrunning::Operation>> AsyncCreateDatabase(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::CreateDatabaseRequest const&)
      override;

  StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::GetDatabaseRequest const&) override;

  StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>
  GetDatabaseDdl(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::GetDatabaseDdlRequest const&)
      override;

  future<StatusOr<google::longrunning::Operation>> AsyncUpdateDatabaseDdl(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&)
      override;

  Status DropDatabase(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::DropDatabaseRequest const& request)
      override;

  StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>
  ListDatabases(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::ListDatabasesRequest const&)
      override;

  future<StatusOr<google::longrunning::Operation>> AsyncRestoreDatabase(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::RestoreDatabaseRequest const&)
      override;

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext& context,
      google::iam::v1::GetIamPolicyRequest const& request) override;

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext& context,
      google::iam::v1::SetIamPolicyRequest const& request) override;

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext& context,
      google::iam::v1::TestIamPermissionsRequest const& request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncCreateBackup(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::CreateBackupRequest const&)
      override;

  StatusOr<google::spanner::admin::database::v1::Backup> GetBackup(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::GetBackupRequest const& request)
      override;

  Status DeleteBackup(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::DeleteBackupRequest const& request)
      override;

  StatusOr<google::spanner::admin::database::v1::ListBackupsResponse>
  ListBackups(grpc::ClientContext&,
              google::spanner::admin::database::v1::ListBackupsRequest const&
                  request) override;

  StatusOr<google::spanner::admin::database::v1::Backup> UpdateBackup(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::UpdateBackupRequest const& request)
      override;

  StatusOr<google::spanner::admin::database::v1::ListBackupOperationsResponse>
  ListBackupOperations(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::ListBackupOperationsRequest const&
          request) override;

  StatusOr<google::spanner::admin::database::v1::ListDatabaseOperationsResponse>
  ListDatabaseOperations(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::ListDatabaseOperationsRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::longrunning::GetOperationRequest const&) override;

  future<Status> AsyncCancelOperation(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::longrunning::CancelOperationRequest const&) override;
  //@}

 private:
  void SetMetadata(grpc::ClientContext& context,
                   std::string const& request_params);

  std::shared_ptr<DatabaseAdminStub> child_;
  std::string api_client_header_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_METADATA_H
