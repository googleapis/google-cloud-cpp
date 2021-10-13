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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_STUB_H

#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <google/spanner/admin/database/v1/spanner_database_admin.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * Defines the low-level interface for database administration RPCs.
 */
class DatabaseAdminStub {
 public:
  virtual ~DatabaseAdminStub() = 0;

  /// Start the long-running operation to create a new Cloud Spanner database.
  virtual future<StatusOr<google::longrunning::Operation>> AsyncCreateDatabase(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::CreateDatabaseRequest const&) = 0;

  /// Fetch the metadata for a particular database.
  virtual StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::GetDatabaseRequest const&) = 0;

  /// Fetch the schema for a particular database.
  virtual StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>
  GetDatabaseDdl(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::GetDatabaseDdlRequest const&) = 0;

  /// Start a database update, using a sequence of DDL statements.
  virtual future<StatusOr<google::longrunning::Operation>>
  AsyncUpdateDatabaseDdl(CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                         google::spanner::admin::database::v1::
                             UpdateDatabaseDdlRequest const&) = 0;

  /// Drop an existing Cloud Spanner database.
  virtual Status DropDatabase(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::DropDatabaseRequest const&
          request) = 0;

  /// Fetch a page of databases.
  virtual StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>
  ListDatabases(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::ListDatabasesRequest const&) = 0;

  /// Start the long-running operation to restore a database from a given
  /// backup.
  virtual future<StatusOr<google::longrunning::Operation>> AsyncRestoreDatabase(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::RestoreDatabaseRequest const&) = 0;

  /// Fetch the IAM policy for a particular database.
  virtual StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext&, google::iam::v1::GetIamPolicyRequest const&) = 0;

  /// Fetch the IAM policy for a particular database.
  virtual StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext&, google::iam::v1::SetIamPolicyRequest const&) = 0;

  /// Get the subset of the permissions the caller has on a particular database.
  virtual StatusOr<google::iam::v1::TestIamPermissionsResponse>
  TestIamPermissions(grpc::ClientContext&,
                     google::iam::v1::TestIamPermissionsRequest const&) = 0;

  /// Start the long-running operation to create a new Cloud Spanner backup.
  virtual future<StatusOr<google::longrunning::Operation>> AsyncCreateBackup(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::database::v1::CreateBackupRequest const&) = 0;

  /// Get metadata on a pending or completed Backup.
  virtual StatusOr<google::spanner::admin::database::v1::Backup> GetBackup(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::GetBackupRequest const&
          request) = 0;

  /// Deletes a pending or completed Backup.
  virtual Status DeleteBackup(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::DeleteBackupRequest const&
          request) = 0;

  /// Fetch a page of backups.
  virtual StatusOr<google::spanner::admin::database::v1::ListBackupsResponse>
  ListBackups(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::ListBackupsRequest const&) = 0;

  /// Update a pending or completed backup.
  virtual StatusOr<google::spanner::admin::database::v1::Backup> UpdateBackup(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::UpdateBackupRequest const&
          request) = 0;

  /// Fetch a page of Backup operations.
  virtual StatusOr<
      google::spanner::admin::database::v1::ListBackupOperationsResponse>
  ListBackupOperations(grpc::ClientContext&,
                       google::spanner::admin::database::v1::
                           ListBackupOperationsRequest const&) = 0;

  /// Fetch a page of database operations.
  virtual StatusOr<
      google::spanner::admin::database::v1::ListDatabaseOperationsResponse>
  ListDatabaseOperations(grpc::ClientContext&,
                         google::spanner::admin::database::v1::
                             ListDatabaseOperationsRequest const&) = 0;

  /// Poll a long-running operation.
  virtual future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::longrunning::GetOperationRequest const&) = 0;

  /// Cancel a long-running operation.
  virtual future<Status> AsyncCancelOperation(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::longrunning::CancelOperationRequest const&) = 0;
};

/**
 * Constructs a simple `DatabaseAdminStub`.
 *
 * This stub does not create a channel pool, or retry operations.
 */
std::shared_ptr<DatabaseAdminStub> CreateDefaultDatabaseAdminStub(
    Options const& opts);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_STUB_H
