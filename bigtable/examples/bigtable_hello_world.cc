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
#include "bigtable/client/table.h"
#include "bigtable/client/table_admin.h"
//! [bigtable includes]

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project_id> <instance_id> <table_id>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];

  // Connect to the Cloud Bigtable Admin API.
  //! [connect admin]
  bigtable::TableAdmin table_admin(
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions()),
      instance_id);
  //! [connect admin]

  //! [create table]
  // Define the desired schema for the Table.
  auto gc_rule = bigtable::GcRule::MaxNumVersions(1);
  bigtable::TableConfig schema({{"family", gc_rule}}, {});

  // Create a table.
  auto returned_schema = table_admin.CreateTable(table_id, schema);
  //! [create table]

  // Create an object to access the Cloud Bigtable Data API.
  //! [connect data]
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            project_id, instance_id, bigtable::ClientOptions()),
                        table_id);
  //! [connect data]

  // Modify (and create if necessary) a row.
  //! [write rows]
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
    table.Apply(bigtable::SingleRowMutation(
        std::move(row_key), bigtable::SetCell("family", "c0", greeting)));
    ++i;
  }
  //! [write rows]

  // Read a single row.
  //! [read row]
  auto result = table.ReadRow(
      "key-0", bigtable::Filter::ColumnRangeClosed("family", "c0", "c0"));
  if (not result.first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table.table_name()
              << std::endl;
    return 0;
  }
  auto const& cell = result.second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp() << "us\n"
            << '"' << cell.value() << '"' << std::endl;
  //! [read row]

  // Read a single row.
  //! [scan all]
  for (auto& row : table.ReadRows(bigtable::RowRange::InfiniteRange(),
                                  bigtable::Filter::PassAllFilter())) {
    std::cout << row.row_key() << ":\n";
    for (auto& cell : row.cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp() << "us\n"
                << "\t\"" << cell.value() << '"' << std::endl;
    }
  }
  //! [scan all]

  // Delete the table
  //! [delete table]
  table_admin.DeleteTable(table_id);
  //! [delete table]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
