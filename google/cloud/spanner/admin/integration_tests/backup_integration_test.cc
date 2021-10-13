// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/database_admin_options.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/backup.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/testing/instance_location.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/kms_key_name.h"
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
        database_admin_client_(spanner_admin::MakeDatabaseAdminConnection(
            Options{}
                .set<spanner_admin::DatabaseAdminRetryPolicyOption>(
                    spanner_admin::DatabaseAdminLimitedTimeRetryPolicy(
                        std::chrono::minutes(60))
                        .clone())
                .set<spanner_admin::DatabaseAdminBackoffPolicyOption>(
                    ExponentialBackoffPolicy(std::chrono::seconds(1),
                                             std::chrono::minutes(1), 2.0)
                        .clone())
                .set<spanner_admin::DatabaseAdminPollingPolicyOption>(
                    GenericPollingPolicy<>(
                        LimitedTimeRetryPolicy(std::chrono::hours(3)),
                        ExponentialBackoffPolicy(std::chrono::seconds(1),
                                                 std::chrono::minutes(1), 2.0))
                        .clone()))) {}

 protected:
  google::cloud::internal::DefaultPRNG generator_;
  spanner_admin::DatabaseAdminClient database_admin_client_;
};

/// @test Backup related integration tests.
TEST_F(BackupIntegrationTest, BackupRestore) {
  if (!RunSlowBackupTests() || Emulator()) GTEST_SKIP();

  auto instance_id = spanner_testing::PickRandomInstance(
      generator_, ProjectId(),
      "labels.restore-database-partition:generated-core");
  ASSERT_STATUS_OK(instance_id);
  Instance in(ProjectId(), *instance_id);
  Database db(in, spanner_testing::RandomDatabaseName(generator_));

  auto database = database_admin_client_
                      .CreateDatabase(db.instance().FullName(),
                                      absl::StrCat("CREATE DATABASE `",
                                                   db.database_id(), "`"))
                      .get();
  ASSERT_STATUS_OK(database);
  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();

  auto expire_time = MakeTimestamp(create_time + absl::Hours(12)).value();
  google::spanner::admin::database::v1::CreateBackupRequest breq;
  breq.set_parent(db.instance().FullName());
  breq.set_backup_id(db.database_id());
  breq.mutable_backup()->set_database(db.FullName());
  *breq.mutable_backup()->mutable_expire_time() =
      expire_time.get<protobuf::Timestamp>().value();
  auto backup_future = database_admin_client_.CreateBackup(breq);

  // Cancel the CreateBackup operation.
  backup_future.cancel();
  auto cancelled_backup = backup_future.get();
  if (cancelled_backup) {
    EXPECT_STATUS_OK(
        database_admin_client_.DeleteBackup(cancelled_backup->name()));
  }

  // Then create a Backup without cancelling.
  backup_future = database_admin_client_.CreateBackup(breq);

  // List the backup operations
  std::ostringstream backup_op_filter;
  backup_op_filter << "(metadata.database:" << db.database_id() << ") AND "
                   << "(metadata.@type:type.googleapis.com/"
                   << "google.spanner.admin.database.v1.CreateBackupMetadata)";
  google::spanner::admin::database::v1::ListBackupOperationsRequest lreq;
  lreq.set_parent(in.FullName());
  lreq.set_filter(backup_op_filter.str());
  std::vector<std::string> db_names;
  for (auto const& operation :
       database_admin_client_.ListBackupOperations(lreq)) {
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
    // Verify that the version_time is the same as the creation_time.
    EXPECT_EQ(MakeTimestamp(backup->version_time()).value(),
              MakeTimestamp(backup->create_time()).value());
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db.FullName()));

  Backup backup_name(in, db.database_id());
  auto backup_get = database_admin_client_.GetBackup(backup_name.FullName());
  EXPECT_STATUS_OK(backup_get);
  EXPECT_EQ(backup_get->name(), backup->name());

  Database restore_db(in, spanner_testing::RandomDatabaseName(generator_));
  auto restored_database =
      database_admin_client_
          .RestoreDatabase(restore_db.instance().FullName(),
                           restore_db.database_id(), backup_name.FullName())
          .get();
  EXPECT_STATUS_OK(restored_database);

  // List the database operations
  std::ostringstream db_op_filter;
  db_op_filter
      << "(metadata.@type:type.googleapis.com/"
      << "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)";
  google::spanner::admin::database::v1::ListDatabaseOperationsRequest dreq;
  dreq.set_parent(in.FullName());
  dreq.set_filter(db_op_filter.str());
  std::vector<std::string> restored_db_names;
  for (auto const& operation :
       database_admin_client_.ListDatabaseOperations(dreq)) {
    if (!operation) break;
    google::spanner::admin::database::v1::OptimizeRestoredDatabaseMetadata md;
    operation->metadata().UnpackTo(&md);
    restored_db_names.push_back(md.name());
  }
  EXPECT_LE(1, std::count(restored_db_names.begin(), restored_db_names.end(),
                          restored_database->name()))
      << "Backup " << restored_database->name()
      << " not found in the OptimizeRestoredDatabase operation list.";

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(restore_db.FullName()));

  std::ostringstream backup_filter;
  backup_filter << "expire_time <= \"" << expire_time << "\"";
  google::spanner::admin::database::v1::ListBackupsRequest req;
  req.set_parent(in.FullName());
  req.set_filter(backup_filter.str());
  std::vector<std::string> backup_names;
  for (auto const& b : database_admin_client_.ListBackups(req)) {
    if (!backup) break;
    backup_names.push_back(b->name());
  }
  EXPECT_LE(
      1, std::count(backup_names.begin(), backup_names.end(), backup->name()))
      << "Backup " << backup->name() << " not found in the backup list.";

  auto new_expire_time = MakeTimestamp(create_time + absl::Hours(16)).value();
  google::spanner::admin::database::v1::UpdateBackupRequest ureq;
  ureq.mutable_backup()->set_name(backup->name());
  *ureq.mutable_backup()->mutable_expire_time() =
      new_expire_time.get<protobuf::Timestamp>().value();
  ureq.mutable_update_mask()->add_paths("expire_time");
  auto updated_backup = database_admin_client_.UpdateBackup(ureq);
  EXPECT_STATUS_OK(updated_backup);
  EXPECT_EQ(MakeTimestamp(updated_backup->expire_time()).value(),
            new_expire_time);

  EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(backup->name()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
