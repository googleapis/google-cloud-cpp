// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/testing/cleanup_stale_databases.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <chrono>
#include <iostream>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

using ::google::cloud::testing_util::IsOk;

// How old a database must be to be considered stale.
auto constexpr kStaleAge = std::chrono::hours(7 * 24);

}  // namespace

internal::DefaultPRNG* DatabaseIntegrationTest::generator_;
spanner::Database* DatabaseIntegrationTest::db_;
bool DatabaseIntegrationTest::emulator_;

void DatabaseIntegrationTest::SetUpTestSuite() {
  testing_util::IntegrationTest::SetUpTestSuite();
  generator_ = new internal::DefaultPRNG(internal::MakeDefaultPRNG());

  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());
  auto instance_id = PickRandomInstance(*generator_, project_id);
  ASSERT_THAT(instance_id, IsOk());
  auto database_id = RandomDatabaseName(*generator_);
  db_ = new spanner::Database(project_id, *instance_id, database_id);
  emulator_ = internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();

  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());
  CleanupStaleDatabases(admin_client, project_id, *instance_id,
                        std::chrono::system_clock::now() - kStaleAge);

  std::cout << "Creating database and table " << std::flush;
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(db_->instance().FullName());
  request.set_create_statement("CREATE DATABASE `" + db_->database_id() + "`");
  if (!emulator_) {                // version_retention_period
    request.add_extra_statements(  //
        "ALTER DATABASE `" + db_->database_id() + "` " +
        "SET OPTIONS (version_retention_period='2h')");
  }
  request.add_extra_statements(R"sql(
        CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024)
        ) PRIMARY KEY (SingerId)
      )sql");
  request.add_extra_statements(R"sql(
        CREATE TABLE DataTypes (
          Id STRING(256) NOT NULL,
          BoolValue BOOL,
          Int64Value INT64,
          Float64Value FLOAT64,
          StringValue STRING(1024),
          BytesValue BYTES(1024),
          TimestampValue TIMESTAMP,
          DateValue DATE,
          JsonValue JSON,
          NumericValue NUMERIC,
          ArrayBoolValue ARRAY<BOOL>,
          ArrayInt64Value ARRAY<INT64>,
          ArrayFloat64Value ARRAY<FLOAT64>,
          ArrayStringValue ARRAY<STRING(1024)>,
          ArrayBytesValue ARRAY<BYTES(1024)>,
          ArrayTimestampValue ARRAY<TIMESTAMP>,
          ArrayDateValue ARRAY<DATE>,
          ArrayJsonValue ARRAY<JSON>,
          ArrayNumericValue ARRAY<NUMERIC>
        ) PRIMARY KEY (Id)
      )sql");
  // Verify that NUMERIC can be used as a table key.
  request.add_extra_statements(R"sql(
        CREATE TABLE NumericKey (
          Key NUMERIC NOT NULL
        ) PRIMARY KEY (Key)
      )sql");
  auto database_future = admin_client.CreateDatabase(request);

  int i = 0;
  int const timeout = 600;
  while (++i < timeout) {
    auto status = database_future.wait_for(std::chrono::seconds(1));
    if (status != std::future_status::timeout) break;
    std::cout << '.' << std::flush;
  }
  if (i >= timeout) {
    std::cout << "TIMEOUT\n";
    FAIL();
  }
  auto database = database_future.get();
  ASSERT_THAT(database, IsOk());
  std::cout << "DONE\n";
}

void DatabaseIntegrationTest::TearDownTestSuite() {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());
  auto drop_status = admin_client.DropDatabase(db_->FullName());
  EXPECT_THAT(drop_status, IsOk());

  emulator_ = false;
  delete db_;
  db_ = nullptr;
  delete generator_;
  generator_ = nullptr;

  testing_util::IntegrationTest::TearDownTestSuite();
}

internal::DefaultPRNG* PgDatabaseIntegrationTest::generator_;
spanner::Database* PgDatabaseIntegrationTest::db_;
bool PgDatabaseIntegrationTest::emulator_;

void PgDatabaseIntegrationTest::SetUpTestSuite() {
  testing_util::IntegrationTest::SetUpTestSuite();
  generator_ = new internal::DefaultPRNG(internal::MakeDefaultPRNG());

  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());
  auto instance_id = PickRandomInstance(*generator_, project_id);
  ASSERT_THAT(instance_id, IsOk());
  auto database_id = RandomDatabaseName(*generator_);
  db_ = new spanner::Database(project_id, *instance_id, database_id);
  emulator_ = internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();

  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());
  CleanupStaleDatabases(admin_client, project_id, *instance_id,
                        std::chrono::system_clock::now() - kStaleAge);

  std::cout << "Creating PostgreSQL database and table " << std::flush;
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(db_->instance().FullName());
  request.set_create_statement("CREATE DATABASE \"" + db_->database_id() +
                               "\"");
  request.set_database_dialect(
      google::spanner::admin::database::v1::DatabaseDialect::POSTGRESQL);
  auto database_future = admin_client.CreateDatabase(request);

  int i = 0;
  int const timeout = 600;
  while (++i < timeout) {
    auto status = database_future.wait_for(std::chrono::seconds(1));
    if (status != std::future_status::timeout) break;
    std::cout << '.' << std::flush;
  }
  if (i >= timeout) {
    std::cout << "TIMEOUT\n";
    FAIL();
  }
  auto database = database_future.get();
  if (emulator_ && database.status().code() == StatusCode::kInvalidArgument) {
    // The emulator does not support PostgreSQL syntax to quote identifiers.
    std::cout << "INVALID-IGNORED\n";
    return;
  }
  ASSERT_THAT(database, IsOk());

  // DDL statements other than <CREATE DATABASE> are not allowed in database
  // creation requests for PostgreSQL-enabled databases, so separate them into
  // an attendant update request.
  std::vector<std::string> statements;
  statements.emplace_back(R"sql(
        CREATE TABLE Singers (
          SingerId   BIGINT NOT NULL,
          FirstName  CHARACTER VARYING(1024),
          LastName   CHARACTER VARYING(1024),
          PRIMARY KEY(SingerId)
        )
      )sql");
  statements.emplace_back(R"sql(
        CREATE TABLE DataTypes (
          Id CHARACTER VARYING(256) NOT NULL,
          BoolValue BOOLEAN,
          Int64Value BIGINT,
          Float64Value DOUBLE PRECISION,
          StringValue CHARACTER VARYING(1024),
          BytesValue BYTEA,
          TimestampValue TIMESTAMP WITH TIME ZONE,
          DateValue DATE,
          JsonValue JSONB,
          NumericValue NUMERIC,
          ArrayBoolValue BOOLEAN[],
          ArrayInt64Value BIGINT[],
          ArrayFloat64Value DOUBLE PRECISION[],
          ArrayStringValue CHARACTER VARYING(1024)[],
          ArrayBytesValue BYTEA[],
          ArrayTimestampValue TIMESTAMP WITH TIME ZONE[],
          ArrayDateValue DATE[],
          -- TODO(#10095): ArrayJsonValue JSONB[],
          ArrayNumericValue NUMERIC[],
          PRIMARY KEY(Id)
        )
      )sql");
  auto metadata_future =
      admin_client.UpdateDatabaseDdl(db_->FullName(), statements);
  while (++i < timeout) {
    auto status = metadata_future.wait_for(std::chrono::seconds(1));
    if (status != std::future_status::timeout) break;
    std::cout << '.' << std::flush;
  }
  if (i >= timeout) {
    std::cout << "TIMEOUT\n";
    FAIL();
  }
  auto metadata = metadata_future.get();
  ASSERT_THAT(metadata, IsOk());
  std::cout << "DONE\n";
}

void PgDatabaseIntegrationTest::TearDownTestSuite() {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());
  auto drop_status = admin_client.DropDatabase(db_->FullName());
  EXPECT_THAT(drop_status, IsOk());

  emulator_ = false;
  delete db_;
  db_ = nullptr;
  delete generator_;
  generator_ = nullptr;

  testing_util::IntegrationTest::TearDownTestSuite();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
