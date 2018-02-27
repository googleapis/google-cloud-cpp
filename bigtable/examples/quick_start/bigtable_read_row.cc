// Copyright 2018 Google Inc.
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

#include "bigtable/admin/table_admin.h"
#include "bigtable/client/table.h"

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

  // Create an object to access the Cloud Bigtable Data API.
  //! [connect data]
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            project_id, instance_id, bigtable::ClientOptions()),
                        table_id);
  //! [connect data]

  // Read a single row.
  //! [read row]
  auto result = table.ReadRow("my-key", bigtable::Filter::PassAllFilter());
  //! [read row]

  // Handle the case where the row does not exist.
  //! [check result]
  if (not result.first) {
    std::cout << "Cannot find row 'my-key' in the table: " << table.table_name()
              << std::endl;
    return 0;
  }
  //! [check result]

  // Print the contents of the row.
  //! [use value]
  for (auto const& cell : result.second.cells()) {
    std::cout << cell.family_name() << ":" << cell.column_qualifier()
              << "    @ " << cell.timestamp() << "\n"
              << '"' << cell.value() << '"' << std::endl;
  }
  //! [use value]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
