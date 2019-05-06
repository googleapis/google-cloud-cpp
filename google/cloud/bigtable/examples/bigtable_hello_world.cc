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

//! [bigtable includes] [START bigtable_hw_imports]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes] [END bigtable_hw_imports]

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project_id> <instance_id> <table_id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];

  // Create a namespace alias to make the code easier to read.
  namespace cbt = google::cloud::bigtable;

  // Connect to the Cloud Bigtable Admin API.
  //! [connect admin] [START bigtable_hw_connect]
  cbt::TableAdmin table_admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions()),
      instance_id);
  //! [connect admin] [END bigtable_hw_connect]

  //! [create table] [START bigtable_hw_create_table]
  // Define the desired schema for the Table.
  cbt::GcRule gc_rule = cbt::GcRule::MaxNumVersions(1);
  cbt::TableConfig schema({{"family", gc_rule}}, {});

  // Create a table.
  google::cloud::StatusOr<google::bigtable::admin::v2::Table> returned_schema =
      table_admin.CreateTable(table_id, schema);
  //! [create table]

  // Create an object to access the Cloud Bigtable Data API.
  //! [connect data]
  cbt::Table table(cbt::CreateDefaultDataClient(project_id, instance_id,
                                                cbt::ClientOptions()),
                   table_id);
  //! [connect data] [END bigtable_hw_create_table]

  // Modify (and create if necessary) a row.
  //! [write rows] [START bigtable_hw_write_rows]
  std::vector<std::string> greetings{"Hello World!", "Hello Cloud Bigtable!",
                                     "Hello C++!"};
  int i = 0;
  for (auto const& greeting : greetings) {
    // Each row has a unique row key.
    //
    // Note: This example uses sequential numeric IDs for simplicity, but
    // this can result in poor performance in a production application.
    // Since rows are stored in sorted order by key, sequential keys can
    // result in poor distribution of operations across nodes.
    //
    // For more information about how to design a Bigtable schema for the
    // best performance, see the documentation:
    //
    //     https://cloud.google.com/bigtable/docs/schema-design
    std::string row_key = "key-" + std::to_string(i);
    google::cloud::Status status = table.Apply(cbt::SingleRowMutation(
        std::move(row_key), cbt::SetCell("family", "c0", greeting)));

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    ++i;
  }
  //! [write rows] [END bigtable_hw_write_rows]

  //! [create filter] [START bigtable_hw_create_filter]
  cbt::Filter filter = cbt::Filter::ColumnRangeClosed("family", "c0", "c0");
  //! [create filter] [END bigtable_hw_create_filter]

  // Read a single row.
  //! [read row] [START bigtable_hw_get_with_filter]
  google::cloud::StatusOr<std::pair<bool, cbt::Row>> result =
      table.ReadRow("key-0", filter);
  if (!result) {
    throw std::runtime_error(result.status().message());
  }
  if (!result->first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table.table_name()
              << "\n";
    return 0;
  }
  cbt::Cell const& cell = result->second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << "\n";
  //! [read row] [END bigtable_hw_get_with_filter]

  // Read all rows.
  //! [scan all] [START bigtable_hw_scan_with_filter]
  for (google::cloud::StatusOr<cbt::Row> const& row : table.ReadRows(
           cbt::RowRange::InfiniteRange(), cbt::Filter::PassAllFilter())) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << row->row_key() << ":\n";
    for (cbt::Cell const& cell : row->cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp().count() << "us\n"
                << "\t\"" << cell.value() << '"' << "\n";
    }
  }
  //! [scan all] [END bigtable_hw_scan_with_filter]

  // Delete the table
  //! [delete table] [START bigtable_hw_delete_table]
  google::cloud::Status status = table_admin.DeleteTable(table_id);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  //! [delete table] [END bigtable_hw_delete_table]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
