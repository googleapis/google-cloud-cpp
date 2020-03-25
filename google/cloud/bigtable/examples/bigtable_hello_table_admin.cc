// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    auto program = cmd.substr(last_slash + 1);
    std::cerr << "\nUsage: " << program
              << " <project-id> <instance-id> <table-id>\n\n"
              << "Example: " << program << " my-project my-instance my-table\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];

  //! [aliases]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  //! [aliases]

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect admin]
  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions()),
      instance_id);
  //! [connect admin]

  //! [create table]
  std::cout << "Creating a table:\n";
  StatusOr<google::bigtable::admin::v2::Table> schema = admin.CreateTable(
      table_id,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)},
                        {"foo", cbt::GcRule::MaxAge(std::chrono::hours(72))}},
                       {}));
  std::cout << "DONE\n";
  //! [create table]

  //! [listing tables]
  std::cout << "Listing tables:\n";
  auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
  if (!tables) throw std::runtime_error(tables.status().message());
  for (auto const& table : *tables) {
    std::cout << "    " << table.name() << "\n";
  }
  std::cout << "DONE\n";
  //! [listing tables]

  //! [retrieve table metadata]
  std::cout << "Get table metadata:\n";
  StatusOr<google::bigtable::admin::v2::Table> table =
      admin.GetTable(table_id, cbt::TableAdmin::FULL);
  if (!table) throw std::runtime_error(table.status().message());
  std::cout << "Table name : " << table->name() << "\n";

  std::cout << "List table families and GC rules:\n";
  for (auto const& kv : table->column_families()) {
    std::string const& family_name = kv.first;
    google::bigtable::admin::v2::ColumnFamily const& metadata = kv.second;
    std::cout << "Column Family :" << family_name << "\t"
              << metadata.DebugString() << "\n";
  }
  std::cout << "DONE\n";
  //! [retrieve table metadata]

  //! [modify column family]
  std::cout << "Update a column family GC rule:\n";
  StatusOr<google::bigtable::admin::v2::Table> updated_schema =
      admin.ModifyColumnFamilies(
          table_id,
          {cbt::ColumnFamilyModification::Update(
              "fam", cbt::GcRule::Union(
                         cbt::GcRule::MaxNumVersions(5),
                         cbt::GcRule::MaxAge(std::chrono::hours(24 * 7))))});
  if (!updated_schema) {
    throw std::runtime_error(updated_schema.status().message());
  }
  std::cout << "Schema modified to: " << updated_schema->DebugString() << "\n";
  //! [modify column family]

  //! [drop all rows]
  std::cout << "Deleting all the rows in " << table_id << "\n";
  google::cloud::Status status = admin.DropAllRows(table_id);
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "DONE\n";
  //! [drop all rows]

  //! [delete table]
  std::cout << "Deleting table:\n";
  google::cloud::Status delete_status = admin.DeleteTable(table_id);
  if (!delete_status.ok()) throw std::runtime_error(delete_status.message());
  std::cout << "DONE\n";
  //! [delete table]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
