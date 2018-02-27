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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/testing/table_integration_test.h"

#include <string>
#include <vector>

namespace admin_proto = ::google::bigtable::admin::v2;

namespace {

class AdminIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;

  void SetUp() {
    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::CreateDefaultAdminClient(
            bigtable::testing::TableTestEnvironment::project_id(),
            bigtable::ClientOptions());
    table_admin_ = bigtable::internal::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
  }

  void TearDown() {}

  bool TestForTableListCheck(std::vector<std::string> expected_table_list) {
    std::vector<std::string> actual_table_list;
    std::vector<std::string> diff_table_list;

    auto table_list = table_admin_->ListTables(admin_proto::Table::NAME_ONLY);
    for (auto const& table_name : table_list) {
      actual_table_list.push_back(table_name.name());
    }

    // Sort actual_table_list
    sort(actual_table_list.begin(), actual_table_list.end());
    // Sort expected_table_list
    sort(expected_table_list.begin(), expected_table_list.end());

    // Get the difference of expected_table_list and actual_table_list
    set_difference(expected_table_list.begin(), expected_table_list.end(),
                   actual_table_list.begin(), actual_table_list.end(),
                   back_inserter(diff_table_list));

    if (not diff_table_list.empty()) {
      std::cout << "Mismatched Tables: ";
      std::copy(diff_table_list.begin(), diff_table_list.end(),
                std::ostream_iterator<std::string>(std::cout, "\n"));
      std::cout << "\nactual: ";
      std::copy(actual_table_list.begin(), actual_table_list.end(),
                std::ostream_iterator<std::string>(std::cout, "\n"));
      std::cout << "\nexpected: ";
      std::copy(expected_table_list.begin(), expected_table_list.end(),
                std::ostream_iterator<std::string>(std::cout, "\n"));
      std::cout << std::endl;
    }

    return diff_table_list.empty();
  }

  bool CheckTableSchema(admin_proto::Table const& actual_table,
                        std::string const& expected_text,
                        std::string const& message) {
    admin_proto::Table expected_table;

    if (not google::protobuf::TextFormat::ParseFromString(expected_text,
                                                          &expected_table)) {
      std::cout << message << ": could not parse protobuf string <\n"
                << expected_text << ">\n"
                << std::endl;

      return false;
    }

    std::string delta;
    google::protobuf::util::MessageDifferencer message_differencer;
    message_differencer.ReportDifferencesToString(&delta);
    bool message_compare_equal =
        message_differencer.Compare(expected_table, actual_table);
    if (not message_compare_equal) {
      std::cout << message << ": mismatch expected vs actual:\n"
                << delta << std::endl;
    }

    return message_compare_equal;
  }
};

}  // namespace

/***
 * Test case for checking create table
 * If created tableID and passed tableID is same then test is successful.
 */
TEST_F(AdminIntegrationTest, CheckCreateTable) {
  std::string const table_id = "table0";

  // Create Table
  auto table = table_admin_->CreateTable(table_id, bigtable::TableConfig());
  // Check table is created properly
  auto table_result = table_admin_->GetTable(table_id);

  // Delete this table so that next run should not throw error
  table_admin_->DeleteTable(table_id);

  ASSERT_EQ(table.name(), table_result.name())
      << "Mismatched names for GetTable(" << table_id << "): " << table.name()
      << " != " << table_result.name();
}

/**
 * Check if list of table names are matching with the
 * expected tablename list
 */
TEST_F(AdminIntegrationTest, CheckTableListWithSingleTable) {
  std::string const table_id = "table0";

  // Create table first here.
  table_admin_->CreateTable(table_id, bigtable::TableConfig());

  std::vector<std::string> expected_table_list = {
      table_admin_->instance_name() + "/tables/" + table_id};

  bool list_is_empty = TestForTableListCheck(expected_table_list);

  // Delete the created table here, so it should not interfere with other test
  // cases
  table_admin_->DeleteTable(table_id);

  ASSERT_TRUE(list_is_empty);
}

TEST_F(AdminIntegrationTest, CheckTableListWithMultipleTables) {
  std::string const table_prefix = "table";
  int table_count = 5;
  std::vector<std::string> expected_table_list;

  // Create multiple table_id in loop`
  for (int index = 0; index < table_count; ++index) {
    std::string table_id = table_prefix + std::to_string(index);
    // Create table First
    table_admin_->CreateTable(table_id, bigtable::TableConfig());

    expected_table_list.emplace_back(table_admin_->instance_name() +
                                     "/tables/" + table_id);
  }

  bool list_is_empty = TestForTableListCheck(expected_table_list);

  // Delete the created table here, so it should not interfere with other test
  // cases
  for (int index = 0; index < table_count; ++index) {
    std::string table_id = table_prefix + std::to_string(index);
    table_admin_->DeleteTable(table_id);
  }

  ASSERT_TRUE(list_is_empty);
}

TEST_F(AdminIntegrationTest, CheckModifyTable) {
  using GC = bigtable::GcRule;
  std::string const table_id = "table2";

  bigtable::TableConfig tab_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  auto table = table_admin_->CreateTable(table_id, tab_config);

  std::string expected_text_create = "name: '" + table.name() + "'\n";
  // The rest is very deterministic, we control it by the previous operations:
  expected_text_create += R"""(
                          column_families {
                                             key: 'fam'
                                             value { gc_rule { max_num_versions: 5 } }
                                          }
                          column_families {
                                             key: 'foo'
                                             value { gc_rule { max_age { seconds: 86400 } } }
                                          }
                               )""";

  auto table_detailed =
      table_admin_->GetTable(table_id, admin_proto::Table::FULL);
  bool valid_schema = CheckTableSchema(table_detailed, expected_text_create,
                                       "CheckModifyTable/Create");

  ASSERT_TRUE(valid_schema);

  std::string expected_text = R"""(
                          column_families {
                                             key: 'fam'
                                             value { gc_rule { max_num_versions: 2 } }
                                          }
                          column_families {
                                             key: 'newfam'
                                             value { gc_rule { intersection {
                                                     rules { max_age { seconds: 604800 } }
                                                     rules { max_num_versions: 1 }
                                                   } } }
                                          }
                        )""";

  std::vector<bigtable::ColumnFamilyModification> column_modification_list = {
      bigtable::ColumnFamilyModification::Create(
          "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                     GC::MaxNumVersions(1))),
      bigtable::ColumnFamilyModification::Update("fam", GC::MaxNumVersions(2)),
      bigtable::ColumnFamilyModification::Drop("foo")};

  auto table_modified =
      table_admin_->ModifyColumnFamilies(table_id, column_modification_list);

  table_modified.set_name("");
  valid_schema = CheckTableSchema(table_modified, expected_text,
                                  "CheckModifyTable/Modify");

  // Delete table so that is should not interfere with the test again on same
  // instance.
  table_admin_->DeleteTable(table_id);

  ASSERT_TRUE(valid_schema);
}
// Test Cases Finished

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  // Check for arguments validity
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of("/");
    // Show Usage if invalid no of arguments
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << "<project_id> <instance_id>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[0];
  std::string const instance_id = argv[1];

  auto admin_client =
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  // If Instance is not empty then dont start test cases
  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    std::cerr << "Expected empty instance at the beginning of integration test"
              << std::endl;
    return 1;
  }

  (void)::testing::AddGlobalTestEnvironment(
      new bigtable::testing::TableTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
}
