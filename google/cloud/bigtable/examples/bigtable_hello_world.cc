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

//! [bigtable includes] [START dependencies]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes] [END dependencies]

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

  // Connect to the Cloud Bigtable Admin API.
  //! [connect admin] [START connecting_to_bigtable]
  google::cloud::bigtable::TableAdmin table_admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);
  //! [connect admin] [END connecting_to_bigtable]

  //! [create table] [START creating_a_table]
  // Define the desired schema for the Table.
  auto gc_rule = google::cloud::bigtable::GcRule::MaxNumVersions(1);
  google::cloud::bigtable::TableConfig schema({{"family", gc_rule}}, {});

  // Create a table.
  auto returned_schema = table_admin.CreateTable(table_id, schema);
  //! [create table] [END creating_a_table]

  // Create an object to access the Cloud Bigtable Data API.
  //! [connect data] [START connecting_to_bigtable]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data] [END connecting_to_bigtable]

  // Modify (and create if necessary) a row.
  //! [write rows] [START writing_rows]
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
    google::cloud::Status status =
        table.Apply(google::cloud::bigtable::SingleRowMutation(
            std::move(row_key),
            google::cloud::bigtable::SetCell("family", "c0", greeting)));

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    ++i;
  }
  //! [write rows] [END writing_rows]

  //! [create filter] [START creating_a_filter]
  auto filter =
      google::cloud::bigtable::Filter::ColumnRangeClosed("family", "c0", "c0");
  //! [create filter] [END creating_a_filter]

  // Read a single row.
  //! [read row] [START getting_a_row]
  auto result = table.ReadRow("key-0", filter);
  if (!result) {
    throw std::runtime_error(result.status().message());
  }
  if (!result->first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table.table_name()
              << "\n";
    return 0;
  }
  auto const& cell = result->second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << "\n";
  //! [read row] [END getting_a_row]

  // Read all rows.
  //! [scan all] [START scanning_all_rows]
  for (auto& row :
       table.ReadRows(google::cloud::bigtable::RowRange::InfiniteRange(),
                      google::cloud::bigtable::Filter::PassAllFilter())) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << row->row_key() << ":\n";
    for (auto& cell : row->cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp().count() << "us\n"
                << "\t\"" << cell.value() << '"' << "\n";
    }
  }
  //! [scan all] [END scanning_all_rows]

  // Delete the table
  //! [delete table] [START deleting_a_table]
  google::cloud::Status status = table_admin.DeleteTable(table_id);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  //! [delete table] [END deleting_a_table]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
