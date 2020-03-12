// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/internal/time_utils.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::spanner_testing::IsProtoEqual;
using ::testing::EndsWith;

/// @test Verify the basic CRUD operations for databases work.
TEST(DatabaseAdminClient, DatabaseBasicCRUD) {
  auto emulator =
      google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto instance_id = spanner_testing::PickRandomInstance(generator, project_id);
  ASSERT_STATUS_OK(instance_id);

  auto test_iam_service_account =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_IAM_TEST_SA")
          .value_or("");
  ASSERT_TRUE(emulator || !test_iam_service_account.empty());

  auto run_slow_integration_tests =
      google::cloud::internal::GetEnv("RUN_SLOW_INTEGRATION_TESTS")
          .value_or("");
  Instance const in(project_id, *instance_id);

  std::string database_id = spanner_testing::RandomDatabaseName(generator);

  DatabaseAdminClient client;

  // We test client.ListDatabases() by verifying that (a) it does not return a
  // randomly generated database name before we create a database with that
  // name, (b) it *does* return that database name once created, and (c) it no
  // longer returns that name once the database is dropped. Implicitly that also
  // tests that client.DropDatabase() and client.CreateDatabase() do something,
  // which is nice.
  auto get_current_databases = [&client, in] {
    std::vector<std::string> names;
    for (auto const& database : client.ListDatabases(in)) {
      EXPECT_STATUS_OK(database);
      if (!database) return names;
      names.push_back(database->name());
    }
    return names;
  };

  Database db(project_id, *instance_id, database_id);

  auto db_list = get_current_databases();
  ASSERT_EQ(0, std::count(db_list.begin(), db_list.end(), db.FullName()))
      << "Database " << database_id << " already exists, this is unexpected"
      << " as the database id is selected at random.";

  auto database_future = client.CreateDatabase(db);
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  EXPECT_THAT(database->name(), EndsWith(database_id));

  auto get_result = client.GetDatabase(db);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(database->name(), get_result->name());

  if (!emulator) {
    auto current_policy = client.GetIamPolicy(db);
    ASSERT_STATUS_OK(current_policy);
    EXPECT_EQ(0, current_policy->bindings_size());

    std::string const reader_role = "roles/spanner.databaseReader";
    std::string const writer_role = "roles/spanner.databaseUser";
    std::string const expected_member =
        "serviceAccount:" + test_iam_service_account;
    auto& binding = *current_policy->add_bindings();
    binding.set_role(reader_role);
    *binding.add_members() = expected_member;

    auto updated_policy = client.SetIamPolicy(db, *current_policy);
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_EQ(1, updated_policy->bindings_size());
    ASSERT_EQ(reader_role, updated_policy->bindings().Get(0).role());
    ASSERT_EQ(1, updated_policy->bindings().Get(0).members().size());
    ASSERT_EQ(expected_member,
              updated_policy->bindings().Get(0).members().Get(0));

    // Perform a different update using the the OCC loop API:
    updated_policy =
        client.SetIamPolicy(db, [&test_iam_service_account, &writer_role](
                                    google::iam::v1::Policy current) {
          std::string const expected_member =
              "serviceAccount:" + test_iam_service_account;
          auto& binding = *current.add_bindings();
          binding.set_role(writer_role);
          *binding.add_members() = expected_member;
          return current;
        });
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_EQ(2, updated_policy->bindings_size());
    ASSERT_EQ(writer_role, updated_policy->bindings().Get(1).role());
    ASSERT_EQ(1, updated_policy->bindings().Get(1).members().size());
    ASSERT_EQ(expected_member,
              updated_policy->bindings().Get(1).members().Get(0));

    // Fetch the Iam Policy again.
    current_policy = client.GetIamPolicy(db);
    ASSERT_STATUS_OK(current_policy);
    EXPECT_THAT(*updated_policy, IsProtoEqual(*current_policy));

    auto test_iam_permission_result =
        client.TestIamPermissions(db, {"spanner.databases.read"});
    ASSERT_STATUS_OK(test_iam_permission_result);
    ASSERT_EQ(1, test_iam_permission_result->permissions_size());
    ASSERT_EQ("spanner.databases.read",
              test_iam_permission_result->permissions(0));
  }

  auto get_ddl_result = client.GetDatabaseDdl(db);
  ASSERT_STATUS_OK(get_ddl_result);
  EXPECT_EQ(0, get_ddl_result->statements_size());

  auto const create_table_statement = R"""(
                             CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024),
                                SingerInfo BYTES(MAX)
                             ) PRIMARY KEY (SingerId)
                            )""";

  auto update_future = client.UpdateDatabase(db, {create_table_statement});
  auto metadata = update_future.get();
  ASSERT_STATUS_OK(metadata);
  EXPECT_THAT(metadata->database(), EndsWith(database_id));
  EXPECT_EQ(1, metadata->statements_size());
  EXPECT_EQ(1, metadata->commit_timestamps_size());
  if (metadata->statements_size() > 1) {
    EXPECT_EQ(create_table_statement, metadata->statements(0));
  }

  db_list = get_current_databases();
  ASSERT_EQ(1, std::count(db_list.begin(), db_list.end(), db.FullName()));

  // Tests for Backup are taking long time. To run them, set
  // RUN_SLOW_INTEGRATION_TESTS environment variable to yes.
  if (run_slow_integration_tests == "yes" && !emulator) {
    auto backup_future = client.CreateBackup(
        db, database_id,
        std::chrono::system_clock::now() + std::chrono::hours(7));

    // Cancel the CreateBackup operation.
    backup_future.cancel();
    auto cancelled_backup = backup_future.get();
    if (cancelled_backup) {
      auto delete_result = client.DeleteBackup(cancelled_backup.value());
      EXPECT_STATUS_OK(delete_result);
    }

    // Then create a Backup without cancelling
    backup_future = client.CreateBackup(
        db, database_id,
        std::chrono::system_clock::now() + std::chrono::hours(7));

    // List the backup operations
    auto filter = std::string("(metadata.database:") + database_id + ") AND " +
                  "(metadata.@type:type.googleapis.com/" +
                  "google.spanner.admin.database.v1.CreateBackupMetadata)";

    std::vector<std::string> db_names;
    for (auto const& operation : client.ListBackupOperations(in, filter)) {
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
        google::cloud::spanner::Instance(project_id, *instance_id),
        database_id);
    auto backup_get = client.GetBackup(backup_name);
    EXPECT_STATUS_OK(backup_get);
    EXPECT_EQ(backup_get->name(), backup->name());
    // RestoreDatabase
    std::string restore_database_id =
        spanner_testing::RandomDatabaseName(generator);
    Database restore_db(project_id, *instance_id, restore_database_id);
    auto r_future = client.RestoreDatabase(restore_db, backup_name);
    auto restored_database = r_future.get();
    EXPECT_STATUS_OK(restored_database);

    // List the database operations
    filter = std::string(
        "(metadata.@type:type.googleapis.com/"
        "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)");
    std::vector<std::string> restored_db_names;
    for (auto const& operation : client.ListDatabaseOperations(in, filter)) {
      if (!operation) break;
      google::spanner::admin::database::v1::OptimizeRestoredDatabaseMetadata md;
      operation->metadata().UnpackTo(&md);
      restored_db_names.push_back(md.name());
    }
    EXPECT_LE(1, std::count(restored_db_names.begin(), restored_db_names.end(),
                            restored_database->name()))
        << "Backup " << restored_database->name()
        << " not found in the OptimizeRestoredDatabase operation list.";
    auto drop_restored_db_status = client.DropDatabase(restore_db);
    EXPECT_STATUS_OK(drop_restored_db_status);
    filter = std::string("expire_time < \"3000-01-01T00:00:00Z\"");
    std::vector<std::string> backup_names;
    for (auto const& backup : client.ListBackups(in, filter)) {
      if (!backup) break;
      backup_names.push_back(backup->name());
    }
    EXPECT_LE(
        1, std::count(backup_names.begin(), backup_names.end(), backup->name()))
        << "Backup " << backup->name() << " not found in the backup list.";
    auto new_expire_time =
        std::chrono::system_clock::now() + std::chrono::hours(7);
    auto updated_backup =
        client.UpdateBackupExpireTime(backup.value(), new_expire_time);
    EXPECT_STATUS_OK(updated_backup);
    auto expected_timestamp =
        google::cloud::spanner::internal::ConvertTimePointToProtoTimestamp(
            new_expire_time);
    EXPECT_STATUS_OK(expected_timestamp);
    EXPECT_EQ(expected_timestamp->seconds(),
              updated_backup->expire_time().seconds());
    // The server only preserves micros.
    EXPECT_EQ(expected_timestamp->nanos() / 1000,
              updated_backup->expire_time().nanos() / 1000);
    auto delete_result = client.DeleteBackup(backup.value());
    EXPECT_STATUS_OK(delete_result);
  }

  auto drop_status = client.DropDatabase(db);
  EXPECT_STATUS_OK(drop_status);

  db_list = get_current_databases();
  ASSERT_EQ(0, std::count(db_list.begin(), db_list.end(), db.FullName()));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
