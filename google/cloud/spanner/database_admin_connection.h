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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CONNECTION_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/backup.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/pagination_range.h"
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * An input range to stream all the databases in a Cloud Spanner instance.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::spanner::admin::v1::Database` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListDatabaseRange = google::cloud::internal::PaginationRange<
    google::spanner::admin::database::v1::Database>;

/**
 * An input range to stream backup operations in Cloud Spanner instance.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::longrunning::Operation` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListBackupOperationsRange =
    google::cloud::internal::PaginationRange<google::longrunning::Operation>;

/**
 * An input range to stream database operations in Cloud Spanner instance.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::longrunning::Operation` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListDatabaseOperationsRange =
    google::cloud::internal::PaginationRange<google::longrunning::Operation>;

/**
 * An input range to stream backups in Cloud Spanner instance.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::longrunning::Operation` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListBackupsRange = google::cloud::internal::PaginationRange<
    google::spanner::admin::database::v1::Backup>;

/**
 * A connection to the Cloud Spanner instance administration service.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `DatabaseAdminClient`.  This allows users to inject custom
 * behavior (e.g., with a Google Mock object) in a `DatabaseAdminClient` object
 * for use in their own tests.
 *
 * To create a concrete instance that connects you to a real Cloud Spanner
 * instance administration service, see `MakeDatabaseAdminConnection()`.
 */
class DatabaseAdminConnection {
 public:
  virtual ~DatabaseAdminConnection() = 0;

  //@{
  /**
   * @name Define the arguments for each member function.
   *
   * Applications may define classes derived from `DatabaseAdminConnection`, for
   * example, because they want to mock the class. To avoid breaking all such
   * derived classes when we change the number or type of the arguments to the
   * member functions we define light weight structures to pass the arguments.
   */
  /// Wrap the arguments for `CreateDatabase()`.
  struct CreateDatabaseParams {
    /// The name of the database.
    Database database;
    /// Any additional statements to execute after creating the database.
    std::vector<std::string> extra_statements;
  };

  /// Wrap the arguments for `GetDatabase()`.
  struct GetDatabaseParams {
    /// The name of the database.
    Database database;
  };

  /// Wrap the arguments for `GetDatabaseDdl()`.
  struct GetDatabaseDdlParams {
    /// The name of the database.
    Database database;
  };

  /// Wrap the arguments for `CreateDatabase()`.
  struct UpdateDatabaseParams {
    /// The name of the database.
    Database database;
    /// The DDL statements updating the database schema.
    std::vector<std::string> statements;
  };

  /// Wrap the arguments for `DropDatabase()`.
  struct DropDatabaseParams {
    /// The name of the database.
    Database database;
  };

  /// Wrap the arguments for `ListDatabases()`.
  struct ListDatabasesParams {
    /// The name of the instance.
    Instance instance;
  };

  /// Wrap the arguments for `GetIamPolicy()`.
  struct GetIamPolicyParams {
    /// The name of the database.
    Database database;
  };

  /// Wrap the arguments for `SetIamPolicy()`.
  struct SetIamPolicyParams {
    /// The name of the database.
    Database database;
    google::iam::v1::Policy policy;
  };

  /// Wrap the arguments for `TestIamPermissions()`.
  struct TestIamPermissionsParams {
    /// The name of the database.
    Database database;
    std::vector<std::string> permissions;
  };

  /// Wrap the arguments for `CreateBackup()`.
  struct CreateBackupParams {
    /// The name of the database.
    Database database;
    std::string backup_id;
    std::chrono::system_clock::time_point expire_time;
  };

  /// Wrap the arguments for `GetBackup()`.
  struct GetBackupParams {
    /// The name of the backup.
    std::string backup_full_name;
  };

  /// Wrap the arguments for `DeleteBackup()`.
  struct DeleteBackupParams {
    /// The name of the backup.
    std::string backup_full_name;
  };

  /// Wrap the arguments for `ListBackups()`.
  struct ListBackupsParams {
    /// The name of the instance.
    Instance instance;
    std::string filter;
  };

  /// Wrap the arguments for `RestoreDatabase()`.
  struct RestoreDatabaseParams {
    /// The name of the database.
    Database database;
    /// The source backup for the restore.
    std::string backup_full_name;
  };

  /// Wrap the arguments for `UpdateBackup()`.
  struct UpdateBackupParams {
    google::spanner::admin::database::v1::UpdateBackupRequest request;
  };

  /// Wrap the arguments for `ListBackupOperations()`.
  struct ListBackupOperationsParams {
    /// The name of the instance.
    Instance instance;
    std::string filter;
  };

  /// Wrap the arguments for `ListDatabaseOperations()`.
  struct ListDatabaseOperationsParams {
    /// The name of the instance.
    Instance instance;
    std::string filter;
  };
  //@}

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.CreateDatabase
  /// RPC.
  virtual future<StatusOr<google::spanner::admin::database::v1::Database>>
      CreateDatabase(CreateDatabaseParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.GetDatabase
  /// RPC.
  virtual StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      GetDatabaseParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.GetDatabaseDdl
  /// RPC.
  virtual StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>
      GetDatabaseDdl(GetDatabaseDdlParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.UpdateDatabase
  /// RPC.
  virtual future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      UpdateDatabase(UpdateDatabaseParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.DropDatabase
  /// RPC.
  virtual Status DropDatabase(DropDatabaseParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.DropDatabase
  /// RPC.
  virtual ListDatabaseRange ListDatabases(ListDatabasesParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.RestoreDatabase
  /// RPC.
  virtual future<StatusOr<google::spanner::admin::database::v1::Database>>
      RestoreDatabase(RestoreDatabaseParams);

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.GetIamPolicy
  /// RPC.
  virtual StatusOr<google::iam::v1::Policy> GetIamPolicy(
      GetIamPolicyParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.SetIamPolicy
  /// RPC.
  virtual StatusOr<google::iam::v1::Policy> SetIamPolicy(
      SetIamPolicyParams) = 0;

  /// Define the interface for a
  /// google.spanner.v1.DatabaseAdmin.TestIamPermissions RPC.
  virtual StatusOr<google::iam::v1::TestIamPermissionsResponse>
      TestIamPermissions(TestIamPermissionsParams) = 0;

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.CreateBackup
  /// RPC.
  virtual future<StatusOr<google::spanner::admin::database::v1::Backup>>
      CreateBackup(CreateBackupParams);

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.GetBackup RPC.
  virtual StatusOr<google::spanner::admin::database::v1::Backup> GetBackup(
      GetBackupParams);

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.DeleteBackup
  /// RPC.
  virtual Status DeleteBackup(DeleteBackupParams);

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.ListBackups
  /// RPC.
  virtual ListBackupsRange ListBackups(ListBackupsParams);

  /// Define the interface for a google.spanner.v1.DatabaseAdmin.UpdateBackup
  /// RPC.
  virtual StatusOr<google::spanner::admin::database::v1::Backup> UpdateBackup(
      UpdateBackupParams);

  /// Define the interface for a
  /// google.spanner.v1.DatabaseAdmin.ListBackupOperations RPC.
  virtual ListBackupOperationsRange ListBackupOperations(
      ListBackupOperationsParams);

  /// Define the interface for a
  /// google.spanner.v1.DatabaseAdmin.ListDatabaseOperations RPC.
  virtual ListDatabaseOperationsRange ListDatabaseOperations(
      ListDatabaseOperationsParams);
};

/**
 * Returns an DatabaseAdminConnection object that can be used for interacting
 * with Cloud Spanner's admin APIs.
 *
 * The returned connection object should not be used directly, rather it should
 * be given to a `DatabaseAdminClient` instance.
 *
 * @see `DatabaseAdminConnection`
 *
 * @param options (optional) configure the `DatabaseAdminConnection` created by
 *     this function.
 */
std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options = ConnectionOptions());

/**
 * @copydoc MakeDatabaseAdminConnection
 *
 * @param retry_policy control for how long (or how many times) are retryable
 *     RPCs attempted
 * @param backoff_policy controls the backoff behavior between retry attempts,
 *     typically some form of exponential backoff with jitter
 * @param polling_policy controls for how often, and how quickly, are long
 *     running checked for completion
 *
 * @par Example
 * @snippet samples.cc custom-database-admin-policies
 */
std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy);

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
std::shared_ptr<spanner::DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<DatabaseAdminStub> stub,
    spanner::ConnectionOptions const& options);

std::shared_ptr<spanner::DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<DatabaseAdminStub> stub,
    spanner::ConnectionOptions const& options,
    std::unique_ptr<spanner::RetryPolicy> retry_policy,
    std::unique_ptr<spanner::BackoffPolicy> backoff_policy,
    std::unique_ptr<spanner::PollingPolicy> polling_policy);
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CONNECTION_H
