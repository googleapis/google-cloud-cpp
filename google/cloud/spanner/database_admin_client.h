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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/internal/database_admin_stub.h"
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
 *       std::cout << "Database created!\n";
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
      ClientOptions const& client_options = ClientOptions());

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
   * and ending characters must be lower case letters, numbers, underscore ('_`)
   * or dashes (`-`), that is, they must belong to the `[a-z0-9_-]`.
   *
   * @return A `google::cloud::future` that becomes satisfied when the operation
   *   completes on the service. Note that this can take minutes in some cases.
   *
   * @par Example
   * @snippet database_admin_samples.cc create-database
   *
   * @see https://cloud.google.com/spanner/docs/data-definition-language for a
   *     full list of the DDL operations
   *
   * @see
   * https://cloud.google.com/spanner/docs/data-definition-language#create_database
   *     for the regular expression that must be satisfied by the database id.
   */
  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(std::string const& project_id, std::string const& instance_id,
                 std::string const& database_id,
                 std::vector<std::string> const& extra_statements = {});

  /**
   * Drops (deletes) an existing Cloud Spanner database.
   *
   * @warning Dropping a database deletes all the tables and other data in the
   *   database. This is an unrecoverable operation.
   *
   * @par Example
   * @snippet database_admin_samples.cc drop-database
   */
  Status DropDatabase(std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& database_id);

  /// Create a new client with the given stub. For testing only.
  explicit DatabaseAdminClient(std::shared_ptr<internal::DatabaseAdminStub> s)
      : stub_(std::move(s)) {}

 private:
  std::shared_ptr<internal::DatabaseAdminStub> stub_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_
