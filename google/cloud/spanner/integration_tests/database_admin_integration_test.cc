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
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Not;

// Constants used to identify the encryption key.
// For staging, the location must be us-central1.
auto constexpr kLocation = "us-central1";
auto constexpr kKeyRing = "spanner-cmek";
auto constexpr kKeyName = "spanner-cmek-test-key";

class DatabaseAdminClientTest : public ::testing::Test {
 protected:
  // We can't use ASSERT* in the constructor, so defer initializing `instance_`
  // and `database_` until `SetUp()`.
  DatabaseAdminClientTest()
      : instance_("dummy", "dummy"), database_(instance_, "dummy") {}

  void SetUp() override {
    using ::google::cloud::internal::GetEnv;
    emulator_ = GetEnv("SPANNER_EMULATOR_HOST").has_value();
    auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());

    auto generator = google::cloud::internal::MakeDefaultPRNG();
    auto instance_id =
        spanner_testing::PickRandomInstance(generator, project_id);
    ASSERT_STATUS_OK(instance_id);
    instance_ = Instance(project_id, *instance_id);

    database_ =
        Database(instance_, spanner_testing::RandomDatabaseName(generator));

    test_iam_service_account_ =
        GetEnv("GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT").value_or("");
    ASSERT_TRUE(emulator_ || !test_iam_service_account_.empty());
  }

  // Does `database_` exist in `instance_`?
  bool DatabaseExists() {
    auto full_name = database_.FullName();
    for (auto const& database : client_.ListDatabases(instance_)) {
      EXPECT_STATUS_OK(database);
      if (!database) break;
      if (database->name() == full_name) return true;
    }
    return false;
  };

  Instance instance_;
  Database database_;
  DatabaseAdminClient client_;
  bool emulator_;
  std::string test_iam_service_account_;
};

/// @test Verify the basic CRUD operations for databases work.
TEST_F(DatabaseAdminClientTest, DatabaseBasicCRUD) {
  // We test Client::ListDatabases() by verifying that (a) it does not return a
  // randomly generated database name before we create a database with that
  // name, (b) it *does* return that database name once created, and (c) it no
  // longer returns that name once the database is dropped. Implicitly that also
  // tests that Client::DropDatabase() and client::CreateDatabase() do
  // something, which is nice.
  EXPECT_FALSE(DatabaseExists()) << "Database " << database_
                                 << " already exists, this is unexpected as "
                                    "the database id is selected at random.";

  // Use an encryption key for this test (the emulator doesn't support it but
  // ignores it, so go ahead and set it, we just can't check that it exists).
  KmsKeyName encryption_key(instance_.project_id(), kLocation, kKeyRing,
                            kKeyName);
  auto database_future = client_.CreateDatabase(
      database_, /*extra_statements=*/{}, encryption_key);
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  EXPECT_THAT(database->name(), EndsWith(database_.database_id()));

  auto get_result = client_.GetDatabase(database_);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(database->name(), get_result->name());

  if (!emulator_) {
    auto current_policy = client_.GetIamPolicy(database_);
    ASSERT_STATUS_OK(current_policy);
    EXPECT_EQ(0, current_policy->bindings_size());

    std::string const reader_role = "roles/spanner.databaseReader";
    std::string const writer_role = "roles/spanner.databaseUser";
    std::string const expected_member =
        "serviceAccount:" + test_iam_service_account_;
    auto& binding = *current_policy->add_bindings();
    binding.set_role(reader_role);
    *binding.add_members() = expected_member;

    auto updated_policy = client_.SetIamPolicy(database_, *current_policy);
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_EQ(1, updated_policy->bindings_size());
    ASSERT_EQ(reader_role, updated_policy->bindings().Get(0).role());
    ASSERT_EQ(1, updated_policy->bindings().Get(0).members().size());
    ASSERT_EQ(expected_member,
              updated_policy->bindings().Get(0).members().Get(0));

    // Perform a different update using the the OCC loop API:
    updated_policy = client_.SetIamPolicy(
        database_, [this, &writer_role](google::iam::v1::Policy current) {
          std::string const expected_member =
              "serviceAccount:" + test_iam_service_account_;
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
    current_policy = client_.GetIamPolicy(database_);
    ASSERT_STATUS_OK(current_policy);
    EXPECT_THAT(*updated_policy, IsProtoEqual(*current_policy));

    auto test_iam_permission_result =
        client_.TestIamPermissions(database_, {"spanner.databases.read"});
    ASSERT_STATUS_OK(test_iam_permission_result);
    ASSERT_EQ(1, test_iam_permission_result->permissions_size());
    ASSERT_EQ("spanner.databases.read",
              test_iam_permission_result->permissions(0));
  }

  auto get_ddl_result = client_.GetDatabaseDdl(database_);
  ASSERT_STATUS_OK(get_ddl_result);
  EXPECT_EQ(0, get_ddl_result->statements_size());

  std::vector<std::string> statements;
  if (!emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    statements.push_back("ALTER DATABASE `" + database_.database_id() +
                         "` SET OPTIONS (version_retention_period='7d')");
  }
  statements.emplace_back(R"""(
        CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024),
          SingerInfo BYTES(MAX)
        ) PRIMARY KEY (SingerId)
      )""");
  auto update_future = client_.UpdateDatabase(database_, statements);
  auto metadata = update_future.get();
  ASSERT_STATUS_OK(metadata);
  EXPECT_THAT(metadata->database(), EndsWith(database_.database_id()));
  EXPECT_EQ(statements.size(), metadata->statements_size());
  EXPECT_EQ(statements.size(), metadata->commit_timestamps_size());
  if (metadata->statements_size() >= 1) {
    EXPECT_THAT(metadata->statements(), Contains(HasSubstr("CREATE TABLE")));
  }
  if (metadata->statements_size() >= 2) {
    EXPECT_THAT(metadata->statements(), Contains(HasSubstr("ALTER DATABASE")));
  }
  EXPECT_FALSE(metadata->throttled());

  EXPECT_TRUE(DatabaseExists()) << "Database " << database_;
  auto drop_status = client_.DropDatabase(database_);
  EXPECT_STATUS_OK(drop_status);
  EXPECT_FALSE(DatabaseExists()) << "Database " << database_;
}

// @test Verify we can create a database with an encryption key.
TEST_F(DatabaseAdminClientTest, CreateWithEncryptionKey) {
  if (emulator_) GTEST_SKIP() << "emulator does not support CMEK";
  KmsKeyName encryption_key(instance_.project_id(), kLocation, kKeyRing,
                            kKeyName);
  auto database_future = client_.CreateDatabase(
      database_, /*extra_statements=*/{}, encryption_key);
  auto database = database_future.get();
  EXPECT_STATUS_OK(database);

  auto get_result = client_.GetDatabase(database_);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(database->name(), get_result->name());
  // Verify the encryption key name
  // TODO(salty) it seems to me that these expectations should also hold true
  // for the `database` returned from `CreateDatabase`, but the encryption
  // config is not present in `database`. I talked to the spanner folks about
  // it, waiting to hear if they're going to change it.
  EXPECT_TRUE(get_result->has_encryption_config());
  EXPECT_EQ(get_result->encryption_config().kms_key_name(),
            encryption_key.FullName());

  auto drop_status = client_.DropDatabase(database_);
  EXPECT_STATUS_OK(drop_status);
}

// @test Verify creating a database fails if a nonexistent encryption key is
// supplied.
TEST_F(DatabaseAdminClientTest, CreateWithNonexistentEncryptionKey) {
  if (emulator_) GTEST_SKIP() << "emulator does not support CMEK";
  KmsKeyName nonexistent_encryption_key(instance_.project_id(), kLocation,
                                        kKeyRing, "ceci-n-est-pas-une-cle");
  auto database_future = client_.CreateDatabase(
      database_, /*extra_statements=*/{}, nonexistent_encryption_key);
  auto database = database_future.get();
  EXPECT_FALSE(database.ok());
}

/// @test Verify setting version_retention_period via CreateDatabase().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodCreate) {
  // Set the version_retention_period via CreateDatabase().
  auto database =
      client_
          .CreateDatabase(
              database_,
              {absl::StrCat("ALTER DATABASE `", database_.database_id(),
                            "` SET OPTIONS (version_retention_period='7d')")})
          .get();
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_THAT(database, Not(IsOk()));
    return;
  }
  ASSERT_THAT(database, IsOk());
  EXPECT_EQ(database_.FullName(), database->name());
  EXPECT_EQ("7d", database->version_retention_period());

  // Verify that version_retention_period is returned from GetDatabase().
  auto get = client_.GetDatabase(database_);
  ASSERT_THAT(get, IsOk());
  EXPECT_EQ(database->name(), get->name());
  EXPECT_EQ("7d", get->version_retention_period());

  // Verify that earliest_version_time doesn't go past database create_time.
  EXPECT_TRUE(get->has_create_time());
  EXPECT_TRUE(get->has_earliest_version_time());
  EXPECT_LE(MakeTimestamp(get->create_time()).value(),
            MakeTimestamp(get->earliest_version_time()).value());

  auto drop = client_.DropDatabase(database_);
  EXPECT_THAT(drop, IsOk());
}

/// @test Verify setting bad version_retention_period via CreateDatabase().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodCreateFailure) {
  // Set an invalid version_retention_period (zero) via CreateDatabase(),
  // and verify that an error is returned.
  auto database =
      client_
          .CreateDatabase(
              database_,
              {absl::StrCat("ALTER DATABASE `", database_.database_id(),
                            "` SET OPTIONS (version_retention_period='0')")})
          .get();
  EXPECT_THAT(database, Not(IsOk()));
}

/// @test Verify setting version_retention_period via UpdateDatabase().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodUpdate) {
  // Create the database.
  auto database = client_
                      .CreateDatabase(database_,
                                      /*extra_statements=*/{})
                      .get();
  ASSERT_THAT(database, IsOk());
  EXPECT_EQ(database_.FullName(), database->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", database->version_retention_period());
  } else {
    EXPECT_NE("", database->version_retention_period());  // default value
  }

  // Set the version_retention_period via UpdateDatabase().
  auto update =
      client_
          .UpdateDatabase(
              database_,
              {absl::StrCat("ALTER DATABASE `", database_.database_id(),
                            "` SET OPTIONS (version_retention_period='7d')")})
          .get();
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_THAT(update, Not(IsOk()));
  } else {
    ASSERT_THAT(update, IsOk());
    EXPECT_EQ(database->name(), update->database());
    EXPECT_THAT(update->statements(),
                Contains(ContainsRegex("version_retention_period *= *'7d'")));
  }

  // Verify that version_retention_period is returned from GetDatabase().
  auto get = client_.GetDatabase(database_);
  ASSERT_THAT(get, IsOk());
  EXPECT_EQ(database->name(), get->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", get->version_retention_period());
  } else {
    EXPECT_EQ("7d", get->version_retention_period());
  }

  // Verify that version_retention_period is returned via ListDatabases().
  auto list_db = [&] {
    for (auto const& db : client_.ListDatabases(instance_)) {
      if (db && db->name() == database_.FullName()) return db;
    }
    return StatusOr<google::spanner::admin::database::v1::Database>{
        Status{StatusCode::kNotFound, "disappeared"}};
  }();
  ASSERT_THAT(list_db, IsOk());
  EXPECT_EQ(database->name(), list_db->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", list_db->version_retention_period());
  } else {
    EXPECT_EQ("7d", list_db->version_retention_period());
  }

  // Verify that version_retention_period is returned from GetDatabaseDdl().
  auto ddl = client_.GetDatabaseDdl(database_);
  ASSERT_THAT(ddl, IsOk());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
  } else {
    EXPECT_THAT(ddl->statements(),
                Contains(ContainsRegex("version_retention_period *= *'7d'")));
  }

  auto drop = client_.DropDatabase(database_);
  EXPECT_THAT(drop, IsOk());
}

/// @test Verify setting bad version_retention_period via UpdateDatabase().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodUpdateFailure) {
  // Create the database.
  auto database = client_
                      .CreateDatabase(database_,
                                      /*extra_statements=*/{})
                      .get();
  ASSERT_THAT(database, IsOk());
  EXPECT_EQ(database_.FullName(), database->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", database->version_retention_period());
  } else {
    EXPECT_NE("", database->version_retention_period());  // default value
  }

  auto get0 = client_.GetDatabase(database_);
  ASSERT_THAT(get0, IsOk());
  EXPECT_EQ(database->name(), get0->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", get0->version_retention_period());
  } else {
    EXPECT_NE("", get0->version_retention_period());  // default value
  }

  // Set an invalid version_retention_period (zero) via UpdateDatabase(),
  // and verify that an error is returned.
  auto update =
      client_
          .UpdateDatabase(
              database_,
              {absl::StrCat("ALTER DATABASE `", database_.database_id(),
                            "` SET OPTIONS (version_retention_period='0')")})
          .get();
  EXPECT_THAT(update, Not(IsOk()));

  // Also verify that version_retention_period was NOT changed.
  auto get = client_.GetDatabase(database_);
  ASSERT_THAT(get, IsOk());
  EXPECT_EQ(database->name(), get->name());
  EXPECT_EQ(get0->version_retention_period(), get->version_retention_period());

  auto drop = client_.DropDatabase(database_);
  EXPECT_THAT(drop, IsOk());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
