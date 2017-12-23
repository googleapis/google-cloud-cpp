// Copyright 2017 Google Inc.
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

#include <google/protobuf/text_format.h>
#include <sstream>
#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"

int main(int argc, char* argv[]) try {
  namespace admin_proto = ::google::bigtable::admin::v2;

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of("/");
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>" << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table0 = "table0";
  std::string const table1 = "table1";
  std::string const family = "fam";

  auto admin_client =
      bigtable::CreateAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  std::vector<std::string> table_names;
  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    throw std::runtime_error("Expected empty instance in integration test");
  }
  std::cout << "Initial ListTables() successful" << std::endl;

  auto t0 = admin.CreateTable(table0, bigtable::TableConfig());
  std::cout << "CreateTable(table0) successful" << std::endl;

  table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (1UL != table_list.size()) {
    throw std::runtime_error("Expected only one table after creating table0");
  }
  if (t0.name() != table_list[0].name()) {
    std::ostringstream os;
    os << "Mismatched names for table0: " << t0.name()
       << " != " << table_list[0].name();
    throw std::runtime_error(os.str());
  }
  std::cout << "ListTables() successful" << std::endl;

  auto get0 = admin.GetTable(table0);
  if (t0.name() != get0.name()) {
    std::ostringstream os;
    os << "Mismatched names for GetTable(table0): " << t0.name()
       << " != " << get0.name();
    throw std::runtime_error(os.str());
  }
  std::cout << "GetTable(table0) successful" << std::endl;

  auto t1 = admin.CreateTable(
      table1, bigtable::TableConfig(
                  {{"fam", bigtable::GcRule::MaxNumVersions(3)},
                   {"foo", bigtable::GcRule::MaxAge(std::chrono::hours(24))}},
                  {}));
  std::cout << "CreateTable(table1) successful" << std::endl;

  auto get1 = admin.GetTable(table1, admin_proto::Table::FULL);
  if (2 != get1.column_families_size()) {
    std::string text;
    google::protobuf::TextFormat::PrintToString(get1, &text);
    std::ostringstream os;
    os << "Unexpected result from GetTable(table1): " << text;
    throw std::runtime_error(os.str());
  }
  std::cout << "GetTable(table1) successful" << std::endl;

  admin.DeleteTable(table0);
  std::cout << "DeleteTable(table0) successful" << std::endl;
  table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (1UL != table_list.size()) {
    throw std::runtime_error("Expected only one table after delete table0");
  }
  if (t1.name() != table_list[0].name()) {
    throw std::runtime_error("Expected only table1 to survive");
  }
  std::cout << "ListTables() after DeleteTable() successful" << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}
