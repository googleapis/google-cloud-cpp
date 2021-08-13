// Copyright 2020 Google LLC
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
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/util/time_util.h>
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
using ::testing::Not;
namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = ::google::cloud::bigtable;

class AdminBackupIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;
  std::unique_ptr<bigtable::InstanceAdmin> instance_admin_;

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
    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        bigtable::testing::TableTestEnvironment::project_id(),
        bigtable::ClientOptions());
    instance_admin_ =
        absl::make_unique<bigtable::InstanceAdmin>(instance_admin_client);
  }
};

/// @test Verify that `bigtable::TableAdmin` Backup CRUD operations work as
/// expected.
TEST_F(AdminBackupIntegrationTest, CreateListGetUpdateRestoreDeleteBackup) {
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

  auto clusters_list =
      instance_admin_->ListClusters(table_admin_->instance_id());
  ASSERT_STATUS_OK(clusters_list);
  std::string const backup_cluster_full_name =
      clusters_list->clusters.begin()->name();
  std::string const backup_cluster_id = backup_cluster_full_name.substr(
      backup_cluster_full_name.rfind('/') + 1,
      backup_cluster_full_name.size() - backup_cluster_full_name.rfind('/'));
  std::string const backup_id = RandomBackupId();
  std::string const backup_full_name =
      backup_cluster_full_name + "/backups/" + backup_id;

  // list backups to verify new backup id does not already exist
  auto previous_backup_list = table_admin_->ListBackups({});
  ASSERT_STATUS_OK(previous_backup_list);
  ASSERT_THAT(
      BackupNames(*previous_backup_list),
      Not(Contains(table_admin_->instance_name() + "/backups/" + backup_id)))
      << "Backup (" << backup_id << ") already exists."
      << " This is unexpected, as the backup ids are"
      << " generated at random.";
  // create backup
  google::protobuf::Timestamp const expire_time =
      google::protobuf::util::TimeUtil::GetCurrentTime() +
      google::protobuf::util::TimeUtil::HoursToDuration(12);

  auto created_backup = table_admin_->CreateBackup(
      {backup_cluster_id, backup_id, table_id,
       google::cloud::internal::ToChronoTimePoint(expire_time)});
  ASSERT_STATUS_OK(created_backup);
  EXPECT_EQ(created_backup->name(), backup_full_name);

  // get backup to verify create
  auto get_backup = table_admin_->GetBackup(backup_cluster_id, backup_id);
  ASSERT_STATUS_OK(get_backup);
  EXPECT_EQ(get_backup->name(), backup_full_name);

  // update backup
  google::protobuf::Timestamp const updated_expire_time =
      expire_time + google::protobuf::util::TimeUtil::HoursToDuration(12);
  auto updated_backup = table_admin_->UpdateBackup(
      {backup_cluster_id, backup_id,
       google::cloud::internal::ToChronoTimePoint(updated_expire_time)});

  // get backup to verify update
  auto get_updated_backup =
      table_admin_->GetBackup(backup_cluster_id, backup_id);
  ASSERT_STATUS_OK(get_updated_backup);
  EXPECT_EQ(get_updated_backup->name(), backup_full_name);
  EXPECT_EQ(get_updated_backup->expire_time(), updated_expire_time);

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  // List to verify it is no longer there
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  EXPECT_THAT(
      TableNames(*current_table_list),
      Not(Contains(table_admin_->instance_name() + "/tables/" + table_id)));

  // restore table
  auto restore_result =
      table_admin_->RestoreTable({table_id, backup_cluster_id, backup_id});
  EXPECT_STATUS_OK(restore_result);
  current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  EXPECT_THAT(
      TableNames(*current_table_list),
      ContainsOnce(table_admin_->instance_name() + "/tables/" + table_id));

  // delete backup
  EXPECT_STATUS_OK(table_admin_->DeleteBackup(backup_cluster_id, backup_id));

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));
  // List to verify it is no longer there
  current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  EXPECT_THAT(
      TableNames(*current_table_list),
      Not(Contains(table_admin_->instance_name() + "/tables/" + table_id)));
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
