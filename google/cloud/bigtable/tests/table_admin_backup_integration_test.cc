// Copyright 2020 Google LLC
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
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::ToProtoTimestamp;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Contains;
using ::testing::Not;
namespace btadmin = ::google::bigtable::admin::v2;
namespace bigtable = ::google::cloud::bigtable;

class AdminBackupIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  void SetUp() override {
    TableIntegrationTest::SetUp();
    table_admin_ = std::make_unique<bigtable_admin::BigtableTableAdminClient>(
        bigtable_admin::MakeBigtableTableAdminConnection());
    instance_admin_ =
        std::make_unique<bigtable_admin::BigtableInstanceAdminClient>(
            bigtable_admin::MakeBigtableInstanceAdminConnection());
  }

  std::unique_ptr<bigtable_admin::BigtableTableAdminClient> table_admin_;
  std::unique_ptr<bigtable_admin::BigtableInstanceAdminClient> instance_admin_;
};

/// @test Verify that `bigtable::TableAdmin` Backup CRUD operations work as
/// expected.
TEST_F(AdminBackupIntegrationTest, CreateListGetUpdateRestoreDeleteBackup) {
  auto const table_id = bigtable::testing::TableTestEnvironment::table_id();
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);

  auto clusters = instance_admin_->ListClusters(
      bigtable::InstanceName(project_id(), instance_id()));
  ASSERT_STATUS_OK(clusters);
  auto const cluster_name = clusters->clusters().begin()->name();
  auto const cluster_id =
      cluster_name.substr(cluster_name.rfind('/') + 1,
                          cluster_name.size() - cluster_name.rfind('/'));
  auto const backup_id = RandomBackupId();
  auto backup_name =
      bigtable::BackupName(project_id(), instance_id(), cluster_id, backup_id);

  // Create backup
  // The proto documentation says backup expiration times are in "microseconds
  // granularity":
  //   https://cloud.google.com/bigtable/docs/reference/admin/rpc/google.bigtable.admin.v2#google.bigtable.admin.v2.Backup
  auto expire_time = std::chrono::time_point_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() + std::chrono::hours(12));

  google::bigtable::admin::v2::Backup b;
  *b.mutable_expire_time() = ToProtoTimestamp(expire_time);
  auto backup = table_admin_
                    ->CreateBackup(bigtable::ClusterName(
                                       project_id(), instance_id(), cluster_id),
                                   backup_id, b)
                    .get();

  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);

  // List backups to verify new backup has been created
  auto backups = table_admin_->ListBackups(table_name);
  std::vector<google::bigtable::admin::v2::Backup> backups_list;
  for (auto& b : backups) {
    ASSERT_STATUS_OK(b);
    backups_list.push_back(*b);
  }
  EXPECT_THAT(BackupNames(backups_list), Contains(backup_name));

  // Get backup to verify create
  backup = table_admin_->GetBackup(backup_name);
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);

  // Update backup
  expire_time += std::chrono::hours(12);
  google::bigtable::admin::v2::Backup update_backup = *backup;
  *update_backup.mutable_expire_time() = ToProtoTimestamp(expire_time);
  google::protobuf::FieldMask update_mask;
  update_mask.add_paths("expire_time");
  backup = table_admin_->UpdateBackup(update_backup, update_mask);
  ASSERT_STATUS_OK(backup);

  // Verify the update
  backup = table_admin_->GetBackup(backup_name);
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);
  EXPECT_THAT(backup->expire_time(),
              IsProtoEqual(ToProtoTimestamp(expire_time)));

  // Delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));

  // Verify the delete
  google::bigtable::admin::v2::ListTablesRequest list_request;
  list_request.set_parent(bigtable::InstanceName(project_id(), instance_id()));
  list_request.set_view(btadmin::Table::NAME_ONLY);
  auto tables = table_admin_->ListTables(list_request);
  std::vector<btadmin::Table> table_list;
  for (auto& t : tables) {
    ASSERT_STATUS_OK(t);
    table_list.push_back(*t);
  }
  EXPECT_THAT(TableNames(table_list), Not(Contains(table_name)));

  // Restore table
  google::bigtable::admin::v2::RestoreTableRequest restore_request;
  restore_request.set_parent(
      bigtable::InstanceName(project_id(), instance_id()));
  restore_request.set_backup(backup_name);
  restore_request.set_table_id(table_name);
  auto table = table_admin_->RestoreTable(restore_request).get();
  EXPECT_STATUS_OK(table);

  // Verify the restore
  tables = table_admin_->ListTables(list_request);
  table_list.clear();
  for (auto& t : tables) {
    ASSERT_STATUS_OK(t);
    table_list.push_back(*t);
  }
  EXPECT_THAT(TableNames(table_list), Contains(table_name).Times(1));

  // Delete backup
  EXPECT_STATUS_OK(table_admin_->DeleteBackup(backup_name));
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
