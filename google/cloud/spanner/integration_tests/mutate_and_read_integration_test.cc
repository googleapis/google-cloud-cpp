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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/mutations.h"
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

using ::testing::UnorderedElementsAre;

/// @test Verify the basic insert operations for transaction commits.
TEST(CommitIntegrationTest, Insert) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");
  ASSERT_FALSE(project_id.empty());
  ASSERT_FALSE(instance_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string database_id = spanner_testing::RandomDatabaseName(generator);

  DatabaseAdminClient admin_client;
  auto database_future =
      admin_client.CreateDatabase(project_id, instance_id, database_id, {R"""(
                             CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024)
                             ) PRIMARY KEY (SingerId)
                            )"""});
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  auto database_name = MakeDatabaseName(project_id, instance_id, database_id);
  auto conn = MakeConnection(database_name);
  Client client(std::move(conn));

  auto commit_result = client.Commit(
      MakeReadWriteTransaction(),
      {InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
           .EmplaceRow(1, "test-first-name-1", "test-last-name-1")
           .EmplaceRow(2, "test-first-name-2", "test-last-name-2")
           .Build()});
  EXPECT_STATUS_OK(commit_result);

  auto reader = client.Read("Singers", KeySet::All(),
                            {"SingerId", "FirstName", "LastName"});
  // Cannot assert, we need to call DeleteDatabase() later.
  EXPECT_STATUS_OK(reader);

  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> returned_rows;
  if (reader) {
    int row_number = 0;
    for (auto& row : reader->Rows<std::int64_t, std::string, std::string>()) {
      EXPECT_STATUS_OK(row);
      if (!row) break;
      SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
      returned_rows.push_back(*std::move(row));
    }
  }

  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(
                  RowType(1, "test-first-name-1", "test-last-name-1"),
                  RowType(2, "test-first-name-2", "test-last-name-2")));

  auto drop_status =
      admin_client.DropDatabase(project_id, instance_id, database_id);
  EXPECT_STATUS_OK(drop_status);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
