// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(#7356): Remove this file after the deprecation period expires
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/instance_location.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <chrono>
#include <regex>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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

class BackupIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 public:
  BackupIntegrationTest()
      : generator_(google::cloud::internal::MakeDefaultPRNG()),
        database_admin_client_(MakeDatabaseAdminConnection(
            ConnectionOptions{},
            LimitedTimeRetryPolicy(std::chrono::minutes(60)).clone(),
            ExponentialBackoffPolicy(std::chrono::seconds(1),
                                     std::chrono::minutes(1), 2.0)
                .clone(),
            GenericPollingPolicy<>(
                LimitedTimeRetryPolicy(std::chrono::minutes(90)),
                ExponentialBackoffPolicy(std::chrono::seconds(1),
                                         std::chrono::minutes(1), 2.0))
                .clone())) {}

 protected:
  google::cloud::internal::DefaultPRNG generator_;
  DatabaseAdminClient database_admin_client_;
};

/// @test Backup related integration tests.
TEST_F(BackupIntegrationTest, BackupRestore) {
  if (!RunSlowBackupTests() || Emulator()) GTEST_SKIP();

  auto instance_id = spanner_testing::PickRandomInstance(
      generator_, ProjectId(),
      "(labels.restore-database-partition:legacy-core OR"
      " labels.restore-database-partition:all)");
  ASSERT_STATUS_OK(instance_id);
  Instance in(ProjectId(), *instance_id);
  Database db(in, spanner_testing::RandomDatabaseName(generator_));

  auto database = database_admin_client_.CreateDatabase(db).get();
  ASSERT_STATUS_OK(database);
  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();

  auto expire_time = MakeTimestamp(create_time + absl::Hours(12)).value();
  auto backup_future =
      database_admin_client_.CreateBackup(db, db.database_id(), expire_time);

  // Cancel the CreateBackup operation.
  backup_future.cancel();
  auto cancelled_backup = backup_future.get();
  if (cancelled_backup) {
    EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*cancelled_backup));
  }

  // Then create a Backup without cancelling
  backup_future =
      database_admin_client_.CreateBackup(db, db.database_id(), expire_time);

  // List the backup operations
  std::ostringstream backup_op_filter;
  backup_op_filter << "(metadata.@type=type.googleapis.com/"
                   << "google.spanner.admin.database.v1.CreateBackupMetadata)"
                   << " AND (metadata.database=" << db.FullName() << ")";
  std::vector<std::string> db_names;
  for (auto const& operation : database_admin_client_.ListBackupOperations(
           in, backup_op_filter.str())) {
    if (!operation) break;
    google::spanner::admin::database::v1::CreateBackupMetadata metadata;
    operation->metadata().UnpackTo(&metadata);
    db_names.push_back(metadata.database());
  }
  EXPECT_LE(1, std::count(db_names.begin(), db_names.end(), database->name()))
      << "Database " << database->name()
      << " not found in the backup operation list.";

  auto backup = backup_future.get();
  {
    // TODO(#8616): Remove this when we know how to deal with the issue.
    auto matcher = testing_util::StatusIs(
        StatusCode::kDeadlineExceeded,
        testing::HasSubstr("terminated by polling policy"));
    testing::StringMatchResultListener listener;
    if (matcher.impl().MatchAndExplain(backup, &listener)) {
      // The backup is still in progress (and may eventually complete),
      // and we can't drop the database while it has pending backups, so
      // we simply abandon them, to be cleaned up offline.
      GTEST_SKIP();
    }
  }
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ(MakeTimestamp(backup->expire_time()).value(), expire_time);
  // Verify that the version_time is the same as the creation_time.
  EXPECT_EQ(MakeTimestamp(backup->version_time()).value(),
            MakeTimestamp(backup->create_time()).value());

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));

  Backup backup_name(in, db.database_id());
  auto backup_get = database_admin_client_.GetBackup(backup_name);
  EXPECT_STATUS_OK(backup_get);
  if (backup_get) {
    EXPECT_EQ(backup_get->name(), backup->name());
  }

  Database restore_db(in, spanner_testing::RandomDatabaseName(generator_));
  auto restored_database =
      database_admin_client_.RestoreDatabase(restore_db, backup_name).get();
  EXPECT_STATUS_OK(restored_database);
  if (restored_database) {
    // List the database operations
    std::ostringstream db_op_filter;
    db_op_filter
        << "(metadata.@type:type.googleapis.com/"
        << "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)";
    std::vector<std::string> restored_db_names;
    for (auto const& operation : database_admin_client_.ListDatabaseOperations(
             in, db_op_filter.str())) {
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
  }

  std::ostringstream backup_filter;
  backup_filter << "expire_time <= \"" << expire_time << "\"";
  std::vector<std::string> backup_names;
  for (auto const& b :
       database_admin_client_.ListBackups(in, backup_filter.str())) {
    backup_names.push_back(b->name());
  }
  EXPECT_LE(
      1, std::count(backup_names.begin(), backup_names.end(), backup->name()))
      << "Backup " << backup->name() << " not found in the backup list.";

  auto new_expire_time = MakeTimestamp(create_time + absl::Hours(16)).value();
  auto updated_backup =
      database_admin_client_.UpdateBackupExpireTime(*backup, new_expire_time);
  EXPECT_STATUS_OK(updated_backup);
  if (updated_backup) {
    EXPECT_EQ(MakeTimestamp(updated_backup->expire_time()).value(),
              new_expire_time);
  }

  EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
