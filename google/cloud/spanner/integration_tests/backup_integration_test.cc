// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/cleanup_stale_instances.h"
#include "google/cloud/spanner/testing/pick_instance_config.h"
#include "google/cloud/spanner/testing/policies.h"
#include "google/cloud/spanner/testing/random_backup_name.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <chrono>
#include <regex>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

class BackupTest : public testing::Test {
 public:
  BackupTest()
      : instance_admin_client_(MakeInstanceAdminConnection(
            ConnectionOptions{}, spanner_testing::TestRetryPolicy(),
            spanner_testing::TestBackoffPolicy(),
            spanner_testing::TestPollingPolicy())),
        database_admin_client_(MakeDatabaseAdminConnection(
            ConnectionOptions{}, spanner_testing::TestRetryPolicy(),
            spanner_testing::TestBackoffPolicy(),
            spanner_testing::TestPollingPolicy())) {}

 protected:
  void SetUp() override {
    emulator_ =
        google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    auto const run_slow_integration_tests =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS")
            .value_or("");
    run_slow_backup_tests_ =
        run_slow_integration_tests.find("backup") != std::string::npos;
  }
  InstanceAdminClient instance_admin_client_;
  DatabaseAdminClient database_admin_client_;
  bool emulator_ = false;
  std::string project_id_;
  bool run_slow_backup_tests_ = false;
};

class BackupTestWithCleanup : public BackupTest {
 protected:
  void SetUp() override {
    BackupTest::SetUp();
    instance_name_regex_ = std::regex(
        R"(projects/.+/instances/(temporary-instance-(\d{4}-\d{2}-\d{2})-.+))");
    instance_config_regex_ = std::regex(".*us-west.*");
    if (!run_slow_backup_tests_) {
      return;
    }
    auto s = spanner_testing::CleanupStaleInstances(project_id_,
                                                    instance_name_regex_);
    EXPECT_STATUS_OK(s) << s.message();
  }
  std::regex instance_name_regex_;
  std::regex instance_config_regex_;
};

/// @test Backup related integration tests.
TEST_F(BackupTestWithCleanup, BackupTestSuite) {
  if (!run_slow_backup_tests_ || emulator_) {
    GTEST_SKIP();
  }
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string instance_id =
      google::cloud::spanner_testing::RandomInstanceName(generator);
  std::string database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);
  std::string restore_database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);
  ASSERT_FALSE(project_id_.empty());
  ASSERT_FALSE(instance_id.empty());
  Instance in(project_id_, instance_id);

  // Make sure we're using correct regex.
  std::smatch m;
  auto full_name = in.FullName();
  EXPECT_TRUE(std::regex_match(full_name, m, instance_name_regex_));
  std::string instance_config = spanner_testing::PickInstanceConfig(
      project_id_, instance_config_regex_, generator);
  ASSERT_FALSE(instance_config.empty()) << "could not get an instance config";
  future<StatusOr<google::spanner::admin::instance::v1::Instance>> f =
      instance_admin_client_.CreateInstance(
          CreateInstanceRequestBuilder(in, instance_config)
              .SetDisplayName("test-display-name")
              .SetNodeCount(1)
              .Build());
  auto instance = f.get();

  EXPECT_STATUS_OK(instance);

  Database db(project_id_, instance_id, database_id);

  auto database_future = database_admin_client_.CreateDatabase(db);
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  auto backup_future = database_admin_client_.CreateBackup(
      db, database_id,
      std::chrono::system_clock::now() + std::chrono::hours(7));

  // Cancel the CreateBackup operation.
  backup_future.cancel();
  auto cancelled_backup = backup_future.get();
  if (cancelled_backup) {
    auto delete_result =
        database_admin_client_.DeleteBackup(cancelled_backup.value());
    EXPECT_STATUS_OK(delete_result);
  }

  // Then create a Backup without cancelling
  backup_future = database_admin_client_.CreateBackup(
      db, database_id,
      std::chrono::system_clock::now() + std::chrono::hours(7));

  // List the backup operations
  auto filter = std::string("(metadata.database:") + database_id + ") AND " +
                "(metadata.@type:type.googleapis.com/" +
                "google.spanner.admin.database.v1.CreateBackupMetadata)";

  std::vector<std::string> db_names;
  for (auto const& operation :
       database_admin_client_.ListBackupOperations(in, filter)) {
    if (!operation) break;
    google::spanner::admin::database::v1::CreateBackupMetadata metadata;
    operation->metadata().UnpackTo(&metadata);
    db_names.push_back(metadata.database());
  }
  EXPECT_LE(1, std::count(db_names.begin(), db_names.end(), db.FullName()))
      << "Database " << database_id
      << " not found in the backup operation list.";

  auto backup = backup_future.get();
  EXPECT_STATUS_OK(backup);

  google::cloud::spanner::Backup backup_name = google::cloud::spanner::Backup(
      google::cloud::spanner::Instance(project_id_, instance_id), database_id);
  auto backup_get = database_admin_client_.GetBackup(backup_name);
  EXPECT_STATUS_OK(backup_get);
  EXPECT_EQ(backup_get->name(), backup->name());
  // RestoreDatabase
  Database restore_db(project_id_, instance_id, restore_database_id);
  auto r_future =
      database_admin_client_.RestoreDatabase(restore_db, backup_name);
  auto restored_database = r_future.get();
  EXPECT_STATUS_OK(restored_database);

  // List the database operations
  filter = std::string(
      "(metadata.@type:type.googleapis.com/"
      "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)");
  std::vector<std::string> restored_db_names;
  for (auto const& operation :
       database_admin_client_.ListDatabaseOperations(in, filter)) {
    if (!operation) break;
    google::spanner::admin::database::v1::OptimizeRestoredDatabaseMetadata md;
    operation->metadata().UnpackTo(&md);
    restored_db_names.push_back(md.name());
  }
  EXPECT_LE(1, std::count(restored_db_names.begin(), restored_db_names.end(),
                          restored_database->name()))
      << "Backup " << restored_database->name()
      << " not found in the OptimizeRestoredDatabase operation list.";
  auto drop_restored_db_status =
      database_admin_client_.DropDatabase(restore_db);
  EXPECT_STATUS_OK(drop_restored_db_status);
  filter = std::string("expire_time < \"3000-01-01T00:00:00Z\"");
  std::vector<std::string> backup_names;
  for (auto const& b : database_admin_client_.ListBackups(in, filter)) {
    if (!backup) break;
    backup_names.push_back(b->name());
  }
  EXPECT_LE(
      1, std::count(backup_names.begin(), backup_names.end(), backup->name()))
      << "Backup " << backup->name() << " not found in the backup list.";
  auto new_expire_time =
      std::chrono::system_clock::now() + std::chrono::hours(7);
  auto updated_backup = database_admin_client_.UpdateBackupExpireTime(
      backup.value(), new_expire_time);
  EXPECT_STATUS_OK(updated_backup);
  auto expected_timestamp =
      google::cloud::internal::ToProtoTimestamp(new_expire_time);
  EXPECT_EQ(expected_timestamp.seconds(),
            updated_backup->expire_time().seconds());
  // The server only preserves micros.
  EXPECT_EQ(expected_timestamp.nanos() / 1000,
            updated_backup->expire_time().nanos() / 1000);
  auto delete_result = database_admin_client_.DeleteBackup(backup.value());
  EXPECT_STATUS_OK(delete_result);
  auto status = instance_admin_client_.DeleteInstance(in);
  EXPECT_STATUS_OK(status);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
