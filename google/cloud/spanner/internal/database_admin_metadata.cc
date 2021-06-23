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
#include "google/cloud/internal/api_client_header.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = ::google::spanner::admin::database::v1;

DatabaseAdminMetadata::DatabaseAdminMetadata(
    std::shared_ptr<DatabaseAdminStub> child)
    : child_(std::move(child)),
      api_client_header_(google::cloud::internal::ApiClientHeader()) {}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminMetadata::AsyncCreateDatabase(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::CreateDatabaseRequest const& request) {
  SetMetadata(*context, "parent=" + request.parent());
  return child_->AsyncCreateDatabase(cq, std::move(context), request);
}

StatusOr<gcsa::Database> DatabaseAdminMetadata::GetDatabase(
    grpc::ClientContext& context, gcsa::GetDatabaseRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->GetDatabase(context, request);
}

StatusOr<gcsa::GetDatabaseDdlResponse> DatabaseAdminMetadata::GetDatabaseDdl(
    grpc::ClientContext& context, gcsa::GetDatabaseDdlRequest const& request) {
  SetMetadata(context, "database=" + request.database());
  return child_->GetDatabaseDdl(context, request);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminMetadata::AsyncUpdateDatabaseDdl(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::UpdateDatabaseDdlRequest const& request) {
  SetMetadata(*context, "database=" + request.database());
  return child_->AsyncUpdateDatabaseDdl(cq, std::move(context), request);
}

Status DatabaseAdminMetadata::DropDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DropDatabaseRequest const& request) {
  SetMetadata(context, "database=" + request.database());
  return child_->DropDatabase(context, request);
}

StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>
DatabaseAdminMetadata::ListDatabases(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListDatabasesRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListDatabases(context, request);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminMetadata::AsyncRestoreDatabase(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::RestoreDatabaseRequest const& request) {
  SetMetadata(*context, "parent=" + request.parent());
  return child_->AsyncRestoreDatabase(cq, std::move(context), request);
}

StatusOr<google::iam::v1::Policy> DatabaseAdminMetadata::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  SetMetadata(context, "resource=" + request.resource());
  return child_->GetIamPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> DatabaseAdminMetadata::SetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::SetIamPolicyRequest const& request) {
  SetMetadata(context, "resource=" + request.resource());
  return child_->SetIamPolicy(context, request);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
DatabaseAdminMetadata::TestIamPermissions(
    grpc::ClientContext& context,
    google::iam::v1::TestIamPermissionsRequest const& request) {
  SetMetadata(context, "resource=" + request.resource());
  return child_->TestIamPermissions(context, request);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminMetadata::AsyncCreateBackup(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::CreateBackupRequest const& request) {
  SetMetadata(*context, "parent=" + request.parent());
  return child_->AsyncCreateBackup(cq, std::move(context), request);
}

StatusOr<gcsa::Backup> DatabaseAdminMetadata::GetBackup(
    grpc::ClientContext& context, gcsa::GetBackupRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->GetBackup(context, request);
}

Status DatabaseAdminMetadata::DeleteBackup(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DeleteBackupRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->DeleteBackup(context, request);
}

StatusOr<google::spanner::admin::database::v1::ListBackupsResponse>
DatabaseAdminMetadata::ListBackups(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListBackupsRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListBackups(context, request);
}

StatusOr<gcsa::Backup> DatabaseAdminMetadata::UpdateBackup(
    grpc::ClientContext& context, gcsa::UpdateBackupRequest const& request) {
  SetMetadata(context, "backup.name=" + request.backup().name());
  return child_->UpdateBackup(context, request);
}

StatusOr<google::spanner::admin::database::v1::ListBackupOperationsResponse>
DatabaseAdminMetadata::ListBackupOperations(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListBackupOperationsRequest const&
        request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListBackupOperations(context, request);
}

StatusOr<google::spanner::admin::database::v1::ListDatabaseOperationsResponse>
DatabaseAdminMetadata::ListDatabaseOperations(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListDatabaseOperationsRequest const&
        request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListDatabaseOperations(context, request);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminMetadata::AsyncGetOperation(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::longrunning::GetOperationRequest const& request) {
  SetMetadata(*context, "name=" + request.name());
  return child_->AsyncGetOperation(cq, std::move(context), request);
}

future<Status> DatabaseAdminMetadata::AsyncCancelOperation(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::longrunning::CancelOperationRequest const& request) {
  SetMetadata(*context, "name=" + request.name());
  return child_->AsyncCancelOperation(cq, std::move(context), request);
}

void DatabaseAdminMetadata::SetMetadata(grpc::ClientContext& context,
                                        std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", api_client_header_);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
