// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/testing/instance_location.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/kms_key_name.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

// Constants used to identify the encryption key.
auto constexpr kKeyRing = "spanner-cmek";
auto constexpr kKeyName = "spanner-cmek-test-key";

class DatabaseAdminClientTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  // We can't use ASSERT* in the constructor, so defer initializing `instance_`
  // and `database_` until `SetUp()`.
  DatabaseAdminClientTest()
      : instance_("placeholder", "placeholder"),
        database_(instance_, "placeholder"),
        client_(spanner_admin::MakeDatabaseAdminConnection()) {}

  void SetUp() override {
    using ::google::cloud::internal::GetEnv;
    emulator_ = GetEnv("SPANNER_EMULATOR_HOST").has_value();
    auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());

    auto generator = google::cloud::internal::MakeDefaultPRNG();

    auto instance_id = spanner_testing::PickRandomInstance(
        generator, project_id, "NOT name:/instances/test-instance-mr-");
    ASSERT_STATUS_OK(instance_id);
    instance_ = Instance(project_id, *instance_id);

    auto location = spanner_testing::InstanceLocation(instance_);
    if (emulator_) {
      if (!location) location = "emulator";
    } else {
      ASSERT_STATUS_OK(location);
    }
    location_ = *std::move(location);

    database_ =
        Database(instance_, spanner_testing::RandomDatabaseName(generator));

    test_iam_service_account_ =
        GetEnv("GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT").value_or("");
    ASSERT_TRUE(emulator_ || !test_iam_service_account_.empty());
  }

  // Does `database_` exist in `instance_`?
  bool DatabaseExists() {
    auto full_name = database_.FullName();
    for (auto const& database : client_.ListDatabases(instance_.FullName())) {
      EXPECT_STATUS_OK(database);
      if (!database) break;
      if (database->name() == full_name) return true;
    }
    return false;
  };

  Instance instance_;
  std::string location_;
  Database database_;
  spanner_admin::DatabaseAdminClient client_;
  bool emulator_;
  std::string test_iam_service_account_;
};

/// @test Verify the basic CRUD operations for databases work.
TEST_F(DatabaseAdminClientTest, DatabaseBasicCRUD) {
  // We test Client::ListDatabases() by verifying that (a) it does not return
  // a randomly generated database name before we create a database with that
  // name, (b) it *does* return that database name once created, and (c) it no
  // longer returns that name once the database is dropped. Implicitly that
  // also tests that Client::DropDatabase() and client::CreateDatabase() do
  // something, which is nice.
  EXPECT_FALSE(DatabaseExists()) << "Database " << database_
                                 << " already exists, this is unexpected as "
                                    "the database id is selected at random.";

  auto database =
      client_
          .CreateDatabase(
              database_.instance().FullName(),
              absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"))
          .get();
  ASSERT_STATUS_OK(database);
  EXPECT_THAT(database->name(), EndsWith(database_.database_id()));
  EXPECT_FALSE(database->has_encryption_config());
  EXPECT_THAT(database->encryption_info(), IsEmpty());
  if (emulator_) {
    EXPECT_EQ(database->database_dialect(),
              google::spanner::admin::database::v1::DatabaseDialect::
                  DATABASE_DIALECT_UNSPECIFIED);
  } else {
    if (database->database_dialect() ==
        google::spanner::admin::database::v1::DatabaseDialect::
            DATABASE_DIALECT_UNSPECIFIED) {
      // TODO(#8573): Remove when CreateDatabase() returns correct dialect.
      database->set_database_dialect(google::spanner::admin::database::v1::
                                         DatabaseDialect::GOOGLE_STANDARD_SQL);
    }
    EXPECT_EQ(database->database_dialect(),
              google::spanner::admin::database::v1::DatabaseDialect::
                  GOOGLE_STANDARD_SQL);
  }

  auto get_result = client_.GetDatabase(database_.FullName());
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(database->name(), get_result->name());
  EXPECT_EQ(database->database_dialect(), get_result->database_dialect());
  EXPECT_FALSE(get_result->has_encryption_config());
  if (emulator_) {
    EXPECT_THAT(get_result->encryption_info(), IsEmpty());
  } else {
    EXPECT_EQ(get_result->encryption_info().size(), 1);
    if (!get_result->encryption_info().empty()) {
      EXPECT_EQ(get_result->encryption_info()[0].encryption_type(),
                google::spanner::admin::database::v1::EncryptionInfo::
                    GOOGLE_DEFAULT_ENCRYPTION);
    }
  }

  auto list_db = [&] {
    for (auto const& db : client_.ListDatabases(instance_.FullName())) {
      if (db && db->name() == database_.FullName()) return db;
    }
    return StatusOr<google::spanner::admin::database::v1::Database>{
        Status{StatusCode::kNotFound, "disappeared"}};
  }();
  ASSERT_THAT(list_db, IsOk());
  EXPECT_EQ(database->name(), list_db->name());
  EXPECT_EQ(database->database_dialect(), list_db->database_dialect());

  if (!emulator_) {
    auto current_policy = client_.GetIamPolicy(database_.FullName());
    ASSERT_STATUS_OK(current_policy);
    EXPECT_EQ(0, current_policy->bindings_size());

    std::string const reader_role = "roles/spanner.databaseReader";
    std::string const writer_role = "roles/spanner.databaseUser";
    std::string const expected_member =
        "serviceAccount:" + test_iam_service_account_;
    auto& binding = *current_policy->add_bindings();
    binding.set_role(reader_role);
    *binding.add_members() = expected_member;

    auto updated_policy =
        client_.SetIamPolicy(database_.FullName(), *current_policy);
    ASSERT_THAT(updated_policy, AnyOf(IsOk(), StatusIs(StatusCode::kAborted)));
    if (updated_policy) {
      EXPECT_EQ(1, updated_policy->bindings_size());
      ASSERT_EQ(reader_role, updated_policy->bindings().Get(0).role());
      ASSERT_EQ(1, updated_policy->bindings().Get(0).members().size());
      ASSERT_EQ(expected_member,
                updated_policy->bindings().Get(0).members().Get(0));
    }

    // Perform a different update using the the OCC loop API:
    updated_policy = client_.SetIamPolicy(
        database_.FullName(),
        [this, &writer_role](google::iam::v1::Policy current) {
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
    current_policy = client_.GetIamPolicy(database_.FullName());
    ASSERT_STATUS_OK(current_policy);
    EXPECT_THAT(*updated_policy, IsProtoEqual(*current_policy));

    auto test_iam_permission_result = client_.TestIamPermissions(
        database_.FullName(), {"spanner.databases.read"});
    ASSERT_STATUS_OK(test_iam_permission_result);
    ASSERT_EQ(1, test_iam_permission_result->permissions_size());
    ASSERT_EQ("spanner.databases.read",
              test_iam_permission_result->permissions(0));
  }

  auto get_ddl_result = client_.GetDatabaseDdl(database_.FullName());
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
      )""");
  if (!emulator_) {
    // TODO(#6873): Remove this check when the emulator supports JSON.
    statements.back().append(R"""(,SingerDetails JSON)""");
  }
  statements.back().append(R"""(
        ) PRIMARY KEY (SingerId)
      )""");
  auto metadata =
      client_.UpdateDatabaseDdl(database_.FullName(), statements).get();
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

  // Verify that a JSON column cannot be used as an index.
  statements.clear();
  statements.emplace_back(R"""(
        CREATE INDEX SingersByDetail
          ON Singers(SingerDetails)
      )""");
  metadata = client_.UpdateDatabaseDdl(database_.FullName(), statements).get();
  if (!emulator_) {
    // TODO(#6873): Remove this check when the emulator supports JSON.
    EXPECT_THAT(metadata,
                StatusIs(StatusCode::kFailedPrecondition,
                         AllOf(HasSubstr("Index SingersByDetail"),
                               HasSubstr("column of unsupported type JSON"))));
  } else {
    EXPECT_THAT(
        metadata,
        StatusIs(
            StatusCode::kInvalidArgument,
            AllOf(HasSubstr("Index SingersByDetail"),
                  HasSubstr("column SingerDetails which does not exist"))));
  }

  // Verify that a JSON column cannot be used as a primary key.
  statements.clear();
  statements.emplace_back(R"""(
        CREATE TABLE JsonKey (
          Key JSON NOT NULL
        ) PRIMARY KEY (Key)
      )""");
  metadata = client_.UpdateDatabaseDdl(database_.FullName(), statements).get();
  if (!emulator_) {
    // TODO(#6873): Remove this check when the emulator supports JSON.
    EXPECT_THAT(metadata,
                StatusIs(StatusCode::kInvalidArgument,
                         AllOf(HasSubstr("Key has type JSON"),
                               HasSubstr("part of the primary key"))));
  } else {
    EXPECT_THAT(metadata, Not(IsOk()));
  }

  EXPECT_TRUE(DatabaseExists()) << "Database " << database_;
  auto drop_status = client_.DropDatabase(database_.FullName());
  EXPECT_STATUS_OK(drop_status);
  EXPECT_FALSE(DatabaseExists()) << "Database " << database_;
}

/// @test Verify setting version_retention_period via CreateDatabase().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodCreate) {
  // Set the version_retention_period via CreateDatabase().
  google::spanner::admin::database::v1::CreateDatabaseRequest creq;
  creq.set_parent(database_.instance().FullName());
  creq.set_create_statement(
      absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"));
  creq.add_extra_statements(
      absl::StrCat("ALTER DATABASE `", database_.database_id(),
                   "` SET OPTIONS (version_retention_period='7d')"));
  creq.set_database_dialect(google::spanner::admin::database::v1::
                                DatabaseDialect::GOOGLE_STANDARD_SQL);
  auto database = client_.CreateDatabase(creq).get();
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_THAT(database, Not(IsOk()));
    return;
  }
  ASSERT_THAT(database, IsOk());
  EXPECT_EQ(database_.FullName(), database->name());
  EXPECT_EQ("7d", database->version_retention_period());
  EXPECT_EQ(database->database_dialect(),
            google::spanner::admin::database::v1::DatabaseDialect::
                GOOGLE_STANDARD_SQL);

  // Verify that version_retention_period is returned from GetDatabase().
  auto get = client_.GetDatabase(database_.FullName());
  ASSERT_THAT(get, IsOk());
  EXPECT_EQ(database->name(), get->name());
  EXPECT_EQ(database->database_dialect(), get->database_dialect());
  EXPECT_EQ("7d", get->version_retention_period());

  // Verify that earliest_version_time doesn't go past database create_time.
  EXPECT_TRUE(get->has_create_time());
  EXPECT_TRUE(get->has_earliest_version_time());
  EXPECT_LE(MakeTimestamp(get->create_time()).value(),
            MakeTimestamp(get->earliest_version_time()).value());

  // Verify that version_retention_period is returned via ListDatabases().
  auto list_db = [&] {
    for (auto const& db : client_.ListDatabases(instance_.FullName())) {
      if (db && db->name() == database_.FullName()) return db;
    }
    return StatusOr<google::spanner::admin::database::v1::Database>{
        Status{StatusCode::kNotFound, "disappeared"}};
  }();
  ASSERT_THAT(list_db, IsOk());
  EXPECT_EQ(database->name(), list_db->name());
  EXPECT_EQ(database->database_dialect(), list_db->database_dialect());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", list_db->version_retention_period());
  } else {
    EXPECT_EQ("7d", list_db->version_retention_period());
  }

  auto drop = client_.DropDatabase(database_.FullName());
  EXPECT_THAT(drop, IsOk());
}

/// @test Verify setting bad version_retention_period via CreateDatabase().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodCreateFailure) {
  // Set an invalid version_retention_period (zero) via CreateDatabase(),
  // and verify that an error is returned.
  google::spanner::admin::database::v1::CreateDatabaseRequest creq;
  creq.set_parent(database_.instance().FullName());
  creq.set_create_statement(
      absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"));
  creq.add_extra_statements(
      absl::StrCat("ALTER DATABASE `", database_.database_id(),
                   "` SET OPTIONS (version_retention_period='0')"));
  auto database = client_.CreateDatabase(creq).get();
  EXPECT_THAT(database, Not(IsOk()));
}

/// @test Verify setting version_retention_period via UpdateDatabaseDdl().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodUpdate) {
  // Create the database.
  auto database =
      client_
          .CreateDatabase(
              database_.instance().FullName(),
              absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"))
          .get();
  ASSERT_THAT(database, IsOk());
  EXPECT_EQ(database_.FullName(), database->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", database->version_retention_period());
  } else {
    EXPECT_NE("", database->version_retention_period());  // default value
  }

  // Set the version_retention_period via UpdateDatabaseDdl().
  auto update =
      client_
          .UpdateDatabaseDdl(
              database_.FullName(),
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
  auto get = client_.GetDatabase(database_.FullName());
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
    for (auto const& db : client_.ListDatabases(instance_.FullName())) {
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
  auto ddl = client_.GetDatabaseDdl(database_.FullName());
  ASSERT_THAT(ddl, IsOk());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
  } else {
    EXPECT_THAT(ddl->statements(),
                Contains(ContainsRegex("version_retention_period *= *'7d'")));
  }

  auto drop = client_.DropDatabase(database_.FullName());
  EXPECT_THAT(drop, IsOk());
}

/// @test Verify setting bad version_retention_period via UpdateDatabaseDdl().
TEST_F(DatabaseAdminClientTest, VersionRetentionPeriodUpdateFailure) {
  // Create the database.
  auto database =
      client_
          .CreateDatabase(
              database_.instance().FullName(),
              absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"))
          .get();
  ASSERT_THAT(database, IsOk());
  EXPECT_EQ(database_.FullName(), database->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", database->version_retention_period());
  } else {
    EXPECT_NE("", database->version_retention_period());  // default value
  }

  auto get0 = client_.GetDatabase(database_.FullName());
  ASSERT_THAT(get0, IsOk());
  EXPECT_EQ(database->name(), get0->name());
  if (emulator_) {
    // TODO(#5479): Awaiting emulator support for version_retention_period.
    EXPECT_EQ("", get0->version_retention_period());
  } else {
    EXPECT_NE("", get0->version_retention_period());  // default value
  }

  // Set an invalid version_retention_period (zero) via UpdateDatabaseDdl(),
  // and verify that an error is returned.
  auto update =
      client_
          .UpdateDatabaseDdl(
              database_.FullName(),
              {absl::StrCat("ALTER DATABASE `", database_.database_id(),
                            "` SET OPTIONS (version_retention_period='0')")})
          .get();
  EXPECT_THAT(update, Not(IsOk()));

  // Also verify that version_retention_period was NOT changed.
  auto get = client_.GetDatabase(database_.FullName());
  ASSERT_THAT(get, IsOk());
  EXPECT_EQ(database->name(), get->name());
  EXPECT_EQ(get0->version_retention_period(), get->version_retention_period());

  auto drop = client_.DropDatabase(database_.FullName());
  EXPECT_THAT(drop, IsOk());
}

/// @test Verify we can create a database with an encryption key.
TEST_F(DatabaseAdminClientTest, CreateWithEncryptionKey) {
  if (emulator_) GTEST_SKIP() << "emulator does not support CMEK";
  KmsKeyName encryption_key(instance_.project_id(), location_, kKeyRing,
                            kKeyName);
  google::spanner::admin::database::v1::CreateDatabaseRequest creq;
  creq.set_parent(database_.instance().FullName());
  creq.set_create_statement(
      absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"));
  creq.mutable_encryption_config()->set_kms_key_name(encryption_key.FullName());
  auto database = client_.CreateDatabase(creq).get();
  ASSERT_STATUS_OK(database);
  EXPECT_EQ(database->name(), database_.FullName());
  EXPECT_TRUE(database->has_encryption_config());
  if (database->has_encryption_config()) {
    EXPECT_EQ(database->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }

  auto get_result = client_.GetDatabase(database_.FullName());
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(database->name(), get_result->name());
  EXPECT_TRUE(get_result->has_encryption_config());
  if (get_result->has_encryption_config()) {
    EXPECT_EQ(get_result->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }

  // Verify that encryption config is returned via ListDatabases().
  auto list_db = [&] {
    for (auto const& db :
         client_.ListDatabases(database_.instance().FullName())) {
      if (db && db->name() == database_.FullName()) return db;
    }
    return StatusOr<google::spanner::admin::database::v1::Database>{
        Status{StatusCode::kNotFound, "disappeared"}};
  }();
  ASSERT_THAT(list_db, IsOk());
  EXPECT_TRUE(list_db->has_encryption_config());
  if (list_db->has_encryption_config()) {
    EXPECT_EQ(list_db->encryption_config().kms_key_name(),
              encryption_key.FullName());
  }

  EXPECT_STATUS_OK(client_.DropDatabase(database_.FullName()));
}

/// @test Verify creating a database fails if a nonexistent encryption key is
/// supplied.
TEST_F(DatabaseAdminClientTest, CreateWithNonexistentEncryptionKey) {
  if (emulator_) GTEST_SKIP() << "emulator does not support CMEK";
  KmsKeyName nonexistent_encryption_key(instance_.project_id(), location_,
                                        kKeyRing, "ceci-n-est-pas-une-cle");
  google::spanner::admin::database::v1::CreateDatabaseRequest creq;
  creq.set_parent(database_.instance().FullName());
  creq.set_create_statement(
      absl::StrCat("CREATE DATABASE `", database_.database_id(), "`"));
  creq.mutable_encryption_config()->set_kms_key_name(
      nonexistent_encryption_key.FullName());
  auto database = client_.CreateDatabase(creq).get();
  EXPECT_THAT(database, StatusIs(StatusCode::kFailedPrecondition,
                                 HasSubstr("KMS Key provided is not usable")));
}

/// @test Verify basic operations for PostgreSQL-type databases.
TEST_F(DatabaseAdminClientTest, DatabasePostgreSQLBasics) {
  google::spanner::admin::database::v1::CreateDatabaseRequest creq;
  creq.set_parent(database_.instance().FullName());
  creq.set_create_statement(
      absl::StrCat("CREATE DATABASE \"", database_.database_id(), "\""));
  creq.set_database_dialect(
      google::spanner::admin::database::v1::DatabaseDialect::POSTGRESQL);
  auto database = client_.CreateDatabase(creq).get();
  if (emulator_) {
    // This will let us know when the emulator starts supporting PostgreSQL
    // syntax to quote identifiers.
    EXPECT_THAT(database,
                StatusIs(StatusCode::kInvalidArgument,
                         HasSubstr("Error parsing Spanner DDL statement")));
    GTEST_SKIP() << "emulator does not support PostgreSQL";
  }
  ASSERT_STATUS_OK(database);
  EXPECT_THAT(database->name(), EndsWith(database_.database_id()));
  EXPECT_EQ(database->database_dialect(),
            google::spanner::admin::database::v1::DatabaseDialect::POSTGRESQL);

  // Verify that GetDatabase() returns the correct dialect.
  auto get = client_.GetDatabase(database->name());
  ASSERT_THAT(get, IsOk());
  EXPECT_EQ(database->name(), get->name());
  EXPECT_EQ(database->database_dialect(), get->database_dialect());

  // Verify that ListDatabases() returns the correct dialect.
  auto list_db = [&] {
    for (auto const& db : client_.ListDatabases(instance_.FullName())) {
      if (db && db->name() == database_.FullName()) return db;
    }
    return StatusOr<google::spanner::admin::database::v1::Database>{
        Status{StatusCode::kNotFound, "disappeared"}};
  }();
  ASSERT_THAT(list_db, IsOk());
  EXPECT_EQ(database->name(), list_db->name());
  EXPECT_EQ(database->database_dialect(), list_db->database_dialect());

  auto drop_status = client_.DropDatabase(database->name());
  EXPECT_STATUS_OK(drop_status);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
