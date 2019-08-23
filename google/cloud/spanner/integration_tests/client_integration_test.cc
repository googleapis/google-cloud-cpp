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
#include "google/cloud/spanner/database.h"
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
using ::testing::UnorderedElementsAreArray;

// Use a ::testing::Environment to create the database once.
class IntegrationTestEnvironment : public ::testing::Environment {
 public:
  static Database const& GetDatabase() { return *db_; }

 protected:
  void SetUp() override {
    auto project_id =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());
    auto instance_id =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
            .value_or("");
    ASSERT_FALSE(instance_id.empty());

    generator_ = new google::cloud::internal::DefaultPRNG(
        google::cloud::internal::MakeDefaultPRNG());
    auto database_id = spanner_testing::RandomDatabaseName(*generator_);

    db_ = new Database(project_id, instance_id, database_id);

    std::cout << "Creating database and table " << std::flush;
    DatabaseAdminClient admin_client;
    auto database_future =
        admin_client.CreateDatabase(*db_, {R"""(CREATE TABLE Singers (
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
    auto drop_status = admin_client.DropDatabase(*db_);
    EXPECT_STATUS_OK(drop_status);
  }

 private:
  static Database* db_;
  static google::cloud::internal::DefaultPRNG* generator_;
};

Database* IntegrationTestEnvironment::db_;
google::cloud::internal::DefaultPRNG* IntegrationTestEnvironment::generator_;

class ClientIntegrationTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    client_ = google::cloud::internal::make_unique<Client>(
        MakeConnection(IntegrationTestEnvironment::GetDatabase()));
  }

  void SetUp() override {
    auto txn = MakeReadWriteTransaction();
    auto reader = client_->ExecuteSql(
        txn, SqlStatement("DELETE FROM Singers WHERE true;"));
    EXPECT_STATUS_OK(reader);
    auto commit = client_->Commit(txn, {});
    EXPECT_STATUS_OK(commit);
  }

  static void TearDownTestSuite() { client_ = nullptr; }

 protected:
  static std::unique_ptr<Client> client_;
};

std::unique_ptr<Client> ClientIntegrationTest::client_;

/// @test Verify the basic insert operations for transaction commits.
TEST_F(ClientIntegrationTest, InsertAndCommit) {
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
TEST_F(ClientIntegrationTest, DeleteAndCommit) {
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

/// @test Verify that read-write transactions with multiple statements work.
TEST_F(ClientIntegrationTest, MultipleInserts) {
  auto commit_result = client_->Commit(
      MakeReadWriteTransaction(),
      {InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
           .EmplaceRow(1, "test-fname-1", "test-lname-1")
           .EmplaceRow(2, "test-fname-2", "test-lname-2")
           .Build()});
  EXPECT_STATUS_OK(commit_result);

  auto txn = MakeReadWriteTransaction();
  auto insert1 = client_->ExecuteSql(
      txn, SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                        "VALUES (@id, @fname, @lname)",
                        {{"id", Value(3)},
                         {"fname", Value("test-fname-3")},
                         {"lname", Value("test-lname-3")}}));
  EXPECT_STATUS_OK(insert1);
  auto insert2 = client_->ExecuteSql(
      txn, SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                        "VALUES (@id, @fname, @lname)",
                        {{"id", Value(4)},
                         {"fname", Value("test-fname-4")},
                         {"lname", Value("test-lname-4")}}));
  EXPECT_STATUS_OK(insert2);
  auto insert_commit_result = client_->Commit(txn, {});
  EXPECT_STATUS_OK(insert_commit_result);

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

  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2"),
                                   RowType(3, "test-fname-3", "test-lname-3"),
                                   RowType(4, "test-fname-4", "test-lname-4")));
}

/// @test Verify that Client::Rollback works as expected.
TEST_F(ClientIntegrationTest, TransactionRollback) {
  auto commit_result = client_->Commit(
      MakeReadWriteTransaction(),
      {InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
           .EmplaceRow(1, "test-fname-1", "test-lname-1")
           .EmplaceRow(2, "test-fname-2", "test-lname-2")
           .Build()});
  EXPECT_STATUS_OK(commit_result);

  auto txn = MakeReadWriteTransaction();
  auto insert1 = client_->ExecuteSql(
      txn, SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                        "VALUES (@id, @fname, @lname)",
                        {{"id", Value(3)},
                         {"fname", Value("test-fname-3")},
                         {"lname", Value("test-lname-3")}}));
  EXPECT_STATUS_OK(insert1);
  auto insert2 = client_->ExecuteSql(
      txn, SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                        "VALUES (@id, @fname, @lname)",
                        {{"id", Value(4)},
                         {"fname", Value("test-fname-4")},
                         {"lname", Value("test-lname-4")}}));
  EXPECT_STATUS_OK(insert2);

  auto reader = client_->Read(txn, "Singers", KeySet::All(),
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

  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2"),
                                   RowType(3, "test-fname-3", "test-lname-3"),
                                   RowType(4, "test-fname-4", "test-lname-4")));

  auto insert_rollback_result = client_->Rollback(txn);
  EXPECT_STATUS_OK(insert_rollback_result);

  returned_rows.clear();
  reader = client_->Read("Singers", KeySet::All(),
                         {"SingerId", "FirstName", "LastName"});
  EXPECT_STATUS_OK(reader);
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
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2")));
}

/// @test Verify the basics of RunTransaction().
TEST_F(ClientIntegrationTest, RunTransaction) {
  Transaction::ReadWriteOptions rw_opts{};

  // Insert SingerIds 100, 102, and 199.
  auto insert = [](Client const&, Transaction const&) {
    auto isb =
        InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"});
    isb.AddRow(MakeRow(100, "first-name-100", "last-name-100"));
    isb.AddRow(MakeRow(102, "first-name-102", "last-name-102"));
    isb.AddRow(MakeRow(199, "first-name-199", "last-name-199"));
    return Mutations{isb.Build()};
  };
  auto insert_result = RunTransaction(*client_, rw_opts, insert);
  EXPECT_STATUS_OK(insert_result);
  EXPECT_NE(Timestamp{}, insert_result->commit_timestamp);

  // Delete SingerId 102.
  auto dele = [](Client const&, Transaction const&) {
    auto ksb = KeySetBuilder<Row<std::int64_t>>().Add(MakeRow(102));
    auto mutation = MakeDeleteMutation("Singers", ksb.Build());
    return Mutations{mutation};
  };
  auto delete_result = RunTransaction(*client_, rw_opts, dele);
  EXPECT_STATUS_OK(delete_result);
  EXPECT_LT(insert_result->commit_timestamp, delete_result->commit_timestamp);

  // Read SingerIds [100 ... 200).
  std::vector<std::int64_t> ids;
  auto ksb = KeySetBuilder<Row<std::int64_t>>().Add(
      MakeKeyRange(MakeBoundClosed(MakeRow(100)), MakeBoundOpen(MakeRow(200))));
  auto results = client_->Read("Singers", ksb.Build(), {"SingerId"});
  EXPECT_STATUS_OK(results);
  if (results) {
    for (auto& row : results->Rows<std::int64_t>()) {
      EXPECT_STATUS_OK(row);
      if (row) ids.push_back(row->get<0>());
    }
  }
  EXPECT_THAT(ids, UnorderedElementsAre(100, 199));
}

/// @test Test various forms of ExecuteSql()
TEST_F(ClientIntegrationTest, ExecuteSql) {
  {
    auto insert =
        client_->ExecuteSql(MakeReadWriteTransaction(),
                            SqlStatement(R"sql(
        INSERT INTO Singers (SingerId, FirstName, LastName)
        VALUES (@id, @fname, @lname))sql",
                                         {{"id", Value(1)},
                                          {"fname", Value("test-fname-1")},
                                          {"lname", Value("test-lname-1")}}));
    EXPECT_STATUS_OK(insert);
  }

  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> expected_rows;
  auto txn = MakeReadWriteTransaction();
  for (int i = 2; i != 10; ++i) {
    auto s = std::to_string(i);
    auto insert = client_->ExecuteSql(
        txn, SqlStatement(R"sql(
        INSERT INTO Singers (SingerId, FirstName, LastName)
        VALUES (@id, @fname, @lname))sql",
                          {{"id", Value(i)},
                           {"fname", Value("test-fname-" + s)},
                           {"lname", Value("test-lname-" + s)}}));
    EXPECT_STATUS_OK(insert);
    expected_rows.push_back(MakeRow(i, "test-fname-" + s, "test-lname-" + s));
  }

  auto delete_1 = client_->ExecuteSql(txn, SqlStatement(R"sql(
        DELETE FROM Singers WHERE SingerId = @id)sql",
                                                        {{"id", Value(1)}}));
  EXPECT_STATUS_OK(delete_1);
  auto commit = client_->Commit(txn, {});
  EXPECT_STATUS_OK(commit);

  auto reader = client_->ExecuteSql(
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers", {}));
  EXPECT_STATUS_OK(reader);

  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> actual_rows;
  if (reader) {
    int row_number = 0;
    for (auto& row : reader->Rows<std::int64_t, std::string, std::string>()) {
      EXPECT_STATUS_OK(row);
      if (!row) break;
      SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
      actual_rows.push_back(*std::move(row));
    }
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
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
