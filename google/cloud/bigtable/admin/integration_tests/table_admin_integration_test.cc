// Copyright 2021 Google Inc.
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
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/bigtable/wait_for_consistency.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::TableTestEnvironment;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Not;

namespace btadmin = ::google::bigtable::admin::v2;

class TableAdminIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  // Extract the table names or return an error.
  StatusOr<std::vector<std::string>> ListTables() {
    btadmin::ListTablesRequest r;
    r.set_parent(bigtable::InstanceName(project_id(), instance_id()));
    r.set_view(btadmin::Table::NAME_ONLY);
    auto tables = client_.ListTables(std::move(r));

    std::vector<std::string> names;
    for (auto const& table : tables) {
      if (!table) return table.status();
      names.push_back(table->name());
    }
    return names;
  }

  BigtableTableAdminClient client_ = bigtable::testing::TableAdminClient();
};

TEST_F(TableAdminIntegrationTest, TableListWithMultipleTables) {
  std::vector<std::string> expected_tables;
  auto const instance_name =
      bigtable::InstanceName(project_id(), instance_id());

  // Create tables
  int constexpr kTableCount = 5;
  for (int index = 0; index != kTableCount; ++index) {
    std::string table_id = RandomTableId();
    EXPECT_STATUS_OK(client_.CreateTable(instance_name, table_id, {}));
    expected_tables.emplace_back(
        bigtable::TableName(project_id(), instance_id(), std::move(table_id)));
  }
  auto tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, IsSupersetOf(expected_tables));

  // Delete the tables so future tests have a clean slate.
  for (auto const& t : expected_tables) {
    EXPECT_STATUS_OK(client_.DeleteTable(t));
  }

  // Verify the tables were deleted.
  tables = ListTables();
  ASSERT_STATUS_OK(tables);
  for (auto const& t : expected_tables) {
    EXPECT_THAT(*tables, Not(Contains(t)));
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
  btadmin::DropRowRangeRequest r;
  r.set_name(table.table_name());
  r.set_row_key_prefix(std::move(row_key1_prefix));
  EXPECT_STATUS_OK(client_.DropRowRange(std::move(r)));
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
  btadmin::DropRowRangeRequest r;
  r.set_name(table.table_name());
  r.set_delete_all_data_from_table(true);
  EXPECT_STATUS_OK(client_.DropRowRange(std::move(r)));
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  ASSERT_TRUE(actual_cells.empty());
}

TEST_F(TableAdminIntegrationTest, CreateListGetDeleteTable) {
  auto const table_id = RandomTableId();
  auto const instance_name =
      bigtable::InstanceName(project_id(), instance_id());
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);

  // Configure create table request
  btadmin::CreateTableRequest r;
  r.set_parent(instance_name);
  r.set_table_id(table_id);

  auto constexpr kSecondsPerDay =
      std::chrono::seconds(std::chrono::hours(24)).count();

  auto& t = *r.mutable_table();
  auto& families = *t.mutable_column_families();
  families["fam"].mutable_gc_rule()->set_max_num_versions(10);
  families["foo"].mutable_gc_rule()->mutable_max_age()->set_seconds(
      kSecondsPerDay);
  for (auto&& split : {"a1000", "a2000", "b3000", "m5000"}) {
    r.add_initial_splits()->set_key(std::move(split));
  }

  // Create table
  ASSERT_STATUS_OK(client_.CreateTable(std::move(r)));
  bigtable::Table table(data_client_, table_id);

  // List tables
  auto tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, Contains(table_name));

  // Get table
  auto table_detailed = client_.GetTable(table_name);
  ASSERT_STATUS_OK(table_detailed);

  // Verify new table was created
  EXPECT_EQ(table.table_name(), table_name);
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
  using Mod = btadmin::ModifyColumnFamiliesRequest::Modification;
  std::vector<Mod> mods(3);

  mods[0].set_id("newfam");
  auto& gc_create = *mods[0].mutable_create()->mutable_gc_rule();
  auto& intersection = *gc_create.mutable_intersection();
  intersection.add_rules()->set_max_num_versions(1);
  intersection.add_rules()->mutable_max_age()->set_seconds(7 * kSecondsPerDay);

  mods[1].set_id("fam");
  mods[1].mutable_update()->mutable_gc_rule()->set_max_num_versions(2);

  mods[2].set_id("foo");
  mods[2].set_drop(true);

  auto table_modified = client_.ModifyColumnFamilies(table_name, mods);
  ASSERT_STATUS_OK(table_modified);
  EXPECT_EQ(1, count_matching_families(*table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(*table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(*table_modified, "newfam"));
  auto const& gc = table_modified->column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // Delete table
  EXPECT_STATUS_OK(client_.DeleteTable(table_name));

  // List to verify it is no longer there
  tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, Not(Contains(table_name)));
}

TEST_F(TableAdminIntegrationTest, WaitForConsistencyCheck) {
  // WaitForConsistencyCheck() only makes sense on a replicated table, we need
  // to create an instance with at least 2 clusters to test it.
  auto const id = TableTestEnvironment::RandomInstanceId();
  auto const random_table_id = RandomTableId();
  auto const project_name = Project(project_id()).FullName();
  auto const instance_name = bigtable::InstanceName(project_id(), id);
  auto const table_name =
      bigtable::TableName(project_id(), id, random_table_id);

  // Create a new instance and a new table.
  auto instance_admin_client =
      BigtableInstanceAdminClient(MakeBigtableInstanceAdminConnection());

  // The instance configuration is involved. It needs two clusters, which must
  // be production clusters (and therefore have at least 3 nodes each), and
  // they must be in different zones. Also, the display name cannot be longer
  // than 30 characters.
  auto const display_name = ("IT " + id).substr(0, 30);

  btadmin::Instance in;
  in.set_display_name(std::move(display_name));

  btadmin::Cluster c1;
  c1.set_location(project_name + "/locations/" +
                  TableTestEnvironment::zone_a());
  c1.set_serve_nodes(3);
  c1.set_default_storage_type(btadmin::StorageType::HDD);

  btadmin::Cluster c2;
  c2.set_location(project_name + "/locations/" +
                  TableTestEnvironment::zone_b());
  c2.set_serve_nodes(3);
  c2.set_default_storage_type(btadmin::StorageType::HDD);

  // Create the new instance.
  auto instance = instance_admin_client
                      .CreateInstance(project_name, id, std::move(in),
                                      {{id + "-c1", std::move(c1)},
                                       {id + "-c2", std::move(c2)}})
                      .get();
  ASSERT_STATUS_OK(instance);

  // The table is going to be very simple, just one column family.
  std::string const family = "column_family";

  btadmin::Table t;
  auto& families = *t.mutable_column_families();
  families[family].mutable_gc_rule()->set_max_num_versions(10);

  // Create the new table.
  auto table_created =
      client_.CreateTable(instance_name, random_table_id, std::move(t));
  ASSERT_STATUS_OK(table_created);

  // We need to mutate the data in the table and then wait for those mutations
  // to propagate to both clusters. First create a `bigtable::Table` object.
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto table = bigtable::Table(
      bigtable::MakeDataConnection(
          Options{}.set<GrpcCompletionQueueOption>(background.cq())),
      bigtable::TableResource(project_id(), id, random_table_id));

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
  auto consistency_token_resp = client_.GenerateConsistencyToken(table_name);
  ASSERT_STATUS_OK(consistency_token_resp);
  auto consistency_token =
      std::move(*consistency_token_resp->mutable_consistency_token());

  // Verify that our clusters are eventually consistent. This calls
  // `AsyncCheckConsistency` under the hood.
  auto result = AsyncWaitForConsistency(background.cq(), client_,
                                        table.table_name(), consistency_token);
  ASSERT_STATUS_OK(result.get());

  // Make a synchronous call, just to test all functions.
  auto resp = client_.CheckConsistency(table_name, consistency_token);
  ASSERT_STATUS_OK(resp);
  EXPECT_TRUE(resp->consistent());

  // Cleanup the table and the instance.
  EXPECT_STATUS_OK(client_.DeleteTable(table_name));
  EXPECT_STATUS_OK(instance_admin_client.DeleteInstance(instance_name));
}

TEST_F(TableAdminIntegrationTest, CreateListGetDeleteTableWithLogging) {
  // In our ci builds, we set GOOGLE_CLOUD_CPP_ENABLE_TRACING to log our tests,
  // by default. We should unset this variable and create a fresh client in
  // order to have a conclusive test.
  testing_util::ScopedEnvironment env = {"GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                         absl::nullopt};
  testing_util::ScopedLog log;
  auto const table_id = RandomTableId();
  auto const instance_name =
      bigtable::InstanceName(project_id(), instance_id());
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);

  BigtableTableAdminClient client =
      BigtableTableAdminClient(MakeBigtableTableAdminConnection(
          Options{}.set<TracingComponentsOption>({"rpc"})));

  // Configure create table request
  btadmin::CreateTableRequest r;
  r.set_parent(instance_name);
  r.set_table_id(table_id);

  auto constexpr kSecondsPerDay =
      std::chrono::seconds(std::chrono::hours(24)).count();

  auto& t = *r.mutable_table();
  auto& families = *t.mutable_column_families();
  families["fam"].mutable_gc_rule()->set_max_num_versions(10);
  families["foo"].mutable_gc_rule()->mutable_max_age()->set_seconds(
      kSecondsPerDay);
  for (auto&& split : {"a1000", "a2000", "b3000", "m5000"}) {
    r.add_initial_splits()->set_key(std::move(split));
  }

  // Create table
  ASSERT_STATUS_OK(client.CreateTable(std::move(r)));
  bigtable::Table table(data_client_, table_id);

  // List tables
  auto tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, Contains(table_name));

  // Get table
  auto table_detailed = client.GetTable(table_name);
  ASSERT_STATUS_OK(table_detailed);

  // Verify new table was created
  EXPECT_EQ(table.table_name(), table_name);
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
  using Mod = btadmin::ModifyColumnFamiliesRequest::Modification;
  std::vector<Mod> mods(3);

  mods[0].set_id("newfam");
  auto& gc_create = *mods[0].mutable_create()->mutable_gc_rule();
  auto& intersection = *gc_create.mutable_intersection();
  intersection.add_rules()->set_max_num_versions(1);
  intersection.add_rules()->mutable_max_age()->set_seconds(7 * kSecondsPerDay);

  mods[1].set_id("fam");
  mods[1].mutable_update()->mutable_gc_rule()->set_max_num_versions(2);

  mods[2].set_id("foo");
  mods[2].set_drop(true);

  auto table_modified = client.ModifyColumnFamilies(table_name, mods);
  ASSERT_STATUS_OK(table_modified);
  EXPECT_EQ(1, count_matching_families(*table_modified, "fam"));
  EXPECT_EQ(0, count_matching_families(*table_modified, "foo"));
  EXPECT_EQ(1, count_matching_families(*table_modified, "newfam"));
  auto const& gc = table_modified->column_families().at("newfam").gc_rule();
  EXPECT_TRUE(gc.has_intersection());
  EXPECT_EQ(2, gc.intersection().rules_size());

  // Delete table
  EXPECT_STATUS_OK(client.DeleteTable(table_name));

  // List to verify it is no longer there
  tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, Not(Contains(table_name)));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateTable")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListTables")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetTable")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ModifyColumnFamilies")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteTable")));

  // Verify that a normal client does not log.
  auto no_logging_client =
      BigtableTableAdminClient(MakeBigtableTableAdminConnection());
  (void)no_logging_client.ListTables(instance_name);
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr("ListTables"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableAdminTestEnvironment);
  return RUN_ALL_TESTS();
}
