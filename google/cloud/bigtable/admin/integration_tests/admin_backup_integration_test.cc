// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/resource_names.h"
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
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ContainsOnce;
using ::google::protobuf::util::TimeUtil;
using ::testing::Contains;
using ::testing::Not;
namespace btadmin = ::google::bigtable::admin::v2;

class AdminBackupIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  // Extract the backup names or return an error.
  StatusOr<std::vector<std::string>> ListBackups() {
    auto backups = client_.ListBackups(
        bigtable::ClusterName(project_id(), instance_id(), "-"));

    std::vector<std::string> names;
    for (auto const& backup : backups) {
      if (!backup) return backup.status();
      names.push_back(backup->name());
    }
    return names;
  }

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

protobuf::FieldMask Mask(std::string const& path) {
  protobuf::FieldMask mask;
  mask.add_paths(path);
  return mask;
}

TEST_F(AdminBackupIntegrationTest, CreateListGetUpdateRestoreDeleteBackup) {
  auto const table_id = bigtable::testing::TableTestEnvironment::table_id();
  auto const instance_name =
      bigtable::InstanceName(project_id(), instance_id());
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);

  // Determine which cluster to make a backup of
  auto instance_admin_client =
      BigtableInstanceAdminClient(MakeBigtableInstanceAdminConnection());

  auto clusters = instance_admin_client.ListClusters(instance_name);
  ASSERT_STATUS_OK(clusters);
  auto const cluster_name = clusters->clusters().begin()->name();
  auto const backup_id = RandomBackupId();
  auto const backup_name = cluster_name + "/backups/" + backup_id;

  // Create backup
  google::protobuf::Timestamp expire_time =
      TimeUtil::GetCurrentTime() + TimeUtil::HoursToDuration(12);

  btadmin::Backup b;
  b.set_source_table(table_name);
  *b.mutable_expire_time() = expire_time;
  auto backup =
      client_.CreateBackup(cluster_name, backup_id, std::move(b)).get();
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);

  // List backups to verify new backup has been created
  auto backups = ListBackups();
  ASSERT_STATUS_OK(backups);
  EXPECT_THAT(*backups, Contains(backup_name));

  // Get backup to verify create
  backup = client_.GetBackup(backup_name);
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);

  // Update backup
  expire_time = expire_time + TimeUtil::HoursToDuration(12);
  *backup->mutable_expire_time() = expire_time;
  backup = client_.UpdateBackup(*backup, Mask("expire_time"));
  ASSERT_STATUS_OK(backup);

  // Verify the update
  backup = client_.GetBackup(backup_name);
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);
  EXPECT_EQ(backup->expire_time(), expire_time);

  // Delete table
  EXPECT_STATUS_OK(client_.DeleteTable(table_name));

  // Verify the delete
  auto tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, Not(Contains(table_name)));

  // Restore table
  btadmin::RestoreTableRequest r;
  r.set_parent(instance_name);
  r.set_table_id(table_id);
  r.set_backup(backup_name);
  auto table = client_.RestoreTable(std::move(r)).get();
  EXPECT_STATUS_OK(table);

  // Verify the restore
  tables = ListTables();
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(*tables, ContainsOnce(table_name));

  // Delete backup
  EXPECT_STATUS_OK(client_.DeleteBackup(backup_name));
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
