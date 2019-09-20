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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_

#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/database_admin_connection.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Performs database administration operations on Spanner.
 *
 * Applications use this class to perform administrative operations on spanner
 * [Databases][database-doc-link].
 *
 * @par Performance
 *
 * Creating a new `DatabaseAdminClient` is a relatively expensive operation, new
 * objects establish new connections to the service. In contrast, copying or
 * moving an existing `DatabaseAdminClient` object is a relatively cheap
 * operation. Copied clients share the underlying resources.
 *
 * @par Thread Safety
 *
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Error Handling
 *
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v0::StatusOr) for more details.
 *
 * @code
 * namespace cs = google::cloud::spanner;
 * using google::cloud::StatusOr;
 * cs::DatabaseAdminClient client = ...;
 * StatusOr<google::spanner::admin::database::v1::Database> db =
 *     client.CreateDatabase(...).get();
 *
 * if (!db) {
 *   std::cerr << "Error in CreateDatabase: " << db.status() << "\n";
 *   return;
 * }
 *
 * // Use `db` as a smart pointer here, e.g.:
 * std::cout << "The database fully qualified name is: " << db->name() << "\n";
 * @endcode
 *
 * @par Long running operations
 *
 * Some operations in this class can take minutes to complete. In this case the
 * class returns a `google::cloud::future<StatusOr<T>>`, the application can
 * then poll the `future` or associate a callback to be invoked when the
 * operation completes:
 *
 * @code
 * namespace cs = google::cloud::spanner;
 * cs::DatabaseAdminClient client = ...;
 * // Make example less verbose.
 * using google::cloud::future;
 * using google::cloud::StatusOr;
 * using std::chrono::chrono_literals; // C++14
 *
 * auto database = client.CreateDatabase(...);
 * if (database.wait_for(5m) == std::future_state::ready) {
 *   std::cout << "Database created in under 5 minutes, yay!\n";
 *   return;
 * }
 * // Too slow, setup a callback instead:
 * database.then([](auto f) {
 *   StatusOr<google::spanner::admin::database::v1::Database> db = f.get();
 *   if (!db) {
 *       std::cout << "Failed creating a database!\n";
 *       return;
 *   }
 *   std::cout << "Database created!\n";
 * });
 * @endcode
 *
 * [database-doc-link]:
 * https://cloud.google.com/spanner/docs/schema-and-data-model
 */
class DatabaseAdminClient {
 public:
  explicit DatabaseAdminClient(
      ConnectionOptions const& options = ConnectionOptions());

  /**
   * Creates a new Cloud Spanner database in the given project and instance.
   *
   * This function creates a database (using the "CREATE DATABASE" DDL
   * statement) in the given Google Cloud Project and Cloud Spanner instance.
   * The application can provide an optional list of additional DDL statements
   * to atomically create tables and indices as well as the new database.
   *
   * Note that the database id must be between 2 and 30 characters long, it must
   * start with a lowercase letter (`[a-z]`), it must end with a lowercase
   * letter or a number (`[a-z0-9]`) and any characters between the beginning
   * and ending characters must be lower case letters, numbers, underscore (`_`)
   * or dashes (`-`), that is, they must belong to the `[a-z0-9_-]` character
   * set.
   *
   * @return A `google::cloud::future` that becomes satisfied when the operation
   *   completes on the service. Note that this can take minutes in some cases.
   *
   * @par Example
   * @snippet samples.cc create-database
   *
   * @see https://cloud.google.com/spanner/docs/data-definition-language for a
   *     full list of the DDL operations
   *
   * @see
   * https://cloud.google.com/spanner/docs/data-definition-language#create_database
   *     for the regular expression that must be satisfied by the database id.
   */
  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(Database db, std::vector<std::string> extra_statements = {});

  /**
   * Retrieve metadata information about a database.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent. Transient
   * failures are automatically retried.
   *
   * @par Example
   * @snippet samples.cc get-database
   */
  StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      Database db);

  /**
   * Retrieve a database schema.
   *
   * @par Idempotency
   * This is a read-only operation and therefore always idempotent. Transient
   * failures are automatically retried.
   *
   * @par Example
   * @snippet samples.cc get-database-ddl
   */
  StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>
  GetDatabaseDdl(Database db);

  /**
   * Updates the database using a series of DDL statements.
   *
   * This function schedules a series of updates to the database using a
   * sequence of DDL statements.
   *
   * @return A `google::cloud::future` that becomes satisfied when all the
   *   statements complete. Note that Cloud Spanner may fail to execute some of
   *   the statements.
   *
   * @par Example
   * @snippet samples.cc update-database
   *
   * @see https://cloud.google.com/spanner/docs/data-definition-language for a
   *     full list of the DDL operations
   */
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
  UpdateDatabase(Database db, std::vector<std::string> statements);

  /**
   * Drops (deletes) an existing Cloud Spanner database.
   *
   * @warning Dropping a database deletes all the tables and other data in the
   *   database. This is an unrecoverable operation.
   *
   * @par Example
   * @snippet samples.cc drop-database
   */
  Status DropDatabase(Database db);

  /**
   * List all the databases in a give project and instance.
   *
   * @par Idempotency
   * This operation is read-only and therefore always idempotent.
   *
   * @par Example
   * @snippet samples.cc list-databases
   */
  ListDatabaseRange ListDatabases(Instance in);

  /**
   * Gets the IAM policy for a database.
   *
   * @par Idempotency
   * This operation is read-only and therefore always idempotent.
   *
   * @par Example
   * @snippet samples.cc database-get-iam-policy
   */
  StatusOr<google::iam::v1::Policy> GetIamPolicy(Database db);

  /**
   * Set the IAM policy for the given database.
   *
   * This function changes the IAM policy configured in the given database to
   * the value of @p policy.
   *
   * @par Idempotency
   * This function is only idempotent if the `etag` field in @p policy is set.
   * Therefore, the underlying RPCs are only retried if the field is set, and
   * the function returns the first RPC error in any other case.
   *
   * @par Example
   * @snippet samples.cc add-database-reader-on-database
   *
   * @see The [Cloud Spanner
   *     documentation](https://cloud.google.com/spanner/docs/iam) for a
   *     description of the roles and permissions supported by Cloud Spanner.
   * @see [IAM Overview](https://cloud.google.com/iam/docs/overview#permissions)
   *     for an introduction to Identity and Access Management in Google Cloud
   *     Platform.
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      Database db, google::iam::v1::Policy policy);

  /// Create a new client with the given stub. For testing only.
  explicit DatabaseAdminClient(std::shared_ptr<DatabaseAdminConnection> c)
      : conn_(std::move(c)) {}

 private:
  std::shared_ptr<DatabaseAdminConnection> conn_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_
