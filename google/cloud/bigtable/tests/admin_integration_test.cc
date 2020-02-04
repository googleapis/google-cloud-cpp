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
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace {
namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

class AdminIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;
  std::string service_account_;
  std::unique_ptr<bigtable::InstanceAdmin> instance_admin_;

  void SetUp() {
    if (google::cloud::internal::GetEnv(
            "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS")
            .value_or("") != "yes") {
      GTEST_SKIP();
    }
    TableIntegrationTest::SetUp();
    service_account_ = google::cloud::internal::GetEnv(
                           "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT")
                           .value_or("");
    ASSERT_FALSE(service_account_.empty());

    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::CreateDefaultAdminClient(
            bigtable::testing::TableTestEnvironment::project_id(),
            bigtable::ClientOptions());
    table_admin_ = google::cloud::internal::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        bigtable::testing::TableTestEnvironment::project_id(),
        bigtable::ClientOptions());
    instance_admin_ =
        google::cloud::internal::make_unique<bigtable::InstanceAdmin>(
            instance_admin_client);
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

  int CountMatchingBackups(std::string const& cluster_id,
          std::string const& backup_id,
                          std::vector<btadmin::Backup> const& backups) {
    std::string backup_name =
        table_admin_->instance_name() + "/clusters/" + cluster_id + "/backups/" + backup_id;
    auto count = std::count_if(backups.begin(), backups.end(),
                               [&backup_name](btadmin::Backup const& t) {
                                 return backup_name == t.name();
                               });
    return static_cast<int>(count);
  }

};
}  // namespace

TEST_F(AdminIntegrationTest, TableListWithMultipleTables) {
  std::vector<std::string> expected_table_list;
  auto table_config = bigtable::TableConfig();

  // Get the current list of tables.
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);

  int const TABLE_COUNT = 5;
  for (int index = 0; index < TABLE_COUNT; ++index) {
    std::string table_id = RandomTableId();
    auto previous_count = CountMatchingTables(table_id, *previous_table_list);
    ASSERT_EQ(0, previous_count) << "Table (" << table_id << ") already exists."
                                 << " This is unexpected, as the table ids are"
                                 << " generated at random.";
    EXPECT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));

    expected_table_list.emplace_back(table_id);
  }
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : expected_table_list) {
    EXPECT_EQ(1, CountMatchingTables(table_id, *current_table_list));
  }
  for (auto const& table_id : expected_table_list) {
    EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  }
  current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  // Delete the tables so future tests have a clean slate.
  for (auto const& table_id : expected_table_list) {
    EXPECT_EQ(0, CountMatchingTables(table_id, *current_table_list));
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
  auto table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(0, table_count);
}

/// @test Verify that `bigtable::TableAdmin` WaitForConsistencyCheck works as
/// expected.
TEST_F(AdminIntegrationTest, WaitForConsistencyCheck) {
  using namespace google::cloud::testing_util::chrono_literals;

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

/// @test Verify that IAM Policy APIs work as expected.
TEST_F(AdminIntegrationTest, SetGetTestIamAPIsTest) {
  // TODO(#151) - remove workarounds for emulator bugs(s)
  if (UsingCloudBigtableEmulator()) GTEST_SKIP();
  auto iam_policy = bigtable::IamPolicy({bigtable::IamBinding(
      "roles/bigtable.reader", {"serviceAccount:" + service_account_})});

  auto initial_policy = table_admin_->SetIamPolicy(table_id, iam_policy);
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = table_admin_->GetIamPolicy(table_id);
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version(), fetched_policy->version());
  EXPECT_EQ(initial_policy->etag(), fetched_policy->etag());

  auto permission_set = table_admin_->TestIamPermissions(
      table_id, {"bigtable.tables.get", "bigtable.tables.readRows"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
}

/// @test Verify that `bigtable::TableAdmin` Backup CRUD operations work as expected.
TEST_F(AdminIntegrationTest, CreateListGetUpdateDeleteBackup) {
  using GC = bigtable::GcRule;
  std::string const table_id = RandomTableId();

  // verify new table id in current table list
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
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
  ASSERT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));

  auto clusters_list = instance_admin_->ListClusters();
  ASSERT_STATUS_OK((clusters_list));
  std::string const backup_cluster_full_name = clusters_list->clusters.begin()->name();
  std::string const backup_cluster_id = backup_cluster_full_name.substr(
          backup_cluster_full_name.rfind("/") + 1, backup_cluster_full_name.size() -
          backup_cluster_full_name.rfind("/"));
  std::string const backup_id = RandomBackupId();
  std::string const backup_full_name = backup_cluster_full_name + "/backups/" + backup_id;

  // list backups to verify new backup id does not already exist
  auto previous_backup_list =
      table_admin_->ListBackups({});
  ASSERT_STATUS_OK(previous_backup_list);
  auto previous_backup_count = CountMatchingBackups(backup_cluster_id, backup_id,
          *previous_backup_list);
  ASSERT_EQ(0, previous_backup_count) << "Backup (" << backup_id << ") already exists."
                               << " This is unexpected, as the backup ids are"
                               << " generated at random.";
  // create backup
  google::protobuf::Timestamp const expire_time =
          google::protobuf::util::TimeUtil::GetCurrentTime() +
          google::protobuf::util::TimeUtil::HoursToDuration(12);

  auto created_backup = table_admin_->CreateBackup({backup_cluster_id, backup_id, table_id, expire_time});
  ASSERT_STATUS_OK(created_backup);
  EXPECT_EQ(created_backup->name(), backup_full_name);

  // get backup to verify create
  auto get_backup = table_admin_->GetBackup(backup_cluster_id, backup_id);
  ASSERT_STATUS_OK(get_backup);
  EXPECT_EQ(get_backup->name(), backup_full_name);

  // update backup
  google::protobuf::Timestamp const updated_expire_time = expire_time +
          google::protobuf::util::TimeUtil::HoursToDuration(12);
  auto updated_backup =
          table_admin_->UpdateBackup({backup_cluster_id, backup_id, updated_expire_time});

  // get backup to verify update
  auto get_updated_backup = table_admin_->GetBackup(backup_cluster_id, backup_id);
  ASSERT_STATUS_OK(get_updated_backup);
  EXPECT_EQ(get_updated_backup->name(), backup_full_name);
  EXPECT_EQ(get_updated_backup->expire_time(), updated_expire_time);

  // delete backup
  EXPECT_STATUS_OK(table_admin_->DeleteBackup(backup_cluster_id, backup_id));

  // list backup to verify delete
  auto post_delete_backup_list =
      table_admin_->ListBackups({});
  ASSERT_STATUS_OK(previous_backup_list);
  auto post_delete_backup_count = CountMatchingBackups(backup_cluster_id, table_id,
          *post_delete_backup_list);
  ASSERT_EQ(0, post_delete_backup_count) << "Backup (" << backup_id << ") still exists.";

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  // List to verify it is no longer there
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  auto table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(0, table_count);
}

// Test Cases Finished
#if 0
  // verify new table id in current table list
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
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
  ASSERT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));

  auto clusters_list = instance_admin_->ListClusters();
  ASSERT_STATUS_OK(clusters_list);
  std::string const backup_cluster_full_name = clusters_list->clusters.begin()->name();
  std::string const backup_cluster_id = backup_cluster_full_name.substr(
          backup_cluster_full_name.rfind("/") + 1, backup_cluster_full_name.size() -
          backup_cluster_full_name.rfind("/"));
  std::string const backup_id = RandomBackupId();
  std::string const backup_full_name = backup_cluster_full_name + "/backups/" + backup_id;

  // list backups to verify new backup id does not already exist
  auto previous_backup_list =
      table_admin_->ListBackups({});
  ASSERT_STATUS_OK(previous_backup_list);
  auto previous_backup_count = CountMatchingBackups(backup_cluster_id, backup_id,
          *previous_backup_list);
  ASSERT_EQ(0, previous_backup_count) << "Backup (" << backup_id << ") already exists."
                               << " This is unexpected, as the backup ids are"
                               << " generated at random.";
  // create backup
  google::protobuf::Timestamp const expire_time =
          google::protobuf::util::TimeUtil::GetCurrentTime() +
          google::protobuf::util::TimeUtil::HoursToDuration(12);
  auto created_backup = table_admin_->CreateBackup(
          {backup_cluster_id, backup_id, table_id, expire_time});
  ASSERT_STATUS_OK(created_backup);
  EXPECT_EQ(created_backup->name(), backup_full_name);

  // get backup to verify create
  auto get_backup = table_admin_->GetBackup(backup_cluster_id, backup_id);
  ASSERT_STATUS_OK(get_backup);
  EXPECT_EQ(get_backup->name(), backup_full_name);

  // update backup
  google::protobuf::Timestamp const updated_expire_time = expire_time +
          google::protobuf::util::TimeUtil::HoursToDuration(12);
  auto updated_backup =
          table_admin_->UpdateBackup({backup_cluster_id, backup_id, updated_expire_time});

  // get backup to verify update
  auto get_updated_backup = table_admin_->GetBackup(backup_cluster_id, backup_id);
  ASSERT_STATUS_OK(get_updated_backup);
  EXPECT_EQ(get_updated_backup->name(), backup_full_name);
  EXPECT_EQ(get_updated_backup->expire_time(), updated_expire_time);

  // delete backup
  EXPECT_STATUS_OK(table_admin_->DeleteBackup(backup_cluster_id, backup_id));

  // list backup to verify delete
  auto post_delete_backup_list =
      table_admin_->ListBackups({});
  ASSERT_STATUS_OK(previous_backup_list);
  auto post_delete_backup_count = CountMatchingBackups(backup_cluster_id, table_id,
          *post_delete_backup_list);
  ASSERT_EQ(0, post_delete_backup_count) << "Backup (" << backup_id << ") still exists.";

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  // List to verify it is no longer there
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  auto table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(0, table_count);
}
#endif
/// @test Verify that `bigtable::TableAdmin` Backup and Restore work as expected.
TEST_F(AdminIntegrationTest, RestoreTableFromBackup) {
  using GC = bigtable::GcRule;
  std::string const table_id = RandomTableId();

  // verify new table id in current table list
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
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
  ASSERT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));

  auto clusters_list = instance_admin_->ListClusters();
  ASSERT_STATUS_OK(clusters_list);
  std::string const backup_cluster_full_name = clusters_list->clusters.begin()->name();
  std::string const backup_cluster_id = backup_cluster_full_name.substr(
          backup_cluster_full_name.rfind("/") + 1, backup_cluster_full_name.size() -
          backup_cluster_full_name.rfind("/"));
  std::string const backup_id = RandomBackupId();
  std::string const backup_full_name = backup_cluster_full_name + "/backups/" + backup_id;

  // list backups to verify new backup id does not already exist
  auto previous_backup_list =
      table_admin_->ListBackups({});
  ASSERT_STATUS_OK(previous_backup_list);
  auto previous_backup_count = CountMatchingBackups(backup_cluster_id, backup_id,
          *previous_backup_list);
  ASSERT_EQ(0, previous_backup_count) << "Backup (" << backup_id << ") already exists."
                               << " This is unexpected, as the backup ids are"
                               << " generated at random.";
  // create backup
  google::protobuf::Timestamp const expire_time =
          google::protobuf::util::TimeUtil::GetCurrentTime() +
          google::protobuf::util::TimeUtil::HoursToDuration(12);
  auto created_backup = table_admin_->CreateBackup(
          {backup_cluster_id, backup_id, table_id, expire_time});
  ASSERT_STATUS_OK(created_backup);
  EXPECT_EQ(created_backup->name(), backup_full_name);

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  // List to verify it is no longer there
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  auto table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(0, table_count);

  // restore table
  auto restore_result = table_admin_->RestoreTable({table_id, backup_cluster_id, backup_id});
  EXPECT_STATUS_OK(restore_result);
  current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(1, table_count);

  // delete backup
  EXPECT_STATUS_OK(table_admin_->DeleteBackup(backup_cluster_id, backup_id));
  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
}

// Test Cases Finished

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new bigtable::testing::TableTestEnvironment);
  return RUN_ALL_TESTS();
}
