// Copyright 2019 Google LLC
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

namespace cbt = google::cloud::bigtable;

// This example shows how to create a table, add some rows to it,
// read certain rows whose keys start with a certain prefix
// and delete the table.

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

  std::string const family_name = "family";
  std::string prefix = "key-";

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
  google::cloud::bigtable::TableConfig schema({{family_name, gc_rule}}, {});

  // Create a new table.
  auto returned_schema = table_admin.CreateTable(table_id, schema);
  //! [create table] [END creating_a_table]

  // Create an object to access the Cloud Bigtable Data API.
  //! [connect data] [START connecting_to_bigtable]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data] [END connecting_to_bigtable]

  //! [write rows] [START writing_rows]
  std::vector<std::string> greetings{"Hello World!", "Hello Cloud Bigtable!",
                                     "Hello C++!"};

  int i = 0;
  for (auto greeting : greetings) {
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
    std::string row_key = prefix + std::to_string(i);
    table.Apply(google::cloud::bigtable::SingleRowMutation(
        std::move(row_key),
        google::cloud::bigtable::SetCell(family_name, "c0", greeting)));
    ++i;
  }
  //! [write rows] [END writing_rows]

  //! [create filter] [START creating_a_filter]
  auto filter = google::cloud::bigtable::Filter::Latest(1);
  //! [create filter] [END creating_a_filter]

  // Create a RowSet object
  //! [create row_set] [START creating row_set]
  auto row_set = cbt::RowSet();
  //! [create row_set] [END creating row_set]

  // Appending a Range::Prefix()
  //! [append range] [START appending range]
  auto range_rowset = cbt::RowRange::Prefix(prefix);
  row_set.Append(range_rowset);
  //! [append range] [END appending range]

  //! [scan rows] [START bigtable_read_range_set]
  std::cout << "\nReading keys with prefix: " << prefix << '\n';
  for (auto& row : table.ReadRows(std::move(row_set), filter)) {
    std::cout << row.row_key() << ":\n";
    for (auto& cell : row.cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp().count() << "us\n"
                << "\t\"" << cell.value() << '"' << '\n';
    }
  }
  //! [scan rows] [END bigtable_read_range_set]

  std::cout << std::flush;

  // Delete the table
  //! [delete table] [START deleting_a_table]
  table_admin.DeleteTable(table_id);
  //! [delete table] [END deleting_a_table]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
