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

#include <absl/memory/memory.h>
#include <absl/strings/str_join.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <bigtable/admin/admin_client.h>
#include <bigtable/admin/table_admin.h>
#include "bigtable/client/testing/table_integration_test.h"

#include <string>
#include <vector>

using namespace std;
namespace admin_proto = ::google::bigtable::admin::v2;

namespace bigtable {
namespace testing {

class AdminIntegrationTest : public TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;

  void SetUp() { CreateTableAdmin(); }

  void TearDown() {}

  void CreateTableAdmin() {
    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::CreateDefaultAdminClient(
            bigtable::testing::TableTestEnvironment::project_id(),
            bigtable::ClientOptions());
    table_admin_ = absl::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
  }

  bool TestForTableListCheck(vector<string> expectedTableList) {
    vector<string> actualTableList;
    vector<string> diffTableList;

    auto tableList = table_admin_->ListTables(admin_proto::Table::NAME_ONLY);
    for (auto const& tableName : tableList)
      actualTableList.push_back(tableName.name());

    // Sort actualTableList
    sort(actualTableList.begin(), actualTableList.end());
    // Sort expectedTableList
    sort(expectedTableList.begin(), expectedTableList.end());

    // Get the difference of expectedTableList and actualTableList
    set_difference(expectedTableList.begin(), expectedTableList.end(),
                   actualTableList.begin(), actualTableList.end(),
                   back_inserter(diffTableList));

    return diffTableList.empty();
  }

  bool checkTableSchema(admin_proto::Table const& actualTable,
                        string const& expectedText, string const& message) {
    admin_proto::Table expectedTable;

    // ASSERT_FALSE(google::protobuf::TextFormat::ParseFromString(expectedText,
    // &expectedTable));
    if (not google::protobuf::TextFormat::ParseFromString(expectedText,
                                                          &expectedTable))
      return false;

    string delta;
    google::protobuf::util::MessageDifferencer msgDifferencer;
    msgDifferencer.ReportDifferencesToString(&delta);
    // ASSERT_FALSE(msgDifferencer.Compare(expectedTable, actualTable));
    if (not msgDifferencer.Compare(expectedTable, actualTable)) return false;

    return true;
  }
};
}
}

namespace bigtable {
namespace testing {

/**
 *  Test case for checking if Instance is empty or not.
 *  If instance is empty then go ahead.
 *  If instance is not empty the throw assertion
 * */
TEST_F(AdminIntegrationTest, CheckInstanceIsEmpty) {
  namespace admin_proto = ::google::bigtable::admin::v2;

  ASSERT_TRUE(table_admin_->ListTables(admin_proto::Table::NAME_ONLY).empty());
}

/***
 * Test case for checking create table
 * If created tableID and passed tableID is same then test is successful.
 *
 * */
TEST_F(AdminIntegrationTest, CheckCreateTable) {
  string table_id = "table0";

  // Create Table
  auto table = table_admin_->CreateTable(table_id, bigtable::TableConfig());

  // Check table is created properly
  auto table_result = table_admin_->GetTable(table_id);

  ASSERT_EQ(table.name(), table_result.name());

  // Delete this table so that next run should not throw error
  table_admin_->DeleteTable(table_id);
}

/**
 * Check if list of table names are matching with the
 * expected tablename list
 *
 * */

TEST_F(AdminIntegrationTest, CheckTableListWithSingleTable) {
  const string table_id = "table0";

  // Create table first here.
  table_admin_->CreateTable(table_id, bigtable::TableConfig());

  vector<string> expectedTableList = {
      absl::StrCat(table_admin_->instance_name(), "/tables/", table_id)};

  bool tableListIsEmpty = TestForTableListCheck(expectedTableList);

  // Delete the created table here, so it should not interfere with other test
  // cases
  table_admin_->DeleteTable(table_id);

  ASSERT_TRUE(tableListIsEmpty);
}

TEST_F(AdminIntegrationTest, CheckTableListWithMultipleTable) {
  const string table_prefix = "table";
  int table_count = 5;
  vector<string> expectedTableList;

  // Create multiple tableID in loop
  for (int index = 0; index < table_count; index++) {
    string table_id = table_prefix + to_string(index);
    // Create table First
    table_admin_->CreateTable(table_id, bigtable::TableConfig());

    expectedTableList.push_back(
        absl::StrCat(table_admin_->instance_name(), "/tables/", table_id));
  }

  bool tableListIsEmpty = TestForTableListCheck(expectedTableList);

  // Delete the created table here, so it should not interfere with other test
  // cases
  for (int index = 0; index < table_count; index++) {
    string table_id = table_prefix + to_string(index);
    table_admin_->DeleteTable(table_id);
  }

  ASSERT_TRUE(tableListIsEmpty);
}

TEST_F(AdminIntegrationTest, CheckModifyTable) {
  using GC = bigtable::GcRule;
  string table_id = "table2";

  bigtable::TableConfig tab_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  auto table = table_admin_->CreateTable(table_id, tab_config);

  string expected_text_create = "name: '" + table.name() + "'\n";
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
  bool valid_schema = checkTableSchema(table_detailed, expected_text_create,
                                       "CheckModifyTable/Create");

  ASSERT_TRUE(valid_schema);

  ///
  string expected_text;
  expected_text += R"""(
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

  vector<bigtable::ColumnFamilyModification,
         allocator<bigtable::ColumnFamilyModification>>
      column_modification_list = {
          bigtable::ColumnFamilyModification::Create(
              "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                         GC::MaxNumVersions(1))),
          bigtable::ColumnFamilyModification::Update("fam",
                                                     GC::MaxNumVersions(2)),
          bigtable::ColumnFamilyModification::Drop("foo")};

  auto table_modified =
      table_admin_->ModifyColumnFamilies(table_id, column_modification_list);

  table_modified.set_name("");
  valid_schema = checkTableSchema(table_modified, expected_text,
                                  "CheckModifyTable/Modify");

  // Delete table so that is should not interfere with the test again on same
  // instance.
  table_admin_->DeleteTable(table_id);

  ASSERT_TRUE(valid_schema);
}
// Test Cases Finished
}
}

int main(int argc, char* argv[]) try {
  ::testing::InitGoogleTest(&argc, argv);

  // Check for arguments validity
  if (argc != 3) {
    string const cmd = argv[0];
    auto last_slash = string(cmd).find_last_of("/");
    // Show Usage if invalid no of arguments
    cerr << "Usage: " << cmd.substr(last_slash + 1)
         << "<project_id> <instance_id>" << endl;
    return 1;
  }

  string project_id = argv[0];
  string instance_id = argv[1];

  (void)::testing::AddGlobalTestEnvironment(
      new bigtable::testing::TableTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
} catch (exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
} catch (...) {
  std::cerr << "Unknown exception raised." << std::endl;
  return 1;
}
