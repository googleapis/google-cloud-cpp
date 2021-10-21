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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/instance_location.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/policies.h"
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
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

// Constants used to identify the encryption key.
auto constexpr kKeyRing = "spanner-cmek";
auto constexpr kKeyName = "spanner-cmek-test-key";

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

class BackupTest : public ::google::cloud::testing_util::IntegrationTest {
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
  backup_op_filter << "(metadata.database:" << db.database_id() << ") AND "
                   << "(metadata.@type:type.googleapis.com/"
                   << "google.spanner.admin.database.v1.CreateBackupMetadata)";
  std::vector<std::string> db_names;
  for (auto const& operation : database_admin_client_.ListBackupOperations(
           in, backup_op_filter.str())) {
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

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));

  Backup backup_name(in, db.database_id());
  auto backup_get = database_admin_client_.GetBackup(backup_name);
  EXPECT_STATUS_OK(backup_get);
  EXPECT_EQ(backup_get->name(), backup->name());

  Database restore_db(in, spanner_testing::RandomDatabaseName(generator_));
  auto restored_database =
      database_admin_client_.RestoreDatabase(restore_db, backup_name).get();
  EXPECT_STATUS_OK(restored_database);

  // List the database operations
  std::ostringstream db_op_filter;
  db_op_filter
      << "(metadata.@type:type.googleapis.com/"
      << "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)";
  std::vector<std::string> restored_db_names;
  for (auto const& operation :
       database_admin_client_.ListDatabaseOperations(in, db_op_filter.str())) {
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

  std::ostringstream backup_filter;
  backup_filter << "expire_time <= \"" << expire_time << "\"";
  std::vector<std::string> backup_names;
  for (auto const& b :
       database_admin_client_.ListBackups(in, backup_filter.str())) {
    if (!backup) break;
    backup_names.push_back(b->name());
  }
  EXPECT_LE(
      1, std::count(backup_names.begin(), backup_names.end(), backup->name()))
      << "Backup " << backup->name() << " not found in the backup list.";

  auto new_expire_time = MakeTimestamp(create_time + absl::Hours(8)).value();
  auto updated_backup =
      database_admin_client_.UpdateBackupExpireTime(*backup, new_expire_time);
  EXPECT_STATUS_OK(updated_backup);
  EXPECT_EQ(MakeTimestamp(updated_backup->expire_time()).value(),
            new_expire_time);

  EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
}

/// @test Verify creating/restoring a backup with a valid version_time.
TEST_F(BackupTest, CreateBackupWithVersionTime) {
  if (!RunSlowBackupTests()) GTEST_SKIP();

  auto instance_id =
      spanner_testing::PickRandomInstance(generator_, ProjectId());
  ASSERT_THAT(instance_id, IsOk()) << instance_id.status();
  Instance in(ProjectId(), *instance_id);
  Database db(in, spanner_testing::RandomDatabaseName(generator_));

  std::vector<std::string> extra_statements = {
      absl::StrCat("ALTER DATABASE `", db.database_id(),
                   "` SET OPTIONS (version_retention_period='1h')"),
      "CREATE TABLE Counters ("
      "  Name   STRING(64) NOT NULL,"
      "  Value  INT64 NOT NULL"
      ") PRIMARY KEY (Name)"};
  auto database =
      database_admin_client_.CreateDatabase(db, extra_statements).get();
  if (Emulator()) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_THAT(database, Not(IsOk()));
    return;
  }
  ASSERT_THAT(database, IsOk()) << database.status();
  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();

  std::string const version_key = "version";
  std::vector<Timestamp> version_times;
  {
    spanner::Client client(spanner::MakeConnection(db));
    auto commit = client.Commit(spanner::Mutations{
        spanner::InsertMutationBuilder("Counters", {"Name", "Value"})
            .EmplaceRow(version_key, 0)  // the version we'll backup/restore
            .Build()});
    EXPECT_STATUS_OK(commit);
    if (commit) {
      // version_times[0]: when Counters[version_key] == 0
      version_times.push_back(commit->commit_timestamp);
      commit = client.Commit(spanner::Mutations{
          spanner::UpdateMutationBuilder("Counters", {"Name", "Value"})
              .EmplaceRow(version_key, 1)  // latest version
              .Build()});
      EXPECT_STATUS_OK(commit);
      if (commit) {
        // version_times[1]: when Counters[version_key] == 1
        version_times.push_back(commit->commit_timestamp);
      }
    }
  }

  if (version_times.size() == 2) {
    EXPECT_LT(MakeTimestamp(create_time).value(), version_times[0]);
    EXPECT_LT(version_times[0], version_times[1]);
    // Create a backup when Counters[version_key] == 0.
    auto version_time = version_times[0];
    auto expire_time = MakeTimestamp(create_time + absl::Hours(8)).value();
    auto backup =
        database_admin_client_
            .CreateBackup(db, db.database_id(), expire_time, version_time)
            .get();
    EXPECT_THAT(backup, IsOk());
    if (backup) {
      EXPECT_EQ(MakeTimestamp(backup->expire_time()).value(), expire_time);
      EXPECT_EQ(MakeTimestamp(backup->version_time()).value(), version_time);
      EXPECT_GT(MakeTimestamp(backup->create_time()).value(), version_time);

      Database rdb(in, spanner_testing::RandomDatabaseName(generator_));
      auto restored =
          database_admin_client_.RestoreDatabase(rdb, *backup).get();
      EXPECT_THAT(restored, IsOk());
      if (restored) {
        auto const& restore_info = restored->restore_info();
        EXPECT_EQ(restore_info.source_type(),
                  google::spanner::admin::database::v1::BACKUP);
        if (restore_info.source_type() ==
            google::spanner::admin::database::v1::BACKUP) {
          auto const& backup_info = restore_info.backup_info();
          EXPECT_EQ(backup_info.backup(), backup->name());
          EXPECT_EQ(MakeTimestamp(backup_info.version_time()).value(),
                    version_time);
          EXPECT_LT(MakeTimestamp(backup_info.version_time()).value(),
                    MakeTimestamp(backup_info.create_time()).value());
          EXPECT_EQ(backup_info.source_database(), db.FullName());
        }
        auto database = database_admin_client_.GetDatabase(rdb);
        EXPECT_THAT(database, IsOk());
        if (database) {
          auto const& restore_info = database->restore_info();
          EXPECT_EQ(restore_info.source_type(),
                    google::spanner::admin::database::v1::BACKUP);
          if (restore_info.source_type() ==
              google::spanner::admin::database::v1::BACKUP) {
            auto const& backup_info = restore_info.backup_info();
            EXPECT_EQ(MakeTimestamp(backup_info.version_time()).value(),
                      version_time);
          }
        }
        bool found_restored = false;
        for (auto const& database : database_admin_client_.ListDatabases(in)) {
          EXPECT_THAT(database, IsOk());
          if (!database) continue;
          if (database->name() == rdb.FullName()) {
            EXPECT_FALSE(found_restored);
            found_restored = true;
            auto const& restore_info = database->restore_info();
            EXPECT_EQ(restore_info.source_type(),
                      google::spanner::admin::database::v1::BACKUP);
            if (restore_info.source_type() ==
                google::spanner::admin::database::v1::BACKUP) {
              auto const& backup_info = restore_info.backup_info();
              EXPECT_EQ(MakeTimestamp(backup_info.version_time()).value(),
                        version_time);
            }
          }
        }
        EXPECT_TRUE(found_restored);
        {
          spanner::Client client(spanner::MakeConnection(rdb));
          auto keys = spanner::KeySet().AddKey(spanner::MakeKey(version_key));
          auto rows = client.Read("Counters", std::move(keys), {"Value"});
          using RowType = std::tuple<std::int64_t>;
          auto row = spanner::GetSingularRow(spanner::StreamOf<RowType>(rows));
          EXPECT_THAT(row, IsOk());
          if (row) {
            // Expect to see the state of the table at version_time.
            EXPECT_EQ(std::get<0>(*std::move(row)), 0);
          }
        }
        EXPECT_STATUS_OK(database_admin_client_.DropDatabase(rdb));
      }

      EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
    }
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));
}

/// @test Verify creating a backup with an expired version_time fails.
TEST_F(BackupTest, CreateBackupWithExpiredVersionTime) {
  auto instance_id =
      spanner_testing::PickRandomInstance(generator_, ProjectId());
  ASSERT_THAT(instance_id, IsOk()) << instance_id.status();
  Instance in(ProjectId(), *instance_id);
  Database db(in, spanner_testing::RandomDatabaseName(generator_));

  std::vector<std::string> extra_statements = {
      absl::StrCat("ALTER DATABASE `", db.database_id(),
                   "` SET OPTIONS (version_retention_period='1h')")};
  auto database =
      database_admin_client_.CreateDatabase(db, extra_statements).get();
  if (Emulator()) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_THAT(database, Not(IsOk()));
    return;
  }
  ASSERT_THAT(database, IsOk()) << database.status();

  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();
  // version_time too far in the past (outside the version_retention_period).
  auto version_time = MakeTimestamp(create_time - absl::Hours(2)).value();
  auto expire_time = MakeTimestamp(create_time + absl::Hours(8)).value();
  auto backup =
      database_admin_client_
          .CreateBackup(db, db.database_id(), expire_time, version_time)
          .get();
  EXPECT_THAT(backup, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("earlier than the creation time")));
  if (backup) {
    EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));
}

/// @test Verify creating a backup with a future version_time fails.
TEST_F(BackupTest, CreateBackupWithFutureVersionTime) {
  auto instance_id =
      spanner_testing::PickRandomInstance(generator_, ProjectId());
  ASSERT_THAT(instance_id, IsOk()) << instance_id.status();
  Instance in(ProjectId(), *instance_id);
  Database db(in, spanner_testing::RandomDatabaseName(generator_));

  std::vector<std::string> extra_statements = {
      absl::StrCat("ALTER DATABASE `", db.database_id(),
                   "` SET OPTIONS (version_retention_period='1h')")};
  auto database =
      database_admin_client_.CreateDatabase(db, extra_statements).get();
  if (Emulator()) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_THAT(database, Not(IsOk()));
    return;
  }
  ASSERT_THAT(database, IsOk()) << database.status();

  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();
  // version_time in the future.
  auto version_time = MakeTimestamp(create_time + absl::Hours(2)).value();
  auto expire_time = MakeTimestamp(create_time + absl::Hours(8)).value();
  auto backup =
      database_admin_client_
          .CreateBackup(db, db.database_id(), expire_time, version_time)
          .get();
  EXPECT_THAT(backup, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("with a future version time")));
  if (backup) {
    EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));
}

/// @test Tests backup/restore with Customer Managed Encryption Key
TEST_F(BackupTest, BackupTestWithCMEK) {
  if (!RunSlowBackupTests() || Emulator()) GTEST_SKIP();

  auto instance_id =
      spanner_testing::PickRandomInstance(generator_, ProjectId());
  ASSERT_STATUS_OK(instance_id);
  Instance in(ProjectId(), *instance_id);

  auto location = spanner_testing::InstanceLocation(in);
  ASSERT_STATUS_OK(location);
  KmsKeyName encryption_key(in.project_id(), *location, kKeyRing, kKeyName);
  auto encryption_config = CustomerManagedEncryption(encryption_key);

  Database db(in, spanner_testing::RandomDatabaseName(generator_));
  auto database =
      database_admin_client_
          .CreateDatabase(db, /*extra_statements=*/{}, encryption_config)
          .get();
  ASSERT_STATUS_OK(database);
  EXPECT_TRUE(database->has_encryption_config());
  if (database->has_encryption_config()) {
    EXPECT_EQ(database->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }
  EXPECT_THAT(database->encryption_info(), IsEmpty());

  auto database_get = database_admin_client_.GetDatabase(db);
  ASSERT_STATUS_OK(database_get);
  EXPECT_EQ(database_get->name(), database->name());
  EXPECT_TRUE(database_get->has_encryption_config());
  if (database_get->has_encryption_config()) {
    EXPECT_EQ(database_get->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }

  auto create_time =
      MakeTimestamp(database->create_time()).value().get<absl::Time>().value();
  auto expire_time = MakeTimestamp(create_time + absl::Hours(7)).value();
  auto backup = database_admin_client_
                    .CreateBackup(db, db.database_id(), expire_time,
                                  absl::nullopt, encryption_config)
                    .get();
  ASSERT_STATUS_OK(backup);
  EXPECT_TRUE(backup->has_encryption_info());
  if (backup->has_encryption_info()) {
    EXPECT_EQ(backup->encryption_info().encryption_type(),
              google::spanner::admin::database::v1::EncryptionInfo::
                  CUSTOMER_MANAGED_ENCRYPTION);
    EXPECT_THAT(backup->encryption_info().kms_key_version(),
                HasSubstr(encryption_key.FullName() + "/cryptoKeyVersions/"));
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(db));

  Backup backup_name(in, db.database_id());
  auto backup_get = database_admin_client_.GetBackup(backup_name);
  ASSERT_STATUS_OK(backup_get);
  EXPECT_EQ(backup_get->name(), backup->name());
  EXPECT_TRUE(backup_get->has_encryption_info());
  if (backup_get->has_encryption_info()) {
    EXPECT_EQ(backup_get->encryption_info().encryption_type(),
              google::spanner::admin::database::v1::EncryptionInfo::
                  CUSTOMER_MANAGED_ENCRYPTION);
    EXPECT_THAT(backup_get->encryption_info().kms_key_version(),
                HasSubstr(encryption_key.FullName() + "/cryptoKeyVersions/"));
  }

  Database restore_db(in, spanner_testing::RandomDatabaseName(generator_));
  auto restored_database =
      database_admin_client_
          .RestoreDatabase(restore_db, backup_name, encryption_config)
          .get();
  ASSERT_STATUS_OK(restored_database);
  EXPECT_TRUE(restored_database->has_encryption_config());
  if (restored_database->has_encryption_config()) {
    EXPECT_EQ(restored_database->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }

  auto restored_get = database_admin_client_.GetDatabase(restore_db);
  ASSERT_STATUS_OK(restored_get);
  EXPECT_EQ(restored_get->name(), restored_database->name());
  EXPECT_TRUE(restored_get->has_encryption_config());
  if (restored_get->has_encryption_config()) {
    EXPECT_EQ(restored_get->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }

  EXPECT_STATUS_OK(database_admin_client_.DropDatabase(restore_db));

  std::ostringstream backup_filter;
  backup_filter << "expire_time <= \"" << expire_time << "\"";
  bool found = false;
  for (auto const& b :
       database_admin_client_.ListBackups(in, backup_filter.str())) {
    if (b->name() == backup->name()) {
      found = true;
      EXPECT_TRUE(b->has_encryption_info());
      if (b->has_encryption_info()) {
        EXPECT_EQ(b->encryption_info().encryption_type(),
                  google::spanner::admin::database::v1::EncryptionInfo::
                      CUSTOMER_MANAGED_ENCRYPTION);
        EXPECT_THAT(
            b->encryption_info().kms_key_version(),
            HasSubstr(encryption_key.FullName() + "/cryptoKeyVersions/"));
      }
    }
  }
  EXPECT_TRUE(found);

  EXPECT_STATUS_OK(database_admin_client_.DeleteBackup(*backup));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
