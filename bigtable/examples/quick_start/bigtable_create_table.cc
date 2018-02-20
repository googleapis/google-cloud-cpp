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

  std::string const project_id = argv[0];
  std::string const instance_id = argv[1];
  std::string const table_id = argv[2];

  // Connect to the Cloud Bigtable Admin API.
  //! [connect]
  bigtable::TableAdmin table_admin(
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions()),
      instance_id);
  //! [connect]

  // Define the desired schema for the Table.
  //! [define schema]
  auto gc_rule = bigtable::GcRule::MaxNumVersions(1);
  bigtable::TableConfig schema({{"family", gc_rule}}, {});
  //! [define schema]

  // Create a table.
  //! [create table]
  auto returned_schema = table_admin.CreateTable(table_id, schema);
  //! [create table]

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
