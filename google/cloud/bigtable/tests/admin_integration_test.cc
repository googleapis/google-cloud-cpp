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

#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace {
namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

class AdminIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;

  void SetUp() {
    TableIntegrationTest::SetUp();
    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::CreateDefaultAdminClient(
            bigtable::testing::TableTestEnvironment::project_id(),
            bigtable::ClientOptions());
    table_admin_ = google::cloud::internal::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
  }

  void TearDown() {}

  int CountMatchingTables(std::string const& table_id,
                          std::vector<btadmin::Table> const& tables) {
    std::string table_name =
        table_admin_->instance_name() + "/tables/" + table_id;
    auto count = std::count_if(tables.begin(), tables.end(),
                               [&table_name](btadmin::Table const& t) {
                                 return table_name == t.name();
                               });
    return static_cast<int>(count);
  }
};
}  // namespace

TEST_F(AdminIntegrationTest, TableListWithMultipleTablesTest) {
  std::string const table_prefix = RandomTableId();

  std::vector<std::string> expected_table_list;
  auto table_config = bigtable::TableConfig();

  // Get the current list of tables.
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_TRUE(previous_table_list);

  int const TABLE_COUNT = 5;
  for (int index = 0; index < TABLE_COUNT; ++index) {
    std::string table_id = table_prefix + "-" + std::to_string(index);
    auto previous_count = CountMatchingTables(table_id, *previous_table_list);
    ASSERT_EQ(0, previous_count) << "Table (" << table_id << ") already exists."
                                 << " This is unexpected, as the table ids are"
                                 << " generated at random.";
    CreateTable(table_id, table_config);

    expected_table_list.emplace_back(table_id);
  }
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_TRUE(current_table_list);
  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : expected_table_list) {
    EXPECT_EQ(1, CountMatchingTables(table_id, *current_table_list));
  }
  for (auto const& table_id : expected_table_list) {
    DeleteTable(table_id);
  }
  current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_TRUE(current_table_list);
  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : expected_table_list) {
    EXPECT_EQ(0, CountMatchingTables(table_id, *current_table_list));
  }
}

TEST_F(AdminIntegrationTest, DropRowsByPrefixTest) {
  std::string const table_id = RandomTableId();
  std::string const column_family1 = "family1";
  std::string const column_family2 = "family2";
  std::string const column_family3 = "family3";

  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{column_family1, bigtable::GcRule::MaxNumVersions(10)},
       {column_family2, bigtable::GcRule::MaxNumVersions(10)},
       {column_family3, bigtable::GcRule::MaxNumVersions(10)}},
      {});
  auto table = CreateTable(table_id, table_config);

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1_prefix = "DropRowPrefix1";
  std::string const row_key2_prefix = "DropRowPrefix2";
  std::string const row_key1 = row_key1_prefix + "-Key1";
  std::string const row_key1_1 = row_key1_prefix + "_1-Key1";
  std::string const row_key2 = row_key2_prefix + "-Key2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key1, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key1_1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key1_1, column_family2, "column_id3", 3000, "v-c-0-2", {}},
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}}};

  // Create records
  CreateCells(*table, created_cells);
  // Delete all the records for a row
  table_admin_->DropRowsByPrefix(table_id, row_key1_prefix);
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);

  CheckEqualUnordered(expected_cells, actual_cells);
}

TEST_F(AdminIntegrationTest, DropAllRowsTest) {
  std::string const table_id = RandomTableId();
  std::string const column_family1 = "family1";
  std::string const column_family2 = "family2";
  std::string const column_family3 = "family3";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{column_family1, bigtable::GcRule::MaxNumVersions(10)},
       {column_family2, bigtable::GcRule::MaxNumVersions(10)},
       {column_family3, bigtable::GcRule::MaxNumVersions(10)}},
      {});
  auto table = CreateTable(table_id, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "DropRowKey1";
  std::string const row_key2 = "DropRowKey2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key1, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  // Delete all the records from a table
  table_admin_->DropAllRows(table_id);
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);

  ASSERT_TRUE(actual_cells.empty());
}

/// @test Verify that `bigtable::TableAdmin` CRUD operations work as expected.
TEST_F(AdminIntegrationTest, CreateListGetDeleteTableTest) {
  using GC = bigtable::GcRule;
  std::string const table_id = RandomTableId();

  // verify new table id in current table list
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_TRUE(previous_table_list);
  auto previous_count = CountMatchingTables(table_id, *previous_table_list);
  ASSERT_EQ(0, previous_count) << "Table (" << table_id << ") already exists."
                               << " This is unexpected, as the table ids are"
                               << " generated at random.";
  // create table config
  bigtable::TableConfig table_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  // create table
  auto table = CreateTable(table_id, table_config);

  // verify new table was created
  auto table_result = table_admin_->GetTable(table_id);
  EXPECT_EQ(table->table_name(), table_result.name())
      << "Mismatched names for GetTable(" << table_id
      << "): " << table->table_name() << " != " << table_result.name();

  // get table
  auto table_detailed = table_admin_->GetTable(table_id, btadmin::Table::FULL);
  auto count_matching_families = [](btadmin::Table const& table,
                                    std::string const& name) {
    int count = 0;
    for (auto const& kv : table.column_families()) {
      if (kv.first == name) {
        ++count;
      }
    }
    return count;
  };
  EXPECT_EQ(1, count_matching_families(table_detailed, "fam"));
  EXPECT_EQ(1, count_matching_families(table_detailed, "foo"));

  // update table
  std::vector<bigtable::ColumnFamilyModification> column_modification_list = {
      bigtable::ColumnFamilyModification::Create(
          "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                     GC::MaxNumVersions(1))),
      bigtable::ColumnFamilyModification::Update("fam", GC::MaxNumVersions(2)),
      bigtable::ColumnFamilyModification::Drop("foo")};

  auto table_modified =
      table_admin_->ModifyColumnFamilies(table_id, column_modification_list);
  EXPECT_EQ(1, count_matching_families(table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(table_modified, "newfam"));
  auto const& gc = table_modified.column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // delete table
  DeleteTable(table_id);
  // List to verify it is no longer there
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_TRUE(current_table_list);
  auto table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(0, table_count);
}

/// @test Verify that `bigtable::TableAdmin` CheckConsistency works as expected.
TEST_F(AdminIntegrationTest, CheckConsistencyIntegrationTest) {
  using namespace google::cloud::testing_util::chrono_literals;

  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string const random_table_id = RandomTableId();

  auto project_id = bigtable::testing::TableTestEnvironment::project_id();

  auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
      project_id, bigtable::ClientOptions());
  bigtable::InstanceAdmin instance_admin(instance_admin_client);

  // need to create table_admin for dynamically created instance
  auto admin_client =
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin table_admin(admin_client, id);

  auto data_client = bigtable::CreateDefaultDataClient(
      project_id, id, bigtable::ClientOptions());
  bigtable::Table table(data_client, random_table_id);

  bigtable::InstanceId instance_id(id);
  bigtable::DisplayName display_name("Integration Tests " + id);

  // Replication needs at least two clusters
  auto cluster_config_1 =
      bigtable::ClusterConfig(bigtable::testing::TableTestEnvironment::zone(),
                              3, bigtable::ClusterConfig::HDD);
  auto cluster_config_2 = bigtable::ClusterConfig(
      bigtable::testing::TableTestEnvironment::replication_zone(), 3,
      bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(
      instance_id, display_name,
      {{id + "-c1", cluster_config_1}, {id + "-c2", cluster_config_2}});

  auto instance = instance_admin.CreateInstance(config).get();

  google::cloud::bigtable::TableId table_id(random_table_id);

  std::string const column_family1 = "family1";
  std::string const column_family2 = "family2";
  std::string const column_family3 = "family3";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{column_family1, bigtable::GcRule::MaxNumVersions(10)},
       {column_family2, bigtable::GcRule::MaxNumVersions(10)},
       {column_family3, bigtable::GcRule::MaxNumVersions(10)}},
      {});

  // create table
  auto table_created = table_admin.CreateTable(table_id.get(), table_config);

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "DropRowKey1";
  std::string const row_key2 = "DropRowKey2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, column_family1, "column_id1", 1000, "v-c-0-0", {}},
      {row_key1, column_family1, "column_id2", 1000, "v-c-0-1", {}},
      {row_key1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };

  CreateCells(table, created_cells);

  google::cloud::bigtable::ConsistencyToken consistency_token(
      table_admin.GenerateConsistencyToken(table_id.get()));

  auto result =
      table_admin.WaitForConsistencyCheck(table_id, consistency_token);

  EXPECT_TRUE(result.get());

  table_admin.DeleteTable(table_id.get());
  instance_admin.DeleteInstance(id);
}

// Test Cases Finished

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Check for arguments validity
  if (argc != 5) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    // Show Usage if invalid no of arguments
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << "<project_id> <instance_id> <zone> <replication_zone>"
              << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const zone = argv[3];
  std::string const replication_zone = argv[4];

  (void)::testing::AddGlobalTestEnvironment(
      new bigtable::testing::TableTestEnvironment(project_id, instance_id, zone,
                                                  replication_zone));

  return RUN_ALL_TESTS();
}
