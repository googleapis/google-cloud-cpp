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
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/protobuf/util/time_util.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

class AdminBackupAsyncFutureIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  std::shared_ptr<AdminClient> admin_client_;
  std::unique_ptr<TableAdmin> table_admin_;
  std::unique_ptr<bigtable::InstanceAdmin> instance_admin_;

  void SetUp() override {
    if (google::cloud::internal::GetEnv(
            "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS")
            .value_or("") != "yes") {
      GTEST_SKIP();
    }

    TableIntegrationTest::SetUp();
    admin_client_ = CreateDefaultAdminClient(
        testing::TableTestEnvironment::project_id(), ClientOptions());
    table_admin_ = absl::make_unique<TableAdmin>(
        admin_client_, bigtable::testing::TableTestEnvironment::instance_id());
    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        bigtable::testing::TableTestEnvironment::project_id(),
        bigtable::ClientOptions());
    instance_admin_ =
        absl::make_unique<bigtable::InstanceAdmin>(instance_admin_client);
  }

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
    std::string backup_name = table_admin_->instance_name() + "/clusters/" +
                              cluster_id + "/backups/" + backup_id;
    auto count = std::count_if(backups.begin(), backups.end(),
                               [&backup_name](btadmin::Backup const& t) {
                                 return backup_name == t.name();
                               });
    return static_cast<int>(count);
  }
};

/// @test Verify that `bigtable::TableAdmin` Backup Async CRUD operations work
/// as expected.
TEST_F(AdminBackupAsyncFutureIntegrationTest,
       CreateListGetUpdateRestoreDeleteBackup) {
  std::string const table_id = RandomTableId();
  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // verify new table id in current table list
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
  auto previous_count = CountMatchingTables(table_id, *previous_table_list);
  ASSERT_EQ(0, previous_count) << "Table (" << table_id << ") already exists."
                               << " This is unexpected, as the table ids are"
                               << " generated at random.";

  TableConfig table_config({{"fam", GcRule::MaxNumVersions(5)},
                            {"foo", GcRule::MaxAge(std::chrono::hours(24))}},
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
  google::protobuf::Timestamp const expire_time =
      google::protobuf::util::TimeUtil::GetCurrentTime() +
      google::protobuf::util::TimeUtil::HoursToDuration(12);
  google::protobuf::Timestamp const updated_expire_time =
      expire_time + google::protobuf::util::TimeUtil::HoursToDuration(12);

  future<void> chain =
      table_admin_->AsyncListBackups(cq, {})
          .then([&](future<StatusOr<std::vector<btadmin::Backup>>> fut) {
            StatusOr<std::vector<btadmin::Backup>> result = fut.get();
            EXPECT_STATUS_OK(result);
            auto previous_count =
                CountMatchingBackups(backup_cluster_id, backup_id, *result);
            EXPECT_EQ(0, previous_count)
                << "Backup (" << backup_id << ") already exists."
                << " This is unexpected, as the backup ids are"
                << " generated at random.";
            return table_admin_->AsyncCreateBackup(
                cq, {backup_cluster_id, backup_id, table_id,
                     google::cloud::internal::ToChronoTimePoint(expire_time)});
          })
          .then([&](future<StatusOr<btadmin::Backup>> fut) {
            StatusOr<btadmin::Backup> result = fut.get();
            EXPECT_STATUS_OK(result);
            EXPECT_THAT(result->name(), ::testing::HasSubstr(backup_id));
            return table_admin_->AsyncGetBackup(cq, backup_cluster_id,
                                                backup_id);
          })
          .then([&](future<StatusOr<btadmin::Backup>> fut) {
            StatusOr<btadmin::Backup> get_result = fut.get();
            EXPECT_STATUS_OK(get_result);
            EXPECT_EQ(get_result->name(), backup_full_name);
            return table_admin_->AsyncUpdateBackup(
                cq, {backup_cluster_id, backup_id,
                     google::cloud::internal::ToChronoTimePoint(
                         updated_expire_time)});
          })
          .then([&](future<StatusOr<btadmin::Backup>> fut) {
            StatusOr<btadmin::Backup> update_result = fut.get();
            EXPECT_STATUS_OK(update_result);
            EXPECT_EQ(update_result->name(), backup_full_name);
            EXPECT_EQ(update_result->expire_time(), updated_expire_time);
            return table_admin_->AsyncDeleteTable(cq, table_id);
          })
          .then([&](future<Status> fut) {
            Status delete_table_result = fut.get();
            EXPECT_STATUS_OK(delete_table_result);
            return table_admin_->AsyncRestoreTable(
                cq, {table_id, backup_cluster_id, backup_id});
          })
          .then([&](future<StatusOr<btadmin::Table>> fut) {
            auto restore_result = fut.get();
            EXPECT_STATUS_OK(restore_result);
            return table_admin_->AsyncDeleteBackup(cq, backup_cluster_id,
                                                   backup_id);
          })
          .then([&](future<Status> fut) {
            Status delete_result = fut.get();
            EXPECT_STATUS_OK(delete_result);
          });
  chain.get();

  // verify table was restored
  auto current_table_list = table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(current_table_list);
  auto table_count = CountMatchingTables(table_id, *current_table_list);
  EXPECT_EQ(1, table_count);

  // delete table
  EXPECT_STATUS_OK(table_admin_->DeleteTable(table_id));

  SUCCEED();  // we expect that previous operations do not fail.

  cq.Shutdown();
  pool.join();
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
