// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_CLUSTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_CLUSTER_H

#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/bigtable/admin/v2/table.pb.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

/**
 * An emulated cluster, which manages the lifecycle of all tables.
 *
 * This emulated cluster holds tables from all projects and instances - they are
 * merely components of table names.
 */
class Cluster {
 public:
  /**
   * Create a new table according to schema.
   *
   * @param table_name table's name in the form of
   *     `/projects/{}/instances/{}/tables/{}`.
   * @param schema the schema of the newly create table.
   * @return the schema of the newly created table.
   */
  StatusOr<google::bigtable::admin::v2::Table> CreateTable(
      std::string const& table_name, google::bigtable::admin::v2::Table schema);

  /**
   * List tables in the clustera.
   *
   * @param instance_name instances` name in the form of
   *     `/projects/{}/instances/{}`.
   * @param view a view to limit the amount of information returned about
   *     tables.
   * @return a vector of tables' schemas present in the instance trimmed
   *     according to `view`.
   */
  StatusOr<std::vector<google::bigtable::admin::v2::Table>> ListTables(
      std::string const& instance_name,
      google::bigtable::admin::v2::Table_View view) const;

  /**
   * Get details about a given table.
   *
   * @param table_name table's name in the form of
   *     `/projects/{}/instances/{}/tables/{}`.
   * @param view a view to limit the amount of information returned about
   *     table.
   * @return table's schema trimmed according to `view`.
   */
  StatusOr<google::bigtable::admin::v2::Table> GetTable(
      std::string const& table_name,
      google::bigtable::admin::v2::Table_View view) const;

  /**
   * Delete a table by its name.
   *
   * @param table_name table's name in the form of
   *     `/projects/{}/instances/{}/tables/{}`.
   * @return whether deletion succeeded. Apart from failing to to remove a
   *     non-existent table it might also fail if the table has deletion
   *     protection set.
   */
  Status DeleteTable(std::string const& table_name);

  /**
   * Check if a table exists.
   * @param table_name table's name in the form of
   *     `/projects/{}/instances/{}/tables/{}`.
   * @return true if table exists.
   */
  bool HasTable(std::string const& table_name) const;

  /**
   * Find a table by name.
   *
   * @param table_name table's name in the form of
   *     `/projects/{}/instances/{}/tables/{}`.
   * @return a pointer to the table or error if it doesn't exist.
   */
  StatusOr<std::shared_ptr<Table>> FindTable(std::string const& table_name);

 private:
  mutable std::mutex mu_;

  /**
   * All the tables indexed by their names.
   *
   * The names are in the form `/projects/{}/instances/{}/tables/{}`. We're
   * holding the tables by `shared_ptr`s in order to be able to allow for more
   * concurrency - every access to a table should start with creating a copy of
   * the shared pointer.
   */
  std::map<std::string, std::shared_ptr<Table>> table_by_name_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_CLUSTER_H
