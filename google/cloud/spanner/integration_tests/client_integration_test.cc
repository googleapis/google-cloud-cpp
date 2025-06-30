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

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::NotNull;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class ClientIntegrationTest : public spanner_testing::DatabaseIntegrationTest {
 protected:
  static void SetUpTestSuite() {
    spanner_testing::DatabaseIntegrationTest::SetUpTestSuite();
    client_ = std::make_unique<Client>(MakeConnection(
        GetDatabase(),
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_GZIP)));
  }

  void SetUp() override {
    auto commit_result = client_->Commit(
        Mutations{MakeDeleteMutation("Singers", KeySet::All())},
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_DEFLATE));
    ASSERT_STATUS_OK(commit_result);
  }

  static StatusOr<Timestamp> DatabaseNow() {
    auto statement = SqlStatement("SELECT CURRENT_TIMESTAMP()");
    auto rows = client_->ExecuteQuery(
        std::move(statement),
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_NONE));
    auto row = GetSingularRow(StreamOf<std::tuple<Timestamp>>(rows));
    if (!row) return std::move(row).status();
    return std::get<0>(*row);
  }

  static void InsertTwoSingers() {
    auto commit_result = client_->Commit(
        Mutations{InsertMutationBuilder("Singers",
                                        {"SingerId", "FirstName", "LastName"})
                      .EmplaceRow(1, "test-fname-1", "test-lname-1")
                      .EmplaceRow(2, "test-fname-2", "test-lname-2")
                      .Build()},
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_DEFLATE));
    ASSERT_STATUS_OK(commit_result);
  }

  static void TearDownTestSuite() {
    client_ = nullptr;
    spanner_testing::DatabaseIntegrationTest::TearDownTestSuite();
  }

  static std::unique_ptr<Client> client_;
};

std::unique_ptr<Client> ClientIntegrationTest::client_;

class PgClientIntegrationTest
    : public spanner_testing::PgDatabaseIntegrationTest {
 protected:
  static void SetUpTestSuite() {
    spanner_testing::PgDatabaseIntegrationTest::SetUpTestSuite();
    client_ = std::make_unique<Client>(MakeConnection(
        GetDatabase(),
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_DEFLATE)));
  }

  void SetUp() override {
    auto commit_result = client_->Commit(
        Mutations{MakeDeleteMutation("Singers", KeySet::All())},
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_DEFLATE));
    ASSERT_STATUS_OK(commit_result);
  }

  static void InsertTwoSingers() {
    auto commit_result = client_->Commit(
        Mutations{InsertMutationBuilder("Singers",
                                        {"SingerId", "FirstName", "LastName"})
                      .EmplaceRow(1, "test-fname-1", "test-lname-1")
                      .EmplaceRow(2, "test-fname-2", "test-lname-2")
                      .Build()},
        Options{}.set<GrpcCompressionAlgorithmOption>(GRPC_COMPRESS_DEFLATE));
    ASSERT_STATUS_OK(commit_result);
  }

  static void TearDownTestSuite() {
    client_ = nullptr;
    spanner_testing::PgDatabaseIntegrationTest::TearDownTestSuite();
  }

  static std::unique_ptr<Client> client_;
};

std::unique_ptr<Client> PgClientIntegrationTest::client_;

/// @test Verify the basic insert operations for transaction commits.
TEST_F(ClientIntegrationTest, InsertAndCommit) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  auto rows = client_->Read("Singers", KeySet::All(),
                            {"SingerId", "FirstName", "LastName"});
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  std::vector<RowType> returned_rows;
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
    SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
    returned_rows.push_back(*std::move(row));
  }
  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2")));
}

/// @test Verify the basic delete mutations work.
TEST_F(ClientIntegrationTest, DeleteAndCommit) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  auto commit_result = client_->Commit(
      Mutations{MakeDeleteMutation("Singers", KeySet().AddKey(MakeKey(1)))});
  EXPECT_STATUS_OK(commit_result);

  auto rows = client_->Read("Singers", KeySet::All(),
                            {"SingerId", "FirstName", "LastName"});

  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  std::vector<RowType> returned_rows;
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
    SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
    returned_rows.push_back(*std::move(row));
  }
  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(2, "test-fname-2", "test-lname-2")));
}

/// @test Verify that read-write transactions with multiple statements work.
TEST_F(ClientIntegrationTest, MultipleInserts) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  auto& client = *client_;
  auto commit_result =
      client_->Commit([&client](Transaction const& txn) -> StatusOr<Mutations> {
        auto insert1 = client.ExecuteDml(
            txn,
            SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                         "VALUES (@id, @fname, @lname)",
                         {{"id", Value(3)},
                          {"fname", Value("test-fname-3")},
                          {"lname", Value("test-lname-3")}}));
        if (!insert1) return std::move(insert1).status();
        auto insert2 = client.ExecuteDml(
            txn,
            SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                         "VALUES (@id, @fname, @lname)",
                         {{"id", Value(4)},
                          {"fname", Value("test-fname-4")},
                          {"lname", Value("test-lname-4")}}));
        if (!insert2) return std::move(insert2).status();
        return Mutations{};
      });
  EXPECT_STATUS_OK(commit_result);

  auto rows = client_->Read("Singers", KeySet::All(),
                            {"SingerId", "FirstName", "LastName"});

  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  std::vector<RowType> returned_rows;
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
    SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
    returned_rows.push_back(*std::move(row));
  }
  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2"),
                                   RowType(3, "test-fname-3", "test-lname-3"),
                                   RowType(4, "test-fname-4", "test-lname-4")));
}

/// @test Verify that Client::Rollback works as expected.
TEST_F(ClientIntegrationTest, TransactionRollback) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  using RowType = std::tuple<std::int64_t, std::string, std::string>;

  // Cannot use Commit in this test because we want to call Rollback
  // explicitly. This means we need to retry ABORTED calls ourselves.
  Transaction txn = MakeReadWriteTransaction();
  for (auto start = std::chrono::steady_clock::now(),
            deadline = start + std::chrono::minutes(1);
       start < deadline; start = std::chrono::steady_clock::now()) {
    auto is_retryable_failure = [](StatusOr<DmlResult> const& s) {
      return !s && s.status().code() == StatusCode::kAborted;
    };

    // Share lock priority with the previous loop so that we have a slightly
    // better chance of avoiding StatusCode::kAborted from ExecuteDml().
    txn = MakeReadWriteTransaction(txn);

    auto insert1 = client_->ExecuteDml(
        txn, SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                          "VALUES (@id, @fname, @lname)",
                          {{"id", Value(3)},
                           {"fname", Value("test-fname-3")},
                           {"lname", Value("test-lname-3")}}));
    if (is_retryable_failure(insert1)) continue;
    ASSERT_STATUS_OK(insert1);

    auto insert2 = client_->ExecuteDml(
        txn, SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                          "VALUES (@id, @fname, @lname)",
                          {{"id", Value(4)},
                           {"fname", Value("test-fname-4")},
                           {"lname", Value("test-lname-4")}}));
    if (is_retryable_failure(insert2)) continue;
    ASSERT_STATUS_OK(insert2);

    auto rows = client_->Read(txn, "Singers", KeySet::All(),
                              {"SingerId", "FirstName", "LastName"});

    std::vector<RowType> returned_rows;
    int row_number = 0;
    Status iteration_status;
    for (auto& row : StreamOf<RowType>(rows)) {
      if (!row) {
        iteration_status = std::move(row).status();
        break;
      }
      SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
      returned_rows.push_back(*std::move(row));
    }
    if (iteration_status.code() == StatusCode::kAborted) continue;
    ASSERT_THAT(returned_rows, UnorderedElementsAre(
                                   RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2"),
                                   RowType(3, "test-fname-3", "test-lname-3"),
                                   RowType(4, "test-fname-4", "test-lname-4")));

    auto insert_rollback_result = client_->Rollback(txn);
    ASSERT_STATUS_OK(insert_rollback_result);
    break;
  }

  std::vector<RowType> returned_rows;
  auto rows = client_->Read("Singers", KeySet::All(),
                            {"SingerId", "FirstName", "LastName"});
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
    SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
    returned_rows.push_back(*std::move(row));
  }
  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2")));
}

/// @test Verify the basics of Commit().
TEST_F(ClientIntegrationTest, Commit) {
  // Insert SingerIds 100, 102, and 199.
  auto isb =
      InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
          .EmplaceRow(100, "first-name-100", "last-name-100")
          .EmplaceRow(102, "first-name-102", "last-name-102")
          .EmplaceRow(199, "first-name-199", "last-name-199");
  auto insert_result = client_->Commit(Mutations{isb.Build()});
  ASSERT_STATUS_OK(insert_result);
  EXPECT_NE(Timestamp{}, insert_result->commit_timestamp);

  // Delete SingerId 102.
  auto delete_result = client_->Commit(
      Mutations{MakeDeleteMutation("Singers", KeySet().AddKey(MakeKey(102)))});
  ASSERT_STATUS_OK(delete_result);
  EXPECT_LT(insert_result->commit_timestamp, delete_result->commit_timestamp);

  // Read SingerIds [100 ... 200).
  using RowType = std::tuple<std::int64_t>;
  std::vector<std::int64_t> ids;
  auto ks = KeySet().AddRange(MakeKeyBoundClosed(100), MakeKeyBoundOpen(200));
  auto rows = client_->Read("Singers", std::move(ks), {"SingerId"});
  for (auto const& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (row) ids.push_back(std::get<0>(*row));
  }
  EXPECT_THAT(ids, UnorderedElementsAre(100, 199));
}

/// @test Verify the basics of CommitAtLeastOnce().
TEST_F(ClientIntegrationTest, CommitAtLeastOnce) {
  // Insert SingerIds 200, 202, and 299.
  auto isb =
      InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
          .EmplaceRow(200, "first-name-200", "last-name-200")
          .EmplaceRow(202, "first-name-202", "last-name-202")
          .EmplaceRow(299, "first-name-299", "last-name-299");
  auto insert_result = client_->CommitAtLeastOnce(
      Transaction::ReadWriteOptions{}, Mutations{isb.Build()});
  if (insert_result) {
    EXPECT_NE(Timestamp{}, insert_result->commit_timestamp);
  } else {
    if (insert_result.status().code() == StatusCode::kAborted) {
      return;  // try another day
    }
    // A replay will make it look like the row already exists.
    EXPECT_THAT(insert_result, StatusIs(StatusCode::kAlreadyExists));
  }

  // Delete SingerId 202.
  auto delete_result = client_->CommitAtLeastOnce(
      Transaction::ReadWriteOptions{},
      Mutations{MakeDeleteMutation("Singers", KeySet().AddKey(MakeKey(202)))});
  if (delete_result) {
    EXPECT_LT(insert_result->commit_timestamp, delete_result->commit_timestamp);
  } else {
    if (delete_result.status().code() == StatusCode::kAborted) {
      return;  // try another day
    }
    // A replay will make it look like the row doesn't exist.
    EXPECT_THAT(delete_result, StatusIs(StatusCode::kNotFound));
  }

  // Read SingerIds [200 ... 300).
  using RowType = std::tuple<std::int64_t>;
  std::vector<std::int64_t> ids;
  auto ks = KeySet().AddRange(MakeKeyBoundClosed(200), MakeKeyBoundOpen(300));
  auto rows = client_->Read("Singers", std::move(ks), {"SingerId"});
  for (auto const& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (row) ids.push_back(std::get<0>(*row));
  }
  EXPECT_THAT(ids, UnorderedElementsAre(200, 299));
}

/// @test Verify the basics of batched CommitAtLeastOnce().
TEST_F(ClientIntegrationTest, CommitAtLeastOnceBatched) {
  auto const now = DatabaseNow();
  ASSERT_THAT(now, IsOk());

  std::string const table = "Singers";
  std::vector<std::string> const columns = {
      "SingerId", "FirstName", "LastName"  //
  };
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  std::vector<std::vector<std::vector<RowType>>> const groups = {
      // group #0
      {
          {
              {1894, "Bessie", "Smith"},
              {1901, "Louis", "Armstrong"},
              {1911, "Mahalia", "Jackson"},
          },
          {
              {1915, "Frank", "Sinatra"},
              {1923, "Hank", "Williams"},
          },
          {
              {1925, "Celia", "Cruz"},
          },
      },
      // group #1
      {
          {
              {1930, "Ray", "Charles"},
              {1931, "Sam", "Cooke"},
              {1932, "Patsy", "Cline"},
          },
          {
              {1933, "Nina", "Simone"},
              {1935, "Elvis", "Presley"},
          },
      },
      // group #2
      {
          {
              {1939, "Dusty", "Springfield"},
              {1940, "Smokey", "Robinson"},
          },
          {
              {1941, "Otis", "Redding"},
              {1942, "Aretha", "Franklin"},
          },
          {
              {1945, "Van", "Morrison"},
          },
      },
      // group #3
      {
          {
              {1946, "Freddie", "Mercury"},
          },
          {
              {1947, "David", "Bowie"},
          },
          {
              {1950, "Stevie", "Wonder"},
          },
          {
              {1951, "Luther", "Vandross"},
          },
          {
              {1953, "Chaka", "Khan"},
          },
      },
      // group #4
      {
          {
              {1958, "Prince", ""},
              {1963, "Whitney", "Houston"},
              {1967, "Kurt", "Cobain"},
              {1968, "Thom", "Yorke"},
              {1969, "Mariah", "Carey"},
              {1988, "Adele", ""},
          },
      },
  };
  std::vector<Mutations> mutation_groups;
  for (auto const& group : groups) {
    Mutations mutations;
    for (auto const& mutation : group) {
      // Use upserts as mutation groups are not replay protected.
      InsertOrUpdateMutationBuilder b(table, columns);
      for (auto const& row : mutation) {
        b.EmplaceRow(std::get<0>(row), std::get<1>(row), std::get<2>(row));
      }
      mutations.push_back(std::move(b).Build());
    }
    mutation_groups.push_back(std::move(mutations));
  }

  auto commit_results = client_->CommitAtLeastOnce(std::move(mutation_groups));
  std::map<std::size_t, StatusOr<Timestamp>> results;
  for (auto& commit_result : commit_results) {
    if (UsingEmulator()) {
      EXPECT_THAT(commit_result, StatusIs(StatusCode::kUnimplemented));
      EXPECT_THAT(results, IsEmpty());
      GTEST_SKIP();
    }
    ASSERT_THAT(commit_result, IsOk());
    EXPECT_THAT(commit_result->indexes, Not(IsEmpty()));
    for (auto index : commit_result->indexes) {
      ASSERT_THAT(index, Lt(groups.size()));
      auto ins = results.emplace(index, commit_result->commit_timestamp);
      EXPECT_TRUE(ins.second);  // Check that the indexes are unique.
    }
    if (commit_result->commit_timestamp) {
      // Check that we got a reasonable commit_timestamp.
      EXPECT_THAT(*commit_result->commit_timestamp, Ge(*now));
    }
  }

  // Check that all indexes are accounted for.
  for (std::size_t i = 0; i != groups.size(); ++i) {
    if (results.find(i) == results.end()) {
      ADD_FAILURE() << "Expected: " << i << ", actual: missing";
    }
  }

  // Snapshot the updated table.
  auto rows = client_->Read(table, KeySet::All(), columns);
  std::map<std::int64_t, RowType> singers;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_THAT(row, IsOk());
    if (!row) break;
    auto const singer_id = std::get<0>(*row);
    singers.emplace(singer_id, std::move(*row));
  }

  // Check that the (successful) mutations were applied.
  for (auto const& result : results) {
    if (!result.second) continue;  // skip failed groups
    for (auto const& mutation : groups[result.first]) {
      for (auto const& row : mutation) {
        auto const itr = singers.find(std::get<0>(row));
        if (itr == singers.end()) {
          ADD_FAILURE() << "Expected: " << testing::PrintToString(row)
                        << ", actual: missing";
        } else {
          EXPECT_THAT(itr->second, Eq(row));
        }
      }
    }
  }
}

/// @test Test various forms of ExecuteQuery() and ExecuteDml()
TEST_F(ClientIntegrationTest, ExecuteQueryDml) {
  auto& client = *client_;
  auto insert_result =
      client_->Commit([&client](Transaction txn) -> StatusOr<Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn), SqlStatement(R"sql(
        INSERT INTO Singers (SingerId, FirstName, LastName)
        VALUES (@id, @fname, @lname))sql",
                                         {{"id", Value(1)},
                                          {"fname", Value("test-fname-1")},
                                          {"lname", Value("test-lname-1")}}));
        if (!insert) return std::move(insert).status();
        return Mutations{};
      });
  EXPECT_STATUS_OK(insert_result);

  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  std::vector<RowType> expected_rows;
  auto commit_result = client_->Commit(
      [&client, &expected_rows](Transaction const& txn) -> StatusOr<Mutations> {
        expected_rows.clear();
        for (int i = 2; i != 10; ++i) {
          auto s = std::to_string(i);
          auto insert = client.ExecuteDml(
              txn, SqlStatement(R"sql(
        INSERT INTO Singers (SingerId, FirstName, LastName)
        VALUES (@id, @fname, @lname))sql",
                                {{"id", Value(i)},
                                 {"fname", Value("test-fname-" + s)},
                                 {"lname", Value("test-lname-" + s)}}));
          if (!insert) return std::move(insert).status();
          expected_rows.emplace_back(i, "test-fname-" + s, "test-lname-" + s);
        }

        auto delete_result =
            client.ExecuteDml(txn, SqlStatement(R"sql(
        DELETE FROM Singers WHERE SingerId = @id)sql",
                                                {{"id", Value(1)}}));
        if (!delete_result) return std::move(delete_result).status();

        return Mutations{};
      });
  EXPECT_STATUS_OK(commit_result);

  auto rows = client_->ExecuteQuery(
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers", {}));

  std::vector<RowType> actual_rows;
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
    SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
    actual_rows.push_back(*std::move(row));
  }
  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
}

TEST_F(ClientIntegrationTest, QueryOptionsWork) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  // An optimizer_version of "latest" should work.
  auto rows = client_->ExecuteQuery(
      SqlStatement("SELECT FirstName, LastName FROM Singers"),
      QueryOptions().set_optimizer_version("latest"));
  int row_count = 0;
  for (auto const& row : rows) {
    EXPECT_STATUS_OK(row);
    if (row) ++row_count;
  }
  EXPECT_EQ(2, row_count);

  // An invalid optimizer_version should produce an error.
  rows = client_->ExecuteQuery(
      SqlStatement("SELECT FirstName, LastName FROM Singers"),
      QueryOptions().set_optimizer_version("some-invalid-version"));
  row_count = 0;
  bool got_error = false;
  for (auto const& row : rows) {
    if (!row) {
      got_error = true;
      break;
    }
    ++row_count;
  }
  if (!UsingEmulator()) {
    EXPECT_TRUE(got_error) << "An invalid optimizer version should be an error";
    EXPECT_EQ(0, row_count);
  } else {
    EXPECT_FALSE(got_error) << "An invalid optimizer version should be OK";
    EXPECT_EQ(2, row_count);
  }
}

/// @test Test ExecutePartitionedDml
TEST_F(ClientIntegrationTest, ExecutePartitionedDml) {
  auto& client = *client_;
  auto insert_result =
      client_->Commit([&client](Transaction txn) -> StatusOr<Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn), SqlStatement(R"sql(
        INSERT INTO Singers (SingerId, FirstName, LastName)
        VALUES (@id, @fname, @lname))sql",
                                         {{"id", Value(1)},
                                          {"fname", Value("test-fname-1")},
                                          {"lname", Value("test-lname-1")}}));
        if (!insert) return std::move(insert).status();
        return Mutations{};
      });
  EXPECT_STATUS_OK(insert_result);

  auto result = client_->ExecutePartitionedDml(
      SqlStatement("UPDATE Singers SET LastName = 'test-only'"
                   " WHERE SingerId >= 1"));
  EXPECT_STATUS_OK(result);
}

void CheckReadWithOptions(
    Client client,
    std::function<Transaction::SingleUseOptions(CommitResult const&)> const&
        options_generator) {
  using RowValues = std::vector<Value>;
  std::vector<RowValues> expected_rows;
  auto commit = client.Commit(
      [&expected_rows](Transaction const&) -> StatusOr<Mutations> {
        expected_rows.clear();
        InsertMutationBuilder insert("Singers",
                                     {"SingerId", "FirstName", "LastName"});
        for (int i = 1; i != 10; ++i) {
          auto s = std::to_string(i);
          auto row = RowValues{Value(i), Value("test-fname-" + s),
                               Value("test-lname-" + s)};
          insert.AddRow(row);
          expected_rows.push_back(row);
        }
        return Mutations{std::move(insert).Build()};
      });
  EXPECT_STATUS_OK(commit);

  auto rows = client.Read(options_generator(*commit), "Singers", KeySet::All(),
                          {"SingerId", "FirstName", "LastName"});

  std::vector<RowValues> actual_rows;
  int row_number = 0;
  for (auto& row :
       StreamOf<std::tuple<std::int64_t, std::string, std::string>>(rows)) {
    SCOPED_TRACE("Reading row[" + std::to_string(row_number++) + "]");
    EXPECT_STATUS_OK(row);
    if (!row) break;
    std::vector<Value> v;
    v.emplace_back(std::get<0>(*row));
    v.emplace_back(std::get<1>(*row));
    v.emplace_back(std::get<2>(*row));
    actual_rows.push_back(std::move(v));
  }
  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
}

/// @test Test Read() with bounded staleness set by a timestamp.
TEST_F(ClientIntegrationTest, ReadBoundedStalenessTimestamp) {
  CheckReadWithOptions(*client_, [](CommitResult const& result) {
    return Transaction::SingleUseOptions(
        /*min_read_timestamp=*/result.commit_timestamp);
  });
}

/// @test Test Read() with bounded staleness set by duration.
TEST_F(ClientIntegrationTest, ReadBoundedStalenessDuration) {
  CheckReadWithOptions(*client_, [](CommitResult const&) {
    // We want a duration sufficiently recent to include the latest commit.
    return Transaction::SingleUseOptions(
        /*max_staleness=*/std::chrono::nanoseconds(1));
  });
}

/// @test Test Read() with exact staleness set to "all previous transactions".
TEST_F(ClientIntegrationTest, ReadExactStalenessLatest) {
  CheckReadWithOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions());
  });
}

/// @test Test Read() with exact staleness set by a timestamp.
TEST_F(ClientIntegrationTest, ReadExactStalenessTimestamp) {
  CheckReadWithOptions(*client_, [](CommitResult const& result) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*read_timestamp=*/result.commit_timestamp));
  });
}

/// @test Test Read() with exact staleness set by duration.
TEST_F(ClientIntegrationTest, ReadExactStalenessDuration) {
  CheckReadWithOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*exact_staleness=*/std::chrono::nanoseconds(0)));
  });
}

void CheckExecuteQueryWithSingleUseOptions(
    Client client,
    std::function<Transaction::SingleUseOptions(CommitResult const&)> const&
        options_generator) {
  using RowValues = std::vector<Value>;
  std::vector<RowValues> expected_rows;
  auto commit = client.Commit(
      [&expected_rows](Transaction const&) -> StatusOr<Mutations> {
        InsertMutationBuilder insert("Singers",
                                     {"SingerId", "FirstName", "LastName"});
        expected_rows.clear();
        for (int i = 1; i != 10; ++i) {
          auto s = std::to_string(i);
          auto row = RowValues{Value(i), Value("test-fname-" + s),
                               Value("test-lname-" + s)};
          insert.AddRow(row);
          expected_rows.push_back(row);
        }
        return Mutations{std::move(insert).Build()};
      });
  EXPECT_STATUS_OK(commit);

  auto rows = client.ExecuteQuery(
      options_generator(*commit),
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers"));

  std::vector<RowValues> actual_rows;
  int row_number = 0;
  for (auto& row :
       StreamOf<std::tuple<std::int64_t, std::string, std::string>>(rows)) {
    SCOPED_TRACE("Reading row[" + std::to_string(row_number++) + "]");
    EXPECT_STATUS_OK(row);
    if (!row) break;
    std::vector<Value> v;
    v.emplace_back(std::get<0>(*row));
    v.emplace_back(std::get<1>(*row));
    v.emplace_back(std::get<2>(*row));
    actual_rows.push_back(std::move(v));
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
}

/// @test Verify the `ReadLockMode` option is sent in the RPC by creating a
/// situation where a transaction A perfomrs a commit while a transaction B
/// performed one after tx A started.
TEST_F(ClientIntegrationTest, ReadLockModeOptionIsSent) {
  if (UsingEmulator()) {
    GTEST_SKIP() << "Optimistic locks not supported by emulator";
  }
  auto const singer_id = 101;

  auto mutation_helper = [singer_id](std::string new_name) {
    return Mutations{MakeInsertOrUpdateMutation(
        "Singers", {"SingerId", "FirstName"}, singer_id, new_name)};
  };

  // initial insert
  auto insert = client_->Commit(mutation_helper("InitialName"));
  ASSERT_STATUS_OK(insert);

  // transaction A will do a DML after another transaction has executed DML
  // If optimistic, we expect transaction A to be aborted
  auto test_helper =
      [&mutation_helper, singer_id](
          Transaction::ReadLockMode read_lock_mode) -> StatusOr<CommitResult> {
    // here we create tx A and confirm it can perform a read.
    auto tx_a =
        MakeReadWriteTransaction(Transaction::ReadWriteOptions(read_lock_mode));
    auto tx_a_read_result = client_->Read(
        tx_a, "Singers", KeySet().AddKey(MakeKey(singer_id)), {"SingerId"});
    for (auto row : StreamOf<std::tuple<std::int64_t>>(tx_a_read_result)) {
      EXPECT_STATUS_OK(row);
    }

    // now a separate tx b will perform a write operation before tx A is
    // finished.
    auto tx_b = client_->Commit(mutation_helper("FirstModifiedName"));
    EXPECT_STATUS_OK(tx_b);

    // depending on the read lock mode, the result of the next write operation
    // in tx A will vary.
    return client_->Commit(tx_a, mutation_helper("SecondModifiedName"));
  };

  auto optimistic_result = test_helper(Transaction::ReadLockMode::kOptimistic);
  auto pessimistic_result =
      test_helper(Transaction::ReadLockMode::kPessimistic);

  EXPECT_STATUS_OK(pessimistic_result);
  EXPECT_THAT(optimistic_result, StatusIs(StatusCode::kAborted));
}

/// @test Test ExecuteQuery() with bounded staleness set by a timestamp.
TEST_F(ClientIntegrationTest, ExecuteQueryBoundedStalenessTimestamp) {
  CheckExecuteQueryWithSingleUseOptions(
      *client_, [](CommitResult const& result) {
        return Transaction::SingleUseOptions(
            /*min_read_timestamp=*/result.commit_timestamp);
      });
}

/// @test Test ExecuteQuery() with bounded staleness set by duration.
TEST_F(ClientIntegrationTest, ExecuteQueryBoundedStalenessDuration) {
  CheckExecuteQueryWithSingleUseOptions(*client_, [](CommitResult const&) {
    // We want a duration sufficiently recent to include the latest commit.
    return Transaction::SingleUseOptions(
        /*max_staleness=*/std::chrono::nanoseconds(1));
  });
}

/// @test Test ExecuteQuery() with exact staleness set to "all previous
/// transactions".
TEST_F(ClientIntegrationTest, ExecuteQueryExactStalenessLatest) {
  CheckExecuteQueryWithSingleUseOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions());
  });
}

/// @test Test ExecuteQuery() with exact staleness set by a timestamp.
TEST_F(ClientIntegrationTest, ExecuteQueryExactStalenessTimestamp) {
  CheckExecuteQueryWithSingleUseOptions(
      *client_, [](CommitResult const& result) {
        return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
            /*read_timestamp=*/result.commit_timestamp));
      });
}

/// @test Test ExecuteQuery() with exact staleness set by duration.
TEST_F(ClientIntegrationTest, ExecuteQueryExactStalenessDuration) {
  CheckExecuteQueryWithSingleUseOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*exact_staleness=*/std::chrono::nanoseconds(0)));
  });
}

/// @test Test that a directed read within a read-write transaction fails.
TEST_F(ClientIntegrationTest, DirectedReadWithinReadWriteTransaction) {
  auto& client = *client_;
  auto commit =
      client_->Commit([&client](Transaction const& txn) -> StatusOr<Mutations> {
        auto rows =
            client.Read(txn, "Singers", KeySet::All(),
                        {"SingerId", "FirstName", "LastName"},
                        Options{}.set<DirectedReadOption>(IncludeReplicas(
                            {ReplicaSelection(ReplicaType::kReadOnly)},
                            /*auto_failover_disabled=*/true)));
        using RowType = std::tuple<std::int64_t, std::string, std::string>;
        for (auto& row : StreamOf<RowType>(rows)) {
          if (!row) return row.status();
        }
        return Mutations{};
      });
  EXPECT_THAT(commit, StatusIs(UsingEmulator() ? StatusCode::kFailedPrecondition
                                               : StatusCode::kInvalidArgument,
                               HasSubstr("Directed reads can only be performed "
                                         "in a read-only transaction")));
}

StatusOr<std::vector<std::vector<Value>>> AddSingerDataToTable(Client client) {
  std::vector<std::vector<Value>> expected_rows;
  auto commit = client.Commit(
      [&expected_rows](Transaction const&) -> StatusOr<Mutations> {
        expected_rows.clear();
        InsertMutationBuilder insert("Singers",
                                     {"SingerId", "FirstName", "LastName"});
        for (int i = 1; i != 10; ++i) {
          auto s = std::to_string(i);
          auto row = std::vector<Value>{Value(i), Value("test-fname-" + s),
                                        Value("test-lname-" + s)};
          insert.AddRow(row);
          expected_rows.push_back(row);
        }
        return Mutations{std::move(insert).Build()};
      });
  if (!commit.ok()) {
    return commit.status();
  }
  return expected_rows;
}

TEST_F(ClientIntegrationTest, PartitionRead) {
  auto expected_rows = AddSingerDataToTable(*client_);
  ASSERT_STATUS_OK(expected_rows);

  auto ro_transaction = MakeReadOnlyTransaction();
  auto read_partitions =
      client_->PartitionRead(ro_transaction, "Singers", KeySet::All(),
                             {"SingerId", "FirstName", "LastName"});
  ASSERT_STATUS_OK(read_partitions);

  std::vector<std::string> serialized_partitions;
  for (auto const& partition : *read_partitions) {
    auto serialized_partition = SerializeReadPartition(partition);
    ASSERT_STATUS_OK(serialized_partition);
    serialized_partitions.push_back(*serialized_partition);
  }

  std::vector<std::vector<Value>> actual_rows;
  int partition_number = 0;
  for (auto const& partition : serialized_partitions) {
    int row_number = 0;
    auto deserialized_partition = DeserializeReadPartition(partition);
    ASSERT_STATUS_OK(deserialized_partition);
    auto rows = client_->Read(*deserialized_partition);
    for (auto& row :
         StreamOf<std::tuple<std::int64_t, std::string, std::string>>(rows)) {
      SCOPED_TRACE("Reading partition[" + std::to_string(partition_number++) +
                   "] row[" + std::to_string(row_number++) + "]");
      EXPECT_STATUS_OK(row);
      if (!row) break;
      std::vector<Value> v;
      v.emplace_back(std::get<0>(*row));
      v.emplace_back(std::get<1>(*row));
      v.emplace_back(std::get<2>(*row));
      actual_rows.push_back(std::move(v));
    }
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(*expected_rows));
}

TEST_F(ClientIntegrationTest, PartitionQuery) {
  auto expected_rows = AddSingerDataToTable(*client_);
  ASSERT_STATUS_OK(expected_rows);

  auto ro_transaction = MakeReadOnlyTransaction();
  auto query_partitions = client_->PartitionQuery(
      ro_transaction,
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers"));
  ASSERT_STATUS_OK(query_partitions);

  std::vector<std::string> serialized_partitions;
  for (auto const& partition : *query_partitions) {
    auto serialized_partition = SerializeQueryPartition(partition);
    ASSERT_STATUS_OK(serialized_partition);
    serialized_partitions.push_back(*serialized_partition);
  }

  std::vector<std::vector<Value>> actual_rows;
  int partition_number = 0;
  for (auto const& partition : serialized_partitions) {
    int row_number = 0;
    auto deserialized_partition = DeserializeQueryPartition(partition);
    ASSERT_STATUS_OK(deserialized_partition);
    auto rows = client_->ExecuteQuery(*deserialized_partition);
    for (auto& row :
         StreamOf<std::tuple<std::int64_t, std::string, std::string>>(rows)) {
      SCOPED_TRACE("Reading partition[" + std::to_string(partition_number++) +
                   "] row[" + std::to_string(row_number++) + "]");
      EXPECT_STATUS_OK(row);
      if (!row) break;
      std::vector<Value> v;
      v.emplace_back(std::get<0>(*row));
      v.emplace_back(std::get<1>(*row));
      v.emplace_back(std::get<2>(*row));
      actual_rows.push_back(std::move(v));
    }
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(*expected_rows));
}

TEST_F(ClientIntegrationTest, ExecuteBatchDml) {
  auto statements = {
      SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                   "VALUES(1, 'Foo1', 'Bar1')"),
      SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                   "VALUES(2, 'Foo2', 'Bar2')"),
      SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                   "VALUES(3, 'Foo3', 'Bar3')"),
      SqlStatement("UPDATE Singers SET FirstName = \"FOO\" "
                   "WHERE FirstName = 'Foo1' or FirstName = 'Foo3'"),
  };

  auto& client = *client_;
  StatusOr<BatchDmlResult> batch_result;
  auto commit_result =
      client_->Commit([&client, &batch_result,
                       &statements](Transaction txn) -> StatusOr<Mutations> {
        batch_result = client.ExecuteBatchDml(std::move(txn), statements);
        if (!batch_result) return batch_result.status();
        if (!batch_result->status.ok()) return batch_result->status;
        return Mutations{};
      });

  EXPECT_STATUS_OK(commit_result);
  ASSERT_STATUS_OK(batch_result);
  EXPECT_STATUS_OK(batch_result->status);
  ASSERT_EQ(batch_result->stats.size(), 4);
  EXPECT_EQ(batch_result->stats[0].row_count, 1);
  EXPECT_EQ(batch_result->stats[1].row_count, 1);
  EXPECT_EQ(batch_result->stats[2].row_count, 1);
  EXPECT_EQ(batch_result->stats[3].row_count, 2);

  auto rows = client_->ExecuteQuery(SqlStatement(
      "SELECT SingerId, FirstName, LastName FROM Singers ORDER BY SingerId"));

  struct Expectation {
    std::int64_t id;
    std::string fname;
    std::string lname;
  };
  auto expected = std::vector<Expectation>{
      Expectation{1, "FOO", "Bar1"},
      Expectation{2, "Foo2", "Bar2"},
      Expectation{3, "FOO", "Bar3"},
  };
  std::size_t counter = 0;
  for (auto const& row :
       StreamOf<std::tuple<std::int64_t, std::string, std::string>>(rows)) {
    ASSERT_STATUS_OK(row);
    ASSERT_LT(counter, expected.size());
    EXPECT_EQ(std::get<0>(*row), expected[counter].id);
    EXPECT_EQ(std::get<1>(*row), expected[counter].fname);
    EXPECT_EQ(std::get<2>(*row), expected[counter].lname);
    ++counter;
  }
  EXPECT_EQ(counter, expected.size());
}

TEST_F(ClientIntegrationTest, ExecuteBatchDmlMany) {
  std::vector<SqlStatement> v;
  constexpr auto kBatchSize = 200;
  for (int i = 0; i < kBatchSize; ++i) {
    std::string const singer_id = std::to_string(i);
    std::string const first_name = "Foo" + singer_id;
    std::string const last_name = "Bar" + singer_id;
    std::string insert =
        "INSERT INTO Singers (SingerId, FirstName, LastName) Values(";
    insert += singer_id + ", '";
    insert += first_name + "', '";
    insert += last_name + "')";
    v.emplace_back(insert);
  }

  std::vector<SqlStatement> left(v.begin(), v.begin() + v.size() / 2);
  std::vector<SqlStatement> right(v.begin() + v.size() / 2, v.end());

  auto& client = *client_;
  StatusOr<BatchDmlResult> batch_result_left;
  StatusOr<BatchDmlResult> batch_result_right;
  auto commit_result =
      client_->Commit([&client, &batch_result_left, &batch_result_right, &left,
                       &right](Transaction txn) -> StatusOr<Mutations> {
        batch_result_left = client.ExecuteBatchDml(txn, left);
        if (!batch_result_left) return batch_result_left.status();
        if (!batch_result_left->status.ok()) return batch_result_left->status;

        batch_result_right = client.ExecuteBatchDml(std::move(txn), right);
        if (!batch_result_right) return batch_result_right.status();
        if (!batch_result_right->status.ok()) return batch_result_right->status;

        return Mutations{};
      });

  EXPECT_STATUS_OK(commit_result);

  ASSERT_STATUS_OK(batch_result_left);
  EXPECT_EQ(batch_result_left->stats.size(), left.size());
  EXPECT_STATUS_OK(batch_result_left->status);
  for (auto const& stats : batch_result_left->stats) {
    EXPECT_EQ(stats.row_count, 1);
  }

  ASSERT_STATUS_OK(batch_result_right);
  EXPECT_EQ(batch_result_right->stats.size(), right.size());
  EXPECT_STATUS_OK(batch_result_right->status);
  for (auto const& stats : batch_result_right->stats) {
    EXPECT_EQ(stats.row_count, 1);
  }

  auto rows = client_->ExecuteQuery(SqlStatement(
      "SELECT SingerId, FirstName, LastName FROM Singers ORDER BY SingerId"));

  auto counter = 0;
  for (auto const& row :
       StreamOf<std::tuple<std::int64_t, std::string, std::string>>(rows)) {
    ASSERT_STATUS_OK(row);
    std::string const singer_id = std::to_string(counter);
    std::string const first_name = "Foo" + singer_id;
    std::string const last_name = "Bar" + singer_id;
    EXPECT_EQ(std::get<0>(*row), counter);
    EXPECT_EQ(std::get<1>(*row), first_name);
    EXPECT_EQ(std::get<2>(*row), last_name);
    ++counter;
  }

  EXPECT_EQ(counter, kBatchSize);
}

TEST_F(ClientIntegrationTest, ExecuteBatchDmlFailure) {
  auto statements = {
      SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                   "VALUES(1, 'Foo1', 'Bar1')"),
      SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                   "VALUES(2, 'Foo2', 'Bar2')"),
      SqlStatement("INSERT OOPS SYNTAX ERROR"),
      SqlStatement("UPDATE Singers SET FirstName = 'FOO' "
                   "WHERE FirstName = 'Foo1' or FirstName = 'Foo3'"),
  };

  auto& client = *client_;
  StatusOr<BatchDmlResult> batch_result;
  auto commit_result =
      client_->Commit([&client, &batch_result,
                       &statements](Transaction txn) -> StatusOr<Mutations> {
        batch_result = client.ExecuteBatchDml(std::move(txn), statements);
        if (!batch_result) return batch_result.status();
        if (!batch_result->status.ok()) return batch_result->status;
        return Mutations{};
      });

  EXPECT_THAT(commit_result, Not(IsOk()));
  ASSERT_STATUS_OK(batch_result);
  EXPECT_THAT(batch_result->status, Not(IsOk()));
  ASSERT_EQ(batch_result->stats.size(), 2);
  EXPECT_EQ(batch_result->stats[0].row_count, 1);
  EXPECT_EQ(batch_result->stats[1].row_count, 1);
}

TEST_F(ClientIntegrationTest, AnalyzeSql) {
  auto txn = MakeReadOnlyTransaction();
  auto sql = SqlStatement(
      "SELECT * FROM Singers  "
      "WHERE FirstName = 'Foo1' OR FirstName = 'Foo3'");

  // This returns a ExecutionPlan without executing the query.
  auto plan = client_->AnalyzeSql(std::move(txn), std::move(sql));
  ASSERT_STATUS_OK(plan);
  EXPECT_GT(plan->plan_nodes_size(), 0);
}

TEST_F(ClientIntegrationTest, ProfileQuery) {
  auto txn = MakeReadOnlyTransaction();
  auto sql = SqlStatement("SELECT * FROM Singers");

  auto rows = client_->ProfileQuery(std::move(txn), std::move(sql));
  // Consume all the rows to make the profile info available.
  for (auto const& row : rows) {
    EXPECT_STATUS_OK(row);
  }

  auto stats = rows.ExecutionStats();
  ASSERT_TRUE(stats);
  EXPECT_GT(stats->size(), 0);

  auto plan = rows.ExecutionPlan();
  if (UsingEmulator()) {
    EXPECT_FALSE(plan);
  } else {
    ASSERT_TRUE(plan);
    EXPECT_GT(plan->plan_nodes_size(), 0);
  }
}

TEST_F(ClientIntegrationTest, ProfileDml) {
  auto& client = *client_;
  ProfileDmlResult profile_result;
  auto commit_result = client_->Commit(
      [&client, &profile_result](Transaction txn) -> StatusOr<Mutations> {
        auto sql = SqlStatement(
            "INSERT INTO Singers (SingerId, FirstName, LastName) "
            "VALUES(1, 'Foo1', 'Bar1')");
        auto dml_profile = client.ProfileDml(std::move(txn), std::move(sql));
        if (!dml_profile) return dml_profile.status();
        profile_result = std::move(*dml_profile);
        return Mutations{};
      });
  EXPECT_STATUS_OK(commit_result);

  EXPECT_EQ(1, profile_result.RowsModified());

  auto stats = profile_result.ExecutionStats();
  ASSERT_TRUE(stats);
  EXPECT_GT(stats->size(), 0);

  auto plan = profile_result.ExecutionPlan();
  ASSERT_TRUE(plan);
  EXPECT_GT(plan->plan_nodes_size(), 0);
}

/// @test Verify database_dialect is returned in information schema.
TEST_F(ClientIntegrationTest, DatabaseDialect) {
  auto rows = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT s.OPTION_VALUE
        FROM INFORMATION_SCHEMA.DATABASE_OPTIONS s
        WHERE s.OPTION_NAME = 'database_dialect'
      )"""));
  using RowType = std::tuple<std::string>;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_THAT(row, IsOk());
    if (!row) break;
    EXPECT_EQ("GOOGLE_STANDARD_SQL", std::get<0>(*row));
  }
}

/// @test Verify database_dialect is returned in information schema.
TEST_F(PgClientIntegrationTest, DatabaseDialect) {
  auto rows = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT s.OPTION_VALUE
        FROM INFORMATION_SCHEMA.DATABASE_OPTIONS s
        WHERE s.OPTION_NAME = 'database_dialect'
      )"""));
  using RowType = std::tuple<std::string>;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_THAT(row, IsOk());
    if (!row) break;
    EXPECT_EQ("POSTGRESQL", std::get<0>(*row));
  }
}

/// @test Verify use of database role to read data.
TEST_F(ClientIntegrationTest, FineGrainedAccessControl) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  std::vector<std::string> statements = {
      "CREATE ROLE Reader",
      "GRANT SELECT ON TABLE Singers TO ROLE Reader",
  };
  auto metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  if (UsingEmulator()) {
    EXPECT_THAT(metadata, StatusIs(StatusCode::kInternal));
    GTEST_SKIP();
  }
  ASSERT_STATUS_OK(metadata);

  // Connect to the database using the Reader role.
  auto client = Client(MakeConnection(
      GetDatabase(),
      google::cloud::Options{}.set<SessionCreatorRoleOption>("Reader")));
  auto rows = client.ExecuteQuery(
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers"));
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
  }

  statements = {
      "REVOKE SELECT ON TABLE Singers FROM ROLE Reader",
      "DROP ROLE Reader",
  };
  metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  ASSERT_STATUS_OK(metadata);
}

/// @test Verify use of database role to read data.
TEST_F(PgClientIntegrationTest, FineGrainedAccessControl) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  std::vector<std::string> statements = {
      "CREATE ROLE Reader",
      "GRANT SELECT ON TABLE Singers TO Reader",
  };
  auto metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  if (UsingEmulator()) {
    EXPECT_THAT(metadata, StatusIs(StatusCode::kFailedPrecondition));
    GTEST_SKIP();
  }
  ASSERT_STATUS_OK(metadata);

  // Connect to the database using the Reader role.
  auto client = Client(MakeConnection(
      GetDatabase(),
      google::cloud::Options{}.set<SessionCreatorRoleOption>("Reader")));
  auto rows = client.ExecuteQuery(
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers"));
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    if (!row) break;
  }

  statements = {
      "REVOKE SELECT ON TABLE Singers FROM Reader",
      "DROP ROLE Reader",
  };
  metadata =
      admin_client.UpdateDatabaseDdl(GetDatabase().FullName(), statements)
          .get();
  ASSERT_STATUS_OK(metadata);
}

/// @test Verify "FOREIGN KEY" "ON DELETE CASCADE".
TEST_F(ClientIntegrationTest, ForeignKeyDeleteCascade) {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  // CREATE TABLE with ON DELETE CASCADE.
  std::vector<std::string> statements;
  statements.emplace_back(R"""(
      CREATE TABLE Customers (
          CustomerId   INT64 NOT NULL,
          CustomerName STRING(62) NOT NULL
      ) PRIMARY KEY (CustomerId))""");
  statements.emplace_back(R"""(
      CREATE TABLE ShoppingCarts (
          CartId       INT64 NOT NULL,
          CustomerId   INT64 NOT NULL,
          CustomerName STRING(62) NOT NULL,
          CONSTRAINT FKShoppingCartsCustomerId
              FOREIGN KEY (CustomerId)
              REFERENCES Customers (CustomerId)
              ON DELETE CASCADE
      ) PRIMARY KEY (CartId))""");
  auto create_metadata =
      admin_client
          .UpdateDatabaseDdl(GetDatabase().FullName(), std::move(statements))
          .get();
  ASSERT_STATUS_OK(create_metadata);

  // ALTER TABLE with ON DELETE CASCADE.
  statements.clear();
  statements.emplace_back(R"""(
      ALTER TABLE ShoppingCarts
      ADD CONSTRAINT FKShoppingCartsCustomerName
          FOREIGN KEY (CustomerName)
          REFERENCES Customers (CustomerName)
          ON DELETE CASCADE)""");
  auto add_metadata =
      admin_client
          .UpdateDatabaseDdl(GetDatabase().FullName(), std::move(statements))
          .get();
  ASSERT_STATUS_OK(add_metadata);

  // Insert a row into the referenced table, and then a referencing row into
  // the referencing table.
  auto insert_commit = client_->Commit(
      Mutations{MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   1, "FKCustomer"),
                MakeInsertMutation("ShoppingCarts",
                                   {"CartId", "CustomerId", "CustomerName"},  //
                                   1, 1, "FKCustomer")});
  ASSERT_STATUS_OK(insert_commit);

  // Cannot insert a row into the referencing table when the key is not
  // present in the referenced table.
  auto bad_key = client_->Commit(Mutations{MakeInsertMutation(
      "ShoppingCarts", {"CartId", "CustomerId", "CustomerName"},  //
      2, 2, "FKCustomer")});
  EXPECT_THAT(bad_key, StatusIs(StatusCode::kFailedPrecondition,
                                AllOf(HasSubstr("FKShoppingCartsCustomerId"),
                                      HasSubstr("ShoppingCarts"))));

  // Delete a row in the referenced table. All rows referencing that key
  // from the referencing table will also be deleted.
  auto delete_commit = client_->Commit(
      Mutations{MakeDeleteMutation("Customers", KeySet().AddKey(MakeKey(1)))});
  ASSERT_STATUS_OK(delete_commit);
  auto carts =
      client_->ExecuteQuery(SqlStatement("SELECT CartId FROM ShoppingCarts"));
  for (auto& cart : StreamOf<std::tuple<std::int64_t>>(carts)) {
    EXPECT_THAT(cart, IsOk());
    if (!cart) break;
    EXPECT_FALSE(true) << "Unexpected cart " << std::get<0>(*cart);
  }

  // Conflicting operation: insert and delete referenced key within the
  // same mutation.
  auto conflict_commit = client_->Commit(
      Mutations{MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   1, "FKCustomer1"),
                MakeDeleteMutation("Customers", KeySet().AddKey(MakeKey(1)))});
  EXPECT_THAT(conflict_commit,
              StatusIs(StatusCode::kFailedPrecondition,
                       AllOf(HasSubstr("Cannot write"), HasSubstr("and delete"),
                             HasSubstr("same transaction"))));

  // Conflicting operation: reference foreign key in referencing table
  // and delete key from referenced table in the same mutation.
  auto reinsert_commit = client_->Commit(
      Mutations{MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   1, "FKCustomer1"),
                MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   2, "FKCustomer2"),
                MakeInsertMutation("ShoppingCarts",
                                   {"CartId", "CustomerId", "CustomerName"},  //
                                   1, 1, "FKCustomer1")});
  ASSERT_STATUS_OK(reinsert_commit);
  auto reconflict_commit = client_->Commit(
      Mutations{MakeInsertMutation("ShoppingCarts",
                                   {"CartId", "CustomerId", "CustomerName"},  //
                                   1, 2, "FKCustomer2"),
                MakeDeleteMutation("Customers", KeySet().AddKey(MakeKey(1)))});
  if (UsingEmulator()) {
    EXPECT_THAT(reconflict_commit, StatusIs(StatusCode::kAlreadyExists,
                                            HasSubstr("ShoppingCarts")));
  } else {
    EXPECT_THAT(reconflict_commit,
                StatusIs(StatusCode::kFailedPrecondition,
                         AllOf(HasSubstr("Cannot modify a row"),
                               HasSubstr("ShoppingCarts"),
                               HasSubstr("referential action is deleting it"),
                               HasSubstr("in the same transaction"))));
  }

  // Query INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS and validate DELETE_RULE.
  auto delete_rules = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT c.DELETE_RULE
        FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS c
        WHERE c.CONSTRAINT_NAME = "FKShoppingCartsCustomerId"
      )"""));
  using RowType = std::tuple<std::string>;
  for (auto& delete_rule : StreamOf<RowType>(delete_rules)) {
    EXPECT_THAT(delete_rule, IsOk());
    if (!delete_rule) break;
    EXPECT_EQ(UsingEmulator() ? "NO ACTION" : "CASCADE",
              std::get<0>(*delete_rule));
  }

  // Drop ON DELETE CASCADE constraint.
  statements.clear();
  statements.emplace_back(R"""(
      ALTER TABLE ShoppingCarts
      DROP CONSTRAINT FKShoppingCartsCustomerName)""");
  auto drop_metadata =
      admin_client
          .UpdateDatabaseDdl(GetDatabase().FullName(), std::move(statements))
          .get();
  ASSERT_STATUS_OK(drop_metadata);
}

/// @test Verify "FOREIGN KEY" "ON DELETE CASCADE".
TEST_F(PgClientIntegrationTest, ForeignKeyDeleteCascade) {
  spanner_admin::DatabaseAdminClient admin_client(
      spanner_admin::MakeDatabaseAdminConnection());

  // CREATE TABLE with ON DELETE CASCADE.
  std::vector<std::string> statements;
  statements.emplace_back(R"""(
      CREATE TABLE Customers (
          CustomerId   BIGINT NOT NULL PRIMARY KEY,
          CustomerName CHARACTER VARYING(62) NOT NULL
      ))""");
  statements.emplace_back(R"""(
      CREATE TABLE ShoppingCarts (
          CartId       BIGINT NOT NULL PRIMARY KEY,
          CustomerId   BIGINT NOT NULL,
          CustomerName CHARACTER VARYING(62) NOT NULL,
          CONSTRAINT FKShoppingCartsCustomerId
              FOREIGN KEY (CustomerId)
              REFERENCES Customers (CustomerId)
              ON DELETE CASCADE
      ))""");
  auto create_metadata =
      admin_client
          .UpdateDatabaseDdl(GetDatabase().FullName(), std::move(statements))
          .get();
  ASSERT_STATUS_OK(create_metadata);

  // ALTER TABLE with ON DELETE CASCADE.
  statements.clear();
  statements.emplace_back(R"""(
      ALTER TABLE ShoppingCarts
      ADD CONSTRAINT FKShoppingCartsCustomerName
          FOREIGN KEY (CustomerName)
          REFERENCES Customers (CustomerName)
          ON DELETE CASCADE)""");
  auto add_metadata =
      admin_client
          .UpdateDatabaseDdl(GetDatabase().FullName(), std::move(statements))
          .get();
  ASSERT_STATUS_OK(add_metadata);

  // Insert a row into the referenced table, and then a referencing row into
  // the referencing table.
  auto insert_commit = client_->Commit(
      Mutations{MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   1, "FKCustomer"),
                MakeInsertMutation("ShoppingCarts",
                                   {"CartId", "CustomerId", "CustomerName"},  //
                                   1, 1, "FKCustomer")});
  ASSERT_STATUS_OK(insert_commit);

  // Cannot insert a row into the referencing table when the key is not
  // present in the referenced table.
  auto bad_key = client_->Commit(Mutations{MakeInsertMutation(
      "ShoppingCarts", {"CartId", "CustomerId", "CustomerName"},  //
      2, 2, "FKCustomer")});
  EXPECT_THAT(bad_key, StatusIs(StatusCode::kFailedPrecondition,
                                AllOf(HasSubstr("fkshoppingcartscustomerid"),
                                      HasSubstr("shoppingcarts"))));

  // Delete a row in the referenced table. All rows referencing that key
  // from the referencing table will also be deleted.
  auto delete_commit = client_->Commit(
      Mutations{MakeDeleteMutation("Customers", KeySet().AddKey(MakeKey(1)))});
  ASSERT_STATUS_OK(delete_commit);
  auto carts =
      client_->ExecuteQuery(SqlStatement("SELECT CartId FROM ShoppingCarts"));
  for (auto& cart : StreamOf<std::tuple<std::int64_t>>(carts)) {
    EXPECT_THAT(cart, IsOk());
    if (!cart) break;
    EXPECT_FALSE(true) << "Unexpected cart " << std::get<0>(*cart);
  }

  // Conflicting operation: insert and delete referenced key within the
  // same mutation.
  auto conflict_commit = client_->Commit(
      Mutations{MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   1, "FKCustomer1"),
                MakeDeleteMutation("Customers", KeySet().AddKey(MakeKey(1)))});
  EXPECT_THAT(conflict_commit,
              StatusIs(StatusCode::kFailedPrecondition,
                       AllOf(HasSubstr("Cannot write"), HasSubstr("and delete"),
                             HasSubstr("same transaction"))));

  // Conflicting operation: reference foreign key in referencing table
  // and delete key from referenced table in the same mutation.
  auto reinsert_commit = client_->Commit(
      Mutations{MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   1, "FKCustomer1"),
                MakeInsertMutation("Customers", {"CustomerId", "CustomerName"},
                                   2, "FKCustomer2"),
                MakeInsertMutation("ShoppingCarts",
                                   {"CartId", "CustomerId", "CustomerName"},  //
                                   1, 1, "FKCustomer1")});
  ASSERT_STATUS_OK(reinsert_commit);
  auto reconflict_commit = client_->Commit(
      Mutations{MakeInsertMutation("ShoppingCarts",
                                   {"CartId", "CustomerId", "CustomerName"},  //
                                   1, 2, "FKCustomer2"),
                MakeDeleteMutation("Customers", KeySet().AddKey(MakeKey(1)))});
  if (UsingEmulator()) {
    EXPECT_THAT(reconflict_commit, StatusIs(StatusCode::kAlreadyExists,
                                            HasSubstr("shoppingcarts")));
  } else {
    EXPECT_THAT(reconflict_commit,
                StatusIs(StatusCode::kFailedPrecondition,
                         AllOf(HasSubstr("Cannot modify a row"),
                               HasSubstr("shoppingcarts"),
                               HasSubstr("referential action is deleting it"),
                               HasSubstr("in the same transaction"))));
  }

  // Query INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS and validate DELETE_RULE.
  auto delete_rules = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT c.DELETE_RULE
        FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS c
        WHERE c.CONSTRAINT_NAME = 'fkshoppingcartscustomerid'
      )"""));
  using RowType = std::tuple<std::string>;
  for (auto& delete_rule : StreamOf<RowType>(delete_rules)) {
    EXPECT_THAT(delete_rule, IsOk());
    if (!delete_rule) break;
    EXPECT_EQ(UsingEmulator() ? "NO ACTION" : "CASCADE",
              std::get<0>(*delete_rule));
  }

  // Drop ON DELETE CASCADE constraint.
  statements.clear();
  statements.emplace_back(R"""(
      ALTER TABLE ShoppingCarts
      DROP CONSTRAINT FKShoppingCartsCustomerName)""");
  auto drop_metadata =
      admin_client
          .UpdateDatabaseDdl(GetDatabase().FullName(), std::move(statements))
          .get();
  ASSERT_STATUS_OK(drop_metadata);
}

/// @test Verify version_retention_period is returned in information schema.
TEST_F(ClientIntegrationTest, VersionRetentionPeriod) {
  auto rows = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT s.OPTION_VALUE
        FROM INFORMATION_SCHEMA.DATABASE_OPTIONS s
        WHERE s.OPTION_NAME = "version_retention_period"
      )"""));
  using RowType = std::tuple<std::string>;
  for (auto& row : StreamOf<RowType>(rows)) {
    if (UsingEmulator()) {  // version_retention_period
      EXPECT_THAT(row, StatusIs(StatusCode::kInvalidArgument));
    } else {
      EXPECT_THAT(row, IsOk());
    }
    if (!row) break;
    EXPECT_EQ("2h", std::get<0>(*row));
  }
}

/// @test Verify default_leader is returned in information schema.
TEST_F(ClientIntegrationTest, DefaultLeader) {
  auto rows = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT s.OPTION_VALUE
        FROM INFORMATION_SCHEMA.DATABASE_OPTIONS s
        WHERE s.OPTION_NAME = "default_leader"
      )"""));
  using RowType = std::tuple<std::string>;
  for (auto& row : StreamOf<RowType>(rows)) {
    if (UsingEmulator()) {  // default_leader
      EXPECT_THAT(row, StatusIs(StatusCode::kInvalidArgument));
    } else {
      EXPECT_THAT(row, IsOk());
    }
    if (!row) break;
    // When the backend starts delivering rows for the table, we can
    // refine the column expectations beyond a non-empty leader.
    EXPECT_NE(std::get<0>(*row), "");
  }
}

/// @test Verify ability to enumerate SUPPORTED_OPTIMIZER_VERSIONS.
TEST_F(ClientIntegrationTest, SupportedOptimizerVersions) {
  auto rows = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT v.VERSION, v.RELEASE_DATE
        FROM SPANNER_SYS.SUPPORTED_OPTIMIZER_VERSIONS AS v
      )"""));
  using RowType = std::tuple<std::int64_t, absl::CivilDay>;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_THAT(row, IsOk());
    if (!row) break;
    EXPECT_GT(std::get<0>(*row), 0);
    EXPECT_GE(std::get<1>(*row), absl::CivilDay(1998, 9, 4));
  }
}

/// @test Verify ability to enumerate SPANNER_STATISTICS.
TEST_F(ClientIntegrationTest, SpannerStatistics) {
  auto rows = client_->ExecuteQuery(SqlStatement(R"""(
        SELECT s.SCHEMA_NAME, s.PACKAGE_NAME, s.ALLOW_GC
        FROM INFORMATION_SCHEMA.SPANNER_STATISTICS AS s
      )"""));
  using RowType = std::tuple<std::string, std::string, bool>;
  for (auto& row : StreamOf<RowType>(rows)) {
    if (UsingEmulator()) {
      EXPECT_THAT(row, StatusIs(AnyOf(StatusCode::kInvalidArgument,
                                      StatusCode::kUnimplemented)));
    } else {
      EXPECT_THAT(row, IsOk());
    }
    if (!row) break;
    // When the backend starts delivering rows for the table, we can
    // refine the column expectations beyond a non-empty package name.
    EXPECT_NE(std::get<1>(*row), "");
  }
}

/// @test Verify backwards compatibility for MakeConnection() arguments.
TEST_F(ClientIntegrationTest, MakeConnectionOverloads) {
  MakeConnection(GetDatabase(), ConnectionOptions());
  MakeConnection(GetDatabase(), ConnectionOptions(), SessionPoolOptions());
  MakeConnection(GetDatabase(), ConnectionOptions(), SessionPoolOptions(),
                 LimitedTimeRetryPolicy(std::chrono::minutes(25)).clone(),
                 ExponentialBackoffPolicy(std::chrono::seconds(2),
                                          std::chrono::minutes(10), 1.5)
                     .clone());
}

/// @test Verify the backwards compatibility `v1` namespace still exists.
TEST_F(ClientIntegrationTest, BackwardsCompatibility) {
  auto connection = ::google::cloud::spanner::v1::MakeConnection(GetDatabase());
  EXPECT_THAT(connection, NotNull());
  ASSERT_NO_FATAL_FAILURE(Client(std::move(connection)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
