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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::EndsWith;

/// @test Verify the basic CRUD operations for databases work.
TEST(DatabaseAdminClient, DatabaseBasicCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto instance_id = spanner_testing::PickRandomInstance(generator, project_id);
  ASSERT_STATUS_OK(instance_id);

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
    for (auto database : client.ListDatabases(in)) {
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

  auto get_iam_policy_result = client.GetIamPolicy(db);
  ASSERT_STATUS_OK(get_iam_policy_result);
  EXPECT_EQ(0, get_iam_policy_result->bindings_size());

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
