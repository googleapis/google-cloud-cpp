// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_H

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class identifies a Cloud Spanner Database.
 *
 * A Cloud Spanner database is identified by its `project_id`, `instance_id`,
 * and `database_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     database name. It is the application's responsibility to provide valid
 *     project, instance, and database ids. Passing invalid values will not be
 *     checked until the database name is used in a RPC to spanner.
 *
 * For more info about the `database_id` format, see
 * https://cloud.google.com/spanner/docs/reference/rpc/google.spanner.admin.database.v1#google.spanner.admin.database.v1.CreateDatabaseRequest
 */
class Database {
 public:
  /**
   * Constructs a Database object identified by the given @p instance and
   * @p database_id.
   */
  Database(Instance instance, std::string database_id);

  /**
   * Constructs a Database object identified by the given IDs.
   *
   * This is equivalent to first constructing an `Instance` from the given
   * @p project_id and @p instance_id arguments and then calling the
   * `Database(Instance, std::string)` constructor.
   */
  Database(std::string project_id, std::string instance_id,
           std::string database_id);

  /// @name Copy and move
  ///@{
  Database(Database const&) = default;
  Database& operator=(Database const&) = default;
  Database(Database&&) = default;
  Database& operator=(Database&&) = default;
  ///@}

  /// Returns the `Instance` containing this database.
  Instance const& instance() const { return instance_; }

  /// Returns the Database ID.
  std::string const& database_id() const { return database_id_; }

  /**
   * Returns the fully qualified database name as a string of the form:
   * "projects/<project-id>/instances/<instance-id>/databases/<database-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  ///@{
  friend bool operator==(Database const& a, Database const& b);
  friend bool operator!=(Database const& a, Database const& b);
  ///@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, Database const&);

 private:
  Instance instance_;
  std::string database_id_;
};

/**
 * Constructs a `Database` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<Database> MakeDatabase(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_H
