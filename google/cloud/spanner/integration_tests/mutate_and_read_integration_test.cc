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
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::UnorderedElementsAre;

// Use a ::testing::Environment to create the database once.
class IntegrationTestEnvironment : public ::testing::Environment {
 public:
  // NOLINTNEXTLINE
  static std::string const& project_id() { return *project_id_; }
  // NOLINTNEXTLINE
  static std::string const& instance_id() { return *instance_id_; }
  // NOLINTNEXTLINE
  static std::string const& database_id() { return *database_id_; }

  static std::string RandomTableName() {
    return google::cloud::internal::Sample(*generator_, 16,
                                           "abcdefghijklmnopqrstuvwxyz");
  }

 protected:
  void SetUp() override {
    project_id_ = new std::string(
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or(""));
    instance_id_ = new std::string(
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
            .value_or(""));

    generator_ = new google::cloud::internal::DefaultPRNG(
        google::cloud::internal::MakeDefaultPRNG());
    database_id_ =
        new std::string(spanner_testing::RandomDatabaseName(*generator_));

    std::cout << "Creating database and table" << std::flush;
    DatabaseAdminClient admin_client;
    auto database_future = admin_client.CreateDatabase(
        project_id(), instance_id(), database_id(), {R"""(CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024)
                             ) PRIMARY KEY (SingerId))"""});

    int i = 0;
    int const timeout = 120;
    while (++i < timeout) {
      auto status = database_future.wait_for(std::chrono::seconds(1));
      if (status == std::future_status::ready) break;
      std::cout << '.' << std::flush;
    }
    if (i >= timeout) {
      std::cout << "TIMEOUT\n";
      FAIL();
    }
    auto database = database_future.get();
    ASSERT_STATUS_OK(database);
    std::cout << "DONE\n";
  }

  void TearDown() override {
    DatabaseAdminClient admin_client;
    auto drop_status =
        admin_client.DropDatabase(project_id(), instance_id(), database_id());
    EXPECT_STATUS_OK(drop_status);
  }

 private:
  static std::string* project_id_;
  static std::string* instance_id_;
  static std::string* database_id_;
  static google::cloud::internal::DefaultPRNG* generator_;
};

std::string* IntegrationTestEnvironment::project_id_;
std::string* IntegrationTestEnvironment::instance_id_;
std::string* IntegrationTestEnvironment::database_id_;
google::cloud::internal::DefaultPRNG* IntegrationTestEnvironment::generator_;

class MutateAndReadIntegrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto project_id = IntegrationTestEnvironment::project_id();
    auto instance_id = IntegrationTestEnvironment::instance_id();
    ASSERT_FALSE(project_id.empty());
    ASSERT_FALSE(instance_id.empty());

    auto database_id = IntegrationTestEnvironment::database_id();
    client_ = google::cloud::internal::make_unique<Client>(
        MakeConnection(MakeDatabaseName(project_id, instance_id, database_id)));

    auto txn = MakeReadWriteTransaction();
    auto reader = client_->ExecuteSql(
        txn, SqlStatement("DELETE FROM Singers WHERE true;"));
    EXPECT_STATUS_OK(reader);
    auto commit = client_->Commit(txn, {});
    EXPECT_STATUS_OK(commit);
  }

 protected:
  std::unique_ptr<Client> client_;
};

/// @test Verify the basic insert operations for transaction commits.
TEST_F(MutateAndReadIntegrationTest, InsertAndCommit) {
  auto commit_result = client_->Commit(
      MakeReadWriteTransaction(),
      {InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
           .EmplaceRow(1, "test-first-name-1", "test-last-name-1")
           .EmplaceRow(2, "test-first-name-2", "test-last-name-2")
           .Build()});
  EXPECT_STATUS_OK(commit_result);

  auto reader = client_->Read("Singers", KeySet::All(),
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
}

/// @test Verify the basic delete mutations work.
TEST_F(MutateAndReadIntegrationTest, DeleteAndCommit) {
  auto commit_result = client_->Commit(
      MakeReadWriteTransaction(),
      {InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
           .EmplaceRow(1, "test-first-name-1", "test-last-name-1")
           .EmplaceRow(2, "test-first-name-2", "test-last-name-2")
           .Build()});
  EXPECT_STATUS_OK(commit_result);

  commit_result = client_->Commit(
      MakeReadWriteTransaction(),
      {MakeDeleteMutation("Singers", KeySetBuilder<Row<std::int64_t>>()
                                         .Add(MakeRow(std::int64_t(1)))
                                         .Build())});
  EXPECT_STATUS_OK(commit_result);

  auto reader = client_->Read("Singers", KeySet::All(),
                              {"SingerId", "FirstName", "LastName"});
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

  EXPECT_THAT(returned_rows, UnorderedElementsAre(RowType(
                                 2, "test-first-name-2", "test-last-name-2")));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::google::cloud::testing_util::InitGoogleMock(argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::spanner::IntegrationTestEnvironment());

  return RUN_ALL_TESTS();
}
