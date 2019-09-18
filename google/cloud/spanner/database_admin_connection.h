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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CONNECTION_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CONNECTION_H_

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/internal/range_from_pagination.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"
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
using ListDatabaseRange = internal::PaginationRange<
    google::spanner::admin::database::v1::Database,
    google::spanner::admin::database::v1::ListDatabasesRequest,
    google::spanner::admin::database::v1::ListDatabasesResponse>;

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
   * Define the arguments for each member function.
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

namespace internal {
/// Create a connection with only the retry decorator.
std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<internal::DatabaseAdminStub> stub,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy);
}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CONNECTION_H_
