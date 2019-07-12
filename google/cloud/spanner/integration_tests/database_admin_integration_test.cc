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

#include "google/cloud/spanner/database_admin_client.h"
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

std::string RandomDatabaseName(
    google::cloud::internal::DefaultPRNG& generator) {
  // A database ID must be between 2 and 30 characters, fitting the regular
  // expression `[a-z][a-z0-9_\-]*[a-z0-9]`
  int max_size = 30;
  std::string const prefix = "db-";
  auto size = static_cast<int>(max_size - 1 - prefix.size());
  return prefix +
         google::cloud::internal::Sample(
             generator, size, "abcdefghijlkmnopqrstuvwxyz012345689_-") +
         google::cloud::internal::Sample(generator, 1,
                                         "abcdefghijlkmnopqrstuvwxyz");
}

/// @test Verify the basic CRUD operations for databases work.
TEST(DatabaseAdminClient, DatabaseBasicCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");
  ASSERT_FALSE(project_id.empty());
  ASSERT_FALSE(instance_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string database_id = RandomDatabaseName(generator);

  DatabaseAdminClient client;
  auto database_future =
      client.CreateDatabase(project_id, instance_id, database_id);
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  EXPECT_THAT(database->name(), EndsWith(database_id));

  auto const create_table_statement = R"""(
                             CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024),
                                SingerInfo BYTES(MAX)
                             ) PRIMARY KEY (SingerId)
                            )""";

  auto update_future = client.UpdateDatabase(
      project_id, instance_id, database_id, {create_table_statement});
  auto metadata = update_future.get();
  EXPECT_STATUS_OK(metadata);
  EXPECT_THAT(metadata->database(), EndsWith(database_id));
  EXPECT_EQ(1, metadata->statements_size());
  EXPECT_EQ(1, metadata->commit_timestamps_size());
  if (metadata->statements_size() > 1) {
    EXPECT_EQ(create_table_statement, metadata->statements(0));
  }

  auto drop_status = client.DropDatabase(project_id, instance_id, database_id);
  EXPECT_STATUS_OK(drop_status);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
