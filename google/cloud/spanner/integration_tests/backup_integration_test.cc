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

#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/policies.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <chrono>
#include <regex>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

std::string const& ProjectId() {
  static std::string project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  return project_id;
}

bool RunSlowBackupTests() {
  static bool run_slow_backup_tests =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS")
          .value_or("")
          .find("backup") != std::string::npos;
  return run_slow_backup_tests;
}

bool Emulator() {
  static bool emulator =
      google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();
  return emulator;
}

class BackupTest : public ::testing::Test {
 public:
  BackupTest()
      : generator_(google::cloud::internal::MakeDefaultPRNG()),
        database_admin_client_(MakeDatabaseAdminConnection(
            ConnectionOptions{}, spanner_testing::TestRetryPolicy(),
            spanner_testing::TestBackoffPolicy(),
            spanner_testing::TestPollingPolicy())) {}

 protected:
  google::cloud::internal::DefaultPRNG generator_;
  DatabaseAdminClient database_admin_client_;
};

/// @test Backup related integration tests.
TEST_F(BackupTest, BackupTest) {
  if (!RunSlowBackupTests() || Emulator()) GTEST_SKIP();

  auto instance_id =
      spanner_testing::PickRandomInstance(generator_, ProjectId());
  ASSERT_STATUS_OK(instance_id);
  Instance in(ProjectId(), *instance_id);
  Database db(in, spanner_testing::RandomDatabaseName(generator_));

  auto database = database_admin_client_.CreateDatabase(db).get();
  ASSERT_STATUS_OK(database);
  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();

  auto expire_time = MakeTimestamp(create_time + absl::Hours(7)).value();
  auto backup_future = database_admin_client_.CreateBackup(
      db, db.database_id(),
      expire_time.get<std::chrono::system_clock::time_point>().value());

  // Cancel the CreateBackup operation.
  backup_future.cancel();
  auto cancelled_backup = backup_future.get();
  if (cancelled_backup) {
    EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*cancelled_backup));
  }

  // Then create a Backup without cancelling
  backup_future = database_admin_client_.CreateBackup(
      db, db.database_id(),
      expire_time.get<std::chrono::system_clock::time_point>().value());

  // List the backup operations
  auto filter = std::string("(metadata.database:") + db.database_id() +
                ") AND " + "(metadata.@type:type.googleapis.com/" +
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
      << "Database " << db.database_id()
      << " not found in the backup operation list.";

  auto backup = backup_future.get();
  EXPECT_STATUS_OK(backup);
  if (backup) {
    EXPECT_EQ(MakeTimestamp(backup->expire_time()).value(), expire_time);
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));

  Backup backup_name(in, db.database_id());
  auto backup_get = database_admin_client_.GetBackup(backup_name);
  EXPECT_STATUS_OK(backup_get);
  EXPECT_EQ(backup_get->name(), backup->name());

  // RestoreDatabase
  Database restore_db(in, spanner_testing::RandomDatabaseName(generator_));
  auto restored_database =
      database_admin_client_.RestoreDatabase(restore_db, backup_name).get();
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

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(restore_db));

  filter = std::string("expire_time < \"3000-01-01T00:00:00Z\"");
  std::vector<std::string> backup_names;
  for (auto const& b : database_admin_client_.ListBackups(in, filter)) {
    if (!backup) break;
    backup_names.push_back(b->name());
  }
  EXPECT_LE(
      1, std::count(backup_names.begin(), backup_names.end(), backup->name()))
      << "Backup " << backup->name() << " not found in the backup list.";

  auto new_expire_time = MakeTimestamp(create_time + absl::Hours(8)).value();
  auto updated_backup = database_admin_client_.UpdateBackupExpireTime(
      *backup,
      new_expire_time.get<std::chrono::system_clock::time_point>().value());
  EXPECT_STATUS_OK(updated_backup);
  EXPECT_EQ(MakeTimestamp(updated_backup->expire_time()).value(),
            new_expire_time);

  EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
