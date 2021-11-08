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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ContainsOnce;
using ::google::protobuf::util::TimeUtil;
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
    TableIntegrationTest::SetUp();

    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::MakeAdminClient(
            bigtable::testing::TableTestEnvironment::project_id());
    table_admin_ = absl::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
    auto instance_admin_client = bigtable::MakeInstanceAdminClient(
        bigtable::testing::TableTestEnvironment::project_id());
    instance_admin_ =
        absl::make_unique<bigtable::InstanceAdmin>(instance_admin_client);
  }
};

/// @test Verify that `bigtable::TableAdmin` Backup CRUD operations work as
/// expected.
TEST_F(AdminBackupIntegrationTest, CreateListGetUpdateRestoreDeleteBackup) {
  auto const table_id = bigtable::testing::TableTestEnvironment::table_id();
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(), table_id);

  auto clusters = instance_admin_->ListClusters(table_admin_->instance_id());
  ASSERT_STATUS_OK(clusters);
  auto const cluster_name = clusters->clusters.begin()->name();
  auto const cluster_id =
      cluster_name.substr(cluster_name.rfind('/') + 1,
                          cluster_name.size() - cluster_name.rfind('/'));
  auto const backup_id = RandomBackupId();
  auto const backup_name = cluster_name + "/backups/" + backup_id;

  // Create backup
  google::protobuf::Timestamp expire_time =
      TimeUtil::GetCurrentTime() + TimeUtil::HoursToDuration(12);

  auto backup = table_admin_->CreateBackup(
      {cluster_id, backup_id, table_id,
       google::cloud::internal::ToChronoTimePoint(expire_time)});
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);

  // List backups to verify new backup has been created
  auto backups = table_admin_->ListBackups({});
  ASSERT_STATUS_OK(backups);
  EXPECT_THAT(BackupNames(*backups), Contains(backup_name));

  // Get backup to verify create
  backup = table_admin_->GetBackup(cluster_id, backup_id);
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);

  // Update backup
  expire_time = expire_time + TimeUtil::HoursToDuration(12);
  backup = table_admin_->UpdateBackup(
      {cluster_id, backup_id,
       google::cloud::internal::ToChronoTimePoint(expire_time)});
  ASSERT_STATUS_OK(backup);

  // Verify the update
  backup = table_admin_->GetBackup(cluster_id, backup_id);
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(backup->name(), backup_name);
  EXPECT_EQ(backup->expire_time(), expire_time);

  // Delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));

  // Verify the delete
  auto tables = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(TableNames(*tables), Not(Contains(table_name)));

  // Restore table
  auto table = table_admin_->RestoreTable({table_id, cluster_id, backup_id});
  EXPECT_STATUS_OK(table);

  // Verify the restore
  tables = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(tables);
  EXPECT_THAT(TableNames(*tables), ContainsOnce(table_name));

  // Delete backup
  EXPECT_STATUS_OK(table_admin_->DeleteBackup(cluster_id, backup_id));
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
