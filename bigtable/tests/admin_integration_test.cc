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

#include <absl/strings/str_join.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <sstream>
#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"

namespace {
namespace admin_proto = ::google::bigtable::admin::v2;

void CheckInstanceIsEmpty(bigtable::TableAdmin& admin) {
  std::vector<std::string> table_names;
  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    throw std::runtime_error("Expected empty instance in integration test");
  }
  std::cout << "Initial ListTables() successful" << std::endl;
}

void CheckCreateTable(bigtable::TableAdmin& admin, std::string table_id) {
  auto table = admin.CreateTable(table_id, bigtable::TableConfig());
  std::cout << "CreateTable(" << table_id << ") successful" << std::endl;

  auto get_result = admin.GetTable(table_id);
  if (table.name() != get_result.name()) {
    std::ostringstream os;
    os << "Mismatched names for GetTable(" << table_id << "): " << table.name()
       << " != " << get_result.name();
    throw std::runtime_error(os.str());
  }
  std::cout << "GetTable(" << table_id << ") successful" << std::endl;
}

void CheckTableList(bigtable::TableAdmin& admin,
                    std::vector<std::string> expected) {
  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  std::vector<std::string> actual;
  for (auto const& tbl : table_list) {
    actual.push_back(tbl.name());
  }
  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());
  std::vector<std::string> differences;
  std::set_difference(expected.begin(), expected.end(), actual.begin(),
                      actual.end(), std::back_inserter(differences));
  if (differences.empty()) {
    std::cout << "ListTables() successful" << std::endl;
    return;
  }
  std::ostringstream os;
  os << "Mismatched names: " << absl::StrJoin(differences, "\n")
     << "\nactual: " << absl::StrJoin(actual, "\n")
     << "\nexpected: " << absl::StrJoin(expected, "\n") << "\n";
  throw std::runtime_error(os.str());
}

void CheckTableSchema(admin_proto::Table const& actual,
                      std::string const& expected_text,
                      std::string const& message) {
  admin_proto::Table expected;
  if (not google::protobuf::TextFormat::ParseFromString(expected_text,
                                                        &expected)) {
    std::ostringstream os;
    os << message << ": could not parse protobuf string <\n"
       << expected_text << ">\n";
    throw std::runtime_error(os.str());
  }

  std::string delta;
  google::protobuf::util::MessageDifferencer differencer;
  differencer.ReportDifferencesToString(&delta);
  if (not differencer.Compare(expected, actual)) {
    std::ostringstream os;
    os << message << ": mismatch expected vs. actual:\n" << delta;
    throw std::runtime_error(os.str());
  }
  std::cout << message << " was successful" << std::endl;
}

void CheckModifyTable(bigtable::TableAdmin& admin, std::string table_id) {
  using GC = bigtable::GcRule;
  auto table = admin.CreateTable(
      table_id,
      bigtable::TableConfig({{"fam", GC::MaxNumVersions(5)},
                             {"foo", GC::MaxAge(std::chrono::hours(24))}},
                            {"a1000", "a2000", "b3000", "m5000"}));

  std::string expected_text_create = "name: '" + table.name() + "'\n";
  // The rest is very deterministic, we control it by the previous operations:
  expected_text_create += R"""(
column_families {
  key: 'fam'
  value { gc_rule { max_num_versions: 5 }}
}
column_families {
  key: 'foo'
  value { gc_rule { max_age { seconds: 86400 }}}
}
)""";
  auto table_detailed = admin.GetTable(table_id, admin_proto::Table::FULL);
  CheckTableSchema(table_detailed, expected_text_create,
                   "CheckModifyTable/Create");

  // TODO(google-cloud-go#837) - the emulator returns the wrong value.
  // std::string expected_text = "name: '" + table.name() + "'\n";
  std::string expected_text;
  // The rest is very deterministic, we control it by the previous operations:
  expected_text += R"""(
column_families {
  key: 'fam'
  value { gc_rule { max_num_versions: 2 }}
}
column_families {
  key: 'newfam'
  value { gc_rule { intersection {
    rules { max_age { seconds: 604800 }}
    rules { max_num_versions: 1 }
  }}}
}
)""";
  auto modified = admin.ModifyColumnFamilies(
      table_id,
      {bigtable::ColumnFamilyModification::Create(
           "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                      GC::MaxNumVersions(1))),
       bigtable::ColumnFamilyModification::Update(
           "fam", bigtable::GcRule::MaxNumVersions(2)),
       bigtable::ColumnFamilyModification::Drop("foo")});

  // TODO(google-cloud-go#837) - the emulator returns the wrong value.
  modified.set_name("");
  CheckTableSchema(modified, expected_text, "CheckModifyTable/Modify");
}
}

/**
 * @file An integration test for the bigtable::TableAdmin class.
 *
 * The continuous integration builds run this test against the Cloud Bigtable
 * Emulator.
 */
int main(int argc, char* argv[]) try {
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
  std::string const table2 = "table2";

  auto admin_client =
      bigtable::CreateAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  auto instance_name = admin.instance_name();

  CheckInstanceIsEmpty(admin);
  CheckCreateTable(admin, table0);
  CheckTableList(admin, {absl::StrCat(instance_name, "/tables/", table0)});

  CheckCreateTable(admin, table1);
  CheckTableList(admin,
                 {absl::StrCat(instance_name, "/tables/", table0),
                  absl::StrCat(instance_name, "/tables/", table1)});

  admin.DeleteTable(table0);
  CheckTableList(admin, {absl::StrCat(instance_name, "/tables/", table1)});
  std::cout << "ListTables() after DeleteTable() successful" << std::endl;

  CheckModifyTable(admin, table2);

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}
