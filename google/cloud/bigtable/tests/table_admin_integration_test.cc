// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/table_config.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/bigtable/wait_for_consistency.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Not;

namespace btadmin = ::google::bigtable::admin::v2;

class TableAdminIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable_admin::BigtableTableAdminClient> table_admin_;

  void SetUp() override {
    TableIntegrationTest::SetUp();

    table_admin_ = std::make_unique<bigtable_admin::BigtableTableAdminClient>(
        bigtable_admin::MakeBigtableTableAdminConnection());
  }
};

TEST_F(TableAdminIntegrationTest, TableListWithMultipleTables) {
  std::vector<std::string> ids;
  std::vector<std::string> expected_tables;

  auto instance_name = bigtable::InstanceName(project_id(), instance_id());

  // Create tables
  int constexpr kTableCount = 5;
  for (int index = 0; index < kTableCount; ++index) {
    std::string table_id = RandomTableId();
    EXPECT_STATUS_OK(table_admin_->CreateTable(instance_name, table_id, {}));
    ids.emplace_back(table_id);
    expected_tables.emplace_back(
        bigtable::TableName(project_id(), instance_id(), std::move(table_id)));
  }
  google::bigtable::admin::v2::ListTablesRequest list_request;
  list_request.set_parent(instance_name);
  list_request.set_view(btadmin::Table::NAME_ONLY);
  auto tables = table_admin_->ListTables(list_request);
  std::vector<btadmin::Table> table_list;
  for (auto& t : tables) {
    ASSERT_STATUS_OK(t);
    table_list.push_back(*t);
  }
  EXPECT_THAT(TableNames(table_list), IsSupersetOf(expected_tables));

  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : ids) {
    EXPECT_STATUS_OK(table_admin_->DeleteTable(
        TableName(project_id(), instance_id(), table_id)));
  }

  // Verify the tables were deleted.
  tables = table_admin_->ListTables(list_request);
  table_list.clear();
  for (auto& t : tables) {
    ASSERT_STATUS_OK(t);
    table_list.push_back(*t);
  }
  auto names = TableNames(table_list);
  for (auto const& t : expected_tables) {
    EXPECT_THAT(names, Not(Contains(t)));
  }
}

TEST_F(TableAdminIntegrationTest, DropRowsByPrefix) {
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
  google::bigtable::admin::v2::DropRowRangeRequest drop_rows_by_prefix;
  drop_rows_by_prefix.set_name(table.table_name());
  drop_rows_by_prefix.set_row_key_prefix(row_key1_prefix);
  EXPECT_STATUS_OK(table_admin_->DropRowRange(drop_rows_by_prefix));
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected_cells, actual_cells);
}

TEST_F(TableAdminIntegrationTest, DropAllRows) {
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
  google::bigtable::admin::v2::DropRowRangeRequest drop_all_rows;
  drop_all_rows.set_name(table.table_name());
  drop_all_rows.set_delete_all_data_from_table(true);
  EXPECT_STATUS_OK(table_admin_->DropRowRange(drop_all_rows));
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());
  ASSERT_TRUE(actual_cells.empty());
}

/// @test Verify that `bigtable::TableAdmin` CRUD operations work as expected.
TEST_F(TableAdminIntegrationTest, CreateListGetDeleteTable) {
  using GC = bigtable::GcRule;
  auto const table_id = RandomTableId();
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);
  auto const instance_name = InstanceName(project_id(), instance_id());

  // Create table config
  bigtable::TableConfig table_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  // Create table
  google::bigtable::admin::v2::CreateTableRequest create_request =
      std::move(table_config).as_proto();
  create_request.set_parent(instance_name);
  create_request.set_table_id(table_id);
  ASSERT_STATUS_OK(table_admin_->CreateTable(create_request));
  bigtable::Table table(MakeDataConnection(),
                        TableResource(project_id(), instance_id(), table_id));

  // List tables
  google::bigtable::admin::v2::ListTablesRequest list_request;
  list_request.set_parent(instance_name);
  list_request.set_view(btadmin::Table::NAME_ONLY);
  auto tables = table_admin_->ListTables(list_request);
  auto table_list = TableNames(tables);
  ASSERT_STATUS_OK(table_list);
  EXPECT_THAT(*table_list, Contains(table_name));

  // Get table
  google::bigtable::admin::v2::GetTableRequest get_request;
  get_request.set_name(table_name);
  get_request.set_view(btadmin::Table::FULL);
  auto table_detailed = table_admin_->GetTable(get_request);
  ASSERT_STATUS_OK(table_detailed);

  // Verify new table was created
  EXPECT_EQ(table.table_name(), table_detailed->name())
      << "Mismatched names for GetTable(" << table_id
      << "): " << table.table_name() << " != " << table_detailed->name();
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

  // Update table
  std::vector<
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest::Modification>
      column_modification_list = {
          bigtable::ColumnFamilyModification::Create(
              "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                         GC::MaxNumVersions(1)))
              .as_proto(),
          bigtable::ColumnFamilyModification::Update("fam",
                                                     GC::MaxNumVersions(2))
              .as_proto(),
          bigtable::ColumnFamilyModification::Drop("foo").as_proto()};

  auto table_modified =
      table_admin_->ModifyColumnFamilies(table_name, column_modification_list);
  ASSERT_STATUS_OK(table_modified);
  EXPECT_EQ(1, count_matching_families(*table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(*table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(*table_modified, "newfam"));
  auto const& gc = table_modified->column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // Delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_name));

  // List to verify it is no longer there
  tables = table_admin_->ListTables(list_request);
  table_list = TableNames(tables);
  ASSERT_STATUS_OK(table_list);
  EXPECT_THAT(*table_list, Not(Contains(table_name)));
}

/// @test Verify that `bigtable::TableAdmin` WaitForConsistencyCheck works as
/// expected.
TEST_F(TableAdminIntegrationTest, WaitForConsistencyCheck) {
  // WaitForConsistencyCheck() only makes sense on a replicated table, we need
  // to create an instance with at least 2 clusters to test it.
  auto const id = bigtable::testing::TableTestEnvironment::RandomInstanceId();
  auto const random_table_id = RandomTableId();

  // Create a bigtable::InstanceAdmin and a bigtable::TableAdmin to create the
  // new instance and the new table.
  auto instance_admin =
      std::make_unique<bigtable_admin::BigtableInstanceAdminClient>(
          bigtable_admin::MakeBigtableInstanceAdminConnection());
  auto table_admin_connection =
      bigtable_admin::MakeBigtableTableAdminConnection();
  auto table_admin = std::make_unique<bigtable_admin::BigtableTableAdminClient>(
      table_admin_connection);

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
  auto create_request = config.as_proto();
  create_request.set_parent(Project(project_id()).FullName());
  auto instance = instance_admin->CreateInstance(create_request).get();
  ASSERT_STATUS_OK(instance);

  // The table is going to be very simple, just one column family.
  std::string const family = "column_family";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{family, bigtable::GcRule::MaxNumVersions(10)}}, {});

  // Create the new table.
  auto request = std::move(table_config).as_proto();
  request.set_parent(InstanceName(project_id(), id));
  request.set_table_id(random_table_id);
  auto table_created = table_admin->CreateTable(request);
  ASSERT_STATUS_OK(table_created);

  // We need to mutate the data in the table and then wait for those mutations
  // to propagate to both clusters. First create a `bigtable::Table` object.
  auto table = Table(MakeDataConnection(),
                     TableResource(project_id(), id, random_table_id));
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
      table_admin->GenerateConsistencyToken(table_created->name());
  ASSERT_STATUS_OK(consistency_token);

  // Wait until all the mutations before the `consistency_token` have propagated
  // everywhere.
//  google::cloud::future<google::cloud::StatusOr<bigtable_admin::Consistency>>
//      result = google::cloud::bigtable_admin::WaitForConsistency(
//          table_admin_connection, table_created->name(),
//          consistency_token->consistency_token());

  google::bigtable::admin::v2::CheckConsistencyRequest wait_request;
  wait_request.set_name(table_created->name());
  wait_request.set_consistency_token(consistency_token->consistency_token());
  future<StatusOr<google::bigtable::admin::v2::CheckConsistencyResponse>> result = table_admin->WaitForConsistency(wait_request);
  StatusOr<google::bigtable::admin::v2::CheckConsistencyResponse> is_consistent = result.get();
  ASSERT_STATUS_OK(is_consistent);
  EXPECT_TRUE(is_consistent->consistent());

  // Cleanup the table and the instance.
  EXPECT_STATUS_OK(table_admin->DeleteTable(table_created->name()));
  EXPECT_STATUS_OK(instance_admin->DeleteInstance(instance->name()));
}

/// @test Verify rpc logging for `bigtable::TableAdmin`
TEST_F(TableAdminIntegrationTest, CreateListGetDeleteTableWithLogging) {
  using GC = bigtable::GcRule;
  // In our ci builds, we set GOOGLE_CLOUD_CPP_ENABLE_TRACING to log our tests,
  // by default. We should unset this variable and create a fresh client in
  // order to have a conclusive test.
  testing_util::ScopedEnvironment env = {"GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                         absl::nullopt};
  testing_util::ScopedLog log;

  auto const instance_name =
      bigtable::InstanceName(project_id(), instance_id());

  auto const table_id = RandomTableId();
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);

  auto table_admin = std::make_unique<bigtable_admin::BigtableTableAdminClient>(
      bigtable_admin::MakeBigtableTableAdminConnection(
          Options{}.set<LoggingComponentsOption>({"rpc"})));

  // Create table config
  bigtable::TableConfig table_config(
      {{"fam", GC::MaxNumVersions(5)},
       {"foo", GC::MaxAge(std::chrono::hours(24))}},
      {"a1000", "a2000", "b3000", "m5000"});

  // Create table
  auto request = std::move(table_config).as_proto();
  request.set_parent(instance_name);
  request.set_table_id(table_id);
  ASSERT_STATUS_OK(table_admin->CreateTable(request));
  bigtable::Table table(MakeDataConnection(),
                        TableResource(project_id(), instance_id(), table_id));

  // List tables
  google::bigtable::admin::v2::ListTablesRequest list_request;
  list_request.set_parent(instance_name);
  list_request.set_view(btadmin::Table::NAME_ONLY);
  auto tables = table_admin->ListTables(list_request);
  std::vector<btadmin::Table> table_list;
  for (auto& t : tables) {
    ASSERT_STATUS_OK(t);
    table_list.push_back(*t);
  }
  EXPECT_THAT(TableNames(table_list), Contains(table_name));

  // Get table
  google::bigtable::admin::v2::GetTableRequest get_request;
  get_request.set_name(table_name);
  get_request.set_view(btadmin::Table::FULL);
  auto table_detailed = table_admin->GetTable(get_request);
  ASSERT_STATUS_OK(table_detailed);

  // Verify new table was created
  EXPECT_EQ(table.table_name(), table_detailed->name())
      << "Mismatched names for GetTable(" << table_id
      << "): " << table.table_name() << " != " << table_detailed->name();
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

  // Update table
  std::vector<
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest::Modification>
      column_modification_list = {
          bigtable::ColumnFamilyModification::Create(
              "newfam", GC::Intersection(GC::MaxAge(std::chrono::hours(7 * 24)),
                                         GC::MaxNumVersions(1)))
              .as_proto(),
          bigtable::ColumnFamilyModification::Update("fam",
                                                     GC::MaxNumVersions(2))
              .as_proto(),
          bigtable::ColumnFamilyModification::Drop("foo").as_proto()};

  auto table_modified =
      table_admin->ModifyColumnFamilies(table_name, column_modification_list);
  ASSERT_STATUS_OK(table_modified);
  EXPECT_EQ(1, count_matching_families(*table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(*table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(*table_modified, "newfam"));
  auto const& gc = table_modified->column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // Delete table
  EXPECT_STATUS_OK(table_admin->DeleteTable(table_name));

  // List to verify it is no longer there
  tables = table_admin->ListTables(list_request);
  table_list.clear();
  for (auto& t : tables) {
    ASSERT_STATUS_OK(t);
    table_list.push_back(*t);
  }
  EXPECT_THAT(TableNames(table_list), Not(Contains(table_name)));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateTable")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListTables")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetTable")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ModifyColumnFamilies")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteTable")));

  // Verify that a normal client does not log.
  auto no_logging_client = bigtable_admin::BigtableTableAdminClient(
      bigtable_admin::MakeBigtableTableAdminConnection());
  (void)no_logging_client.ListTables(list_request);
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr("ListTables"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableAdminTestEnvironment);
  return RUN_ALL_TESTS();
}
