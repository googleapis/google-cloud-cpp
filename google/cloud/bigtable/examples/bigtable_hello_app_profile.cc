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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"

//! [cbt namespace]
namespace cbt = google::cloud::bigtable;
//! [cbt namespace]

int main(int argc, char* argv[]) try {
  // A little helper to extract the last component of a '/' separated path.
  auto basename = [](std::string const& path) -> std::string {
    auto last_slash = path.find_last_of('/');
    return path.substr(last_slash + 1);
  };

  if (argc != 5) {
    std::string const cmd = argv[0];
    std::cerr << "Usage: " << basename(cmd)
              << " <project_id> <instance_id> <table_id> <profile_id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];
  std::string const profile_id = argv[4];

  // Create an object to access the Cloud Bigtable Data API.
  auto data_client = cbt::CreateDefaultDataClient(project_id, instance_id,
                                                  cbt::ClientOptions());

  // Use the default profile to write some data.
  cbt::Table write(data_client, table_id);

  // Modify (and create if necessary) a row.
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
    google::cloud::Status status = write.Apply(cbt::SingleRowMutation(
        std::move(row_key), cbt::SetCell("fam", "c0", greeting)));
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    ++i;
  }

  std::cout << "Wrote some greetings to " << table_id << "\n";

  // Access Cloud Bigtable using a different profile
  //! [read with app profile]
  cbt::Table read(data_client, cbt::AppProfileId(profile_id), table_id);

  auto result =
      read.ReadRow("key-0", cbt::Filter::ColumnRangeClosed("fam", "c0", "c0"));
  if (!result) {
    throw std::runtime_error(result.status().message());
  }
  if (!result->first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table_id << "\n";
    return 1;
  }
  auto const& cell = result->second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << "\n";
  //! [read with app profile]

  // Read multiple rows.
  //! [scan all with app profile]
  std::cout << "Scanning all the data from " << table_id << "\n";
  for (auto& row : read.ReadRows(cbt::RowRange::InfiniteRange(),
                                 cbt::Filter::PassAllFilter())) {
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
  //! [scan all with app profile]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
