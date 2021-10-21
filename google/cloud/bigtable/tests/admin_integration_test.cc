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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::ContainsOnce;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;

namespace btadmin = google::bigtable::admin::v2;

class AdminIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;

  void SetUp() override {
    if (google::cloud::internal::GetEnv(
            "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS")
            .value_or("") != "yes") {
      GTEST_SKIP();
    }
    TableIntegrationTest::SetUp();

    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::CreateDefaultAdminClient(
            bigtable::testing::TableTestEnvironment::project_id(),
            bigtable::ClientOptions());
    table_admin_ = absl::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
  }
};

TEST_F(AdminIntegrationTest, TableListWithMultipleTables) {
  std::vector<std::string> expected_table_list;
  auto table_config = bigtable::TableConfig();

  // Get the current list of tables.
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);

  int constexpr kTableCount = 5;
  for (int index = 0; index < kTableCount; ++index) {
    std::string table_id = RandomTableId();
    ASSERT_THAT(
        TableNames(*previous_table_list),
        Not(Contains(table_admin_->instance_name() + "/tables/" + table_id)))
        << "Table (" << table_id << ") already exists."
        << " This is unexpected, as the table ids are generated at random.";
    EXPECT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));

    expected_table_list.emplace_back(table_id);
  }
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : expected_table_list) {
    EXPECT_THAT(
        TableNames(*current_table_list),
        ContainsOnce(table_admin_->instance_name() + "/tables/" + table_id));
  }
  for (auto const& table_id : expected_table_list) {
    EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  }
  current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : expected_table_list) {
    EXPECT_THAT(
        TableNames(*current_table_list),
        Not(Contains(table_admin_->instance_name() + "/tables/" + table_id)));
  }
}

TEST_F(AdminIntegrationTest, DropRowsByPrefix) {
  auto table = GetTable();

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1_prefix = "DropRowPrefix1";
  std::string const row_key2_prefix = "DropRowPrefix2";
  std::string const row_key1 = row_key1_prefix + "-Key1";
  std::string const row_key1_1 = row_key1_prefix + "_1-Key1";
  std::string const row_key2 = row_key2_prefix + "-Key2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, "family1", "column_id1", 0, "v-c-0-0"},
      {row_key1, "family1", "column_id1", 1000, "v-c-0-1"},
      {row_key1, "family2", "column_id3", 2000, "v-c-0-2"},
      {row_key1_1, "family2", "column_id3", 2000, "v-c-0-2"},
      {row_key1_1, "family2", "column_id3", 3000, "v-c-0-2"},
      {row_key2, "family2", "column_id2", 2000, "v-c0-0-0"},
      {row_key2, "family3", "column_id3", 3000, "v-c1-0-2"},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key2, "family2", "column_id2", 2000, "v-c0-0-0"},
      {row_key2, "family3", "column_id3", 3000, "v-c1-0-2"}};

  // Create records
  CreateCells(table, created_cells);
  // Delete all the records for a row
  EXPECT_STATUS_OK(table_admin_->DropRowsByPrefix(
      bigtable::testing::TableTestEnvironment::table_id(), row_key1_prefix));
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(expected_cells, actual_cells);
}

TEST_F(AdminIntegrationTest, DropAllRows) {
  auto table = GetTable();

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "DropRowKey1";
  std::string const row_key2 = "DropRowKey2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, "family1", "column_id1", 0, "v-c-0-0"},
      {row_key1, "family1", "column_id1", 1000, "v-c-0-1"},
      {row_key1, "family2", "column_id3", 2000, "v-c-0-2"},
      {row_key2, "family2", "column_id2", 2000, "v-c0-0-0"},
      {row_key2, "family3", "column_id3", 3000, "v-c1-0-2"},
  };

  // Create records
  CreateCells(table, created_cells);
  // Delete all the records from a table
  EXPECT_STATUS_OK(table_admin_->DropAllRows(
      bigtable::testing::TableTestEnvironment::table_id()));
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  ASSERT_TRUE(actual_cells.empty());
}

/// @test Verify that `bigtable::TableAdmin` CRUD operations work as expected.
TEST_F(AdminIntegrationTest, CreateListGetDeleteTable) {
  using GC = bigtable::GcRule;
  std::string const table_id = RandomTableId();

  // verify new table id in current table list
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
  ASSERT_THAT(
      TableNames(*previous_table_list),
      Not(Contains(table_admin_->instance_name() + "/tables/" + table_id)))
      << "Table (" << table_id << ") already exists."
      << " This is unexpected, as the table ids are generated at random.";

  // create table config
  bigtable::TableConfig table_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  // create table
  ASSERT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));
  bigtable::Table table(data_client_, table_id);

  // verify new table was created
  auto table_result = table_admin_->GetTable(table_id);
  ASSERT_STATUS_OK(table_result);
  EXPECT_EQ(table.table_name(), table_result->name())
      << "Mismatched names for GetTable(" << table_id
      << "): " << table.table_name() << " != " << table_result->name();

  // get table
  auto table_detailed = table_admin_->GetTable(table_id, btadmin::Table::FULL);
  ASSERT_STATUS_OK(table_detailed);
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
  EXPECT_EQ(1, count_matching_families(*table_detailed, "fam"));
  EXPECT_EQ(1, count_matching_families(*table_detailed, "foo"));

  // update table
  std::vector<bigtable::ColumnFamilyModification> column_modification_list = {
      bigtable::ColumnFamilyModification::Create(
          "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                     GC::MaxNumVersions(1))),
      bigtable::ColumnFamilyModification::Update("fam", GC::MaxNumVersions(2)),
      bigtable::ColumnFamilyModification::Drop("foo")};

  auto table_modified =
      table_admin_->ModifyColumnFamilies(table_id, column_modification_list);
  ASSERT_STATUS_OK(table_modified);
  EXPECT_EQ(1, count_matching_families(*table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(*table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(*table_modified, "newfam"));
  auto const& gc = table_modified->column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  // List to verify it is no longer there
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  EXPECT_THAT(
      TableNames(*current_table_list),
      Not(Contains(table_admin_->instance_name() + "/tables/" + table_id)));
}

/// @test Verify that `bigtable::TableAdmin` WaitForConsistencyCheck works as
/// expected.
TEST_F(AdminIntegrationTest, WaitForConsistencyCheck) {
  // WaitForConsistencyCheck() only makes sense on a replicated table, we need
  // to create an instance with at least 2 clusters to test it.
  auto project_id = bigtable::testing::TableTestEnvironment::project_id();
  std::string id = bigtable::testing::TableTestEnvironment::RandomInstanceId();
  std::string const random_table_id = RandomTableId();

  // Create a bigtable::InstanceAdmin and a bigtable::TableAdmin to create the
  // new instance and the new table.
  auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
      project_id, bigtable::ClientOptions());
  bigtable::InstanceAdmin instance_admin(instance_admin_client);

  auto admin_client =
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin table_admin(admin_client, id);

  // The instance configuration is involved, it needs two clusters, which must
  // be production clusters (and therefore have at least 3 nodes each), and
  // they must be in different zones. Also, the display name cannot be longer
  // than 30 characters.
  auto display_name = ("IT " + id).substr(0, 30);
  auto cluster_config_1 =
      bigtable::ClusterConfig(bigtable::testing::TableTestEnvironment::zone_a(),
                              3, bigtable::ClusterConfig::HDD);
  auto cluster_config_2 =
      bigtable::ClusterConfig(bigtable::testing::TableTestEnvironment::zone_b(),
                              3, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(
      id, display_name,
      {{id + "-c1", cluster_config_1}, {id + "-c2", cluster_config_2}});

  // Create the new instance.
  auto instance = instance_admin.CreateInstance(config).get();
  ASSERT_STATUS_OK(instance);

  // The table is going to be very simple, just one column family.
  std::string const family = "column_family";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{family, bigtable::GcRule::MaxNumVersions(10)}}, {});

  // Create the new table.
  auto table_created = table_admin.CreateTable(random_table_id, table_config);
  ASSERT_STATUS_OK(table_created);

  // We need to mutate the data in the table and then wait for those mutations
  // to propagate to both clusters. First create a `bigtable::Table` object.
  auto data_client = bigtable::CreateDefaultDataClient(
      project_id, id, bigtable::ClientOptions());
  bigtable::Table table(data_client, random_table_id);

  // Insert some cells into the table.
  std::string const row_key1 = "check-consistency-row1";
  std::string const row_key2 = "check-consistency-row2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, family, "column1", 1000, "not interesting"},
      {row_key1, family, "column2", 1000, "not interesting"},
      {row_key1, family, "column1", 2000, "not interesting"},
      {row_key2, family, "column2", 2000, "not interesting"},
      {row_key2, family, "column1", 3000, "not interesting"},
  };
  CreateCells(table, created_cells);

  // Create a consistency token after modifying the table.
  auto consistency_token =
      table_admin.GenerateConsistencyToken(random_table_id);
  ASSERT_STATUS_OK(consistency_token);

  // Wait until all the mutations before the `consistency_token` have propagated
  // everywhere.
  google::cloud::future<google::cloud::StatusOr<bigtable::Consistency>> result =
      table_admin.WaitForConsistency(random_table_id, *consistency_token);
  auto is_consistent = result.get();
  ASSERT_STATUS_OK(is_consistent);
  EXPECT_EQ(bigtable::Consistency::kConsistent, *is_consistent);

  // Cleanup the table and the instance.
  EXPECT_STATUS_OK(table_admin.DeleteTable(random_table_id));
  EXPECT_STATUS_OK(instance_admin.DeleteInstance(id));
}

/// @test Verify rpc logging for `bigtable::TableAdmin`
TEST_F(AdminIntegrationTest, CreateListGetDeleteTableWithLogging) {
  using GC = bigtable::GcRule;
  testing_util::ScopedLog log;

  std::string const table_id = RandomTableId();

  std::shared_ptr<bigtable::AdminClient> admin_client =
      bigtable::CreateDefaultAdminClient(
          bigtable::testing::TableTestEnvironment::project_id(),
          bigtable::ClientOptions().enable_tracing("rpc"));
  auto table_admin = absl::make_unique<bigtable::TableAdmin>(
      admin_client, bigtable::testing::TableTestEnvironment::instance_id());

  // verify new table id in current table list
  auto previous_table_list = table_admin->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
  ASSERT_THAT(
      TableNames(*previous_table_list),
      Not(Contains(table_admin->instance_name() + "/tables/" + table_id)))
      << "Table (" << table_id << ") already exists."
      << " This is unexpected, as the table ids are generated at random.";

  // create table config
  bigtable::TableConfig table_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  // create table
  ASSERT_STATUS_OK(table_admin->CreateTable(table_id, table_config));
  bigtable::Table table(data_client_, table_id);

  // verify new table was created
  auto table_result = table_admin->GetTable(table_id);
  ASSERT_STATUS_OK(table_result);
  EXPECT_EQ(table.table_name(), table_result->name())
      << "Mismatched names for GetTable(" << table_id
      << "): " << table.table_name() << " != " << table_result->name();

  // get table
  auto table_detailed = table_admin->GetTable(table_id, btadmin::Table::FULL);
  ASSERT_STATUS_OK(table_detailed);
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
  EXPECT_EQ(1, count_matching_families(*table_detailed, "fam"));
  EXPECT_EQ(1, count_matching_families(*table_detailed, "foo"));

  // update table
  std::vector<bigtable::ColumnFamilyModification> column_modification_list = {
      bigtable::ColumnFamilyModification::Create(
          "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                     GC::MaxNumVersions(1))),
      bigtable::ColumnFamilyModification::Update("fam", GC::MaxNumVersions(2)),
      bigtable::ColumnFamilyModification::Drop("foo")};

  auto table_modified =
      table_admin->ModifyColumnFamilies(table_id, column_modification_list);
  ASSERT_STATUS_OK(table_modified);
  EXPECT_EQ(1, count_matching_families(*table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(*table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(*table_modified, "newfam"));
  auto const& gc = table_modified->column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // delete table
  EXPECT_STATUS_OK(table_admin->DeleteTable(table_id));
  // List to verify it is no longer there
  auto current_table_list = table_admin->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  EXPECT_THAT(
      TableNames(*current_table_list),
      Not(Contains(table_admin->instance_name() + "/tables/" + table_id)));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListTables")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateTable")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetTable")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ModifyColumnFamilies")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteTable")));
}
}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableTestEnvironment);
  return RUN_ALL_TESTS();
}
