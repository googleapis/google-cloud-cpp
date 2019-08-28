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
#include "google/cloud/spanner/testing/database_environment.h"
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

class ClientIntegrationTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    client_ = google::cloud::internal::make_unique<Client>(
        MakeConnection(spanner_testing::DatabaseEnvironment::GetDatabase()));
  }

  void SetUp() override {
    auto commit_result =
        RunTransaction(*client_, {}, [](Client const&, Transaction const&) {
          return Mutations{MakeDeleteMutation("Singers", KeySet::All())};
        });
    EXPECT_STATUS_OK(commit_result);
  }

  void InsertTwoSingers() {
    auto commit_result =
        RunTransaction(*client_, {}, [](Client const&, Transaction const&) {
          return Mutations{InsertMutationBuilder(
                               "Singers", {"SingerId", "FirstName", "LastName"})
                               .EmplaceRow(1, "test-fname-1", "test-lname-1")
                               .EmplaceRow(2, "test-fname-2", "test-lname-2")
                               .Build()};
        });
    ASSERT_STATUS_OK(commit_result);
  }

  static void TearDownTestSuite() { client_ = nullptr; }

 protected:
  static std::unique_ptr<Client> client_;
};

std::unique_ptr<Client> ClientIntegrationTest::client_;

/// @test Verify the basic insert operations for transaction commits.
TEST_F(ClientIntegrationTest, InsertAndCommit) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

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
              UnorderedElementsAre(RowType(1, "test-fname-1", "test-lname-1"),
                                   RowType(2, "test-fname-2", "test-lname-2")));
}

/// @test Verify the basic delete mutations work.
TEST_F(ClientIntegrationTest, DeleteAndCommit) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  auto commit_result =
      RunTransaction(*client_, {}, [](Client const&, Transaction const&) {
        return Mutations{
            MakeDeleteMutation("Singers", KeySetBuilder<Row<std::int64_t>>()
                                              .Add(MakeRow(std::int64_t(1)))
                                              .Build())};
      });
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
  EXPECT_THAT(returned_rows,
              UnorderedElementsAre(RowType(2, "test-fname-2", "test-lname-2")));
}

/// @test Verify that read-write transactions with multiple statements work.
TEST_F(ClientIntegrationTest, MultipleInserts) {
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  auto commit_result = RunTransaction(
      *client_, {},
      [](Client client, Transaction const& txn) -> StatusOr<Mutations> {
        auto insert1 = client.ExecuteSql(
            txn,
            SqlStatement("INSERT INTO Singers (SingerId, FirstName, LastName) "
                         "VALUES (@id, @fname, @lname)",
                         {{"id", Value(3)},
                          {"fname", Value("test-fname-3")},
                          {"lname", Value("test-lname-3")}}));
        if (!insert1) return std::move(insert1).status();
        auto insert2 = client.ExecuteSql(
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
  ASSERT_NO_FATAL_FAILURE(InsertTwoSingers());

  // Cannot use RunTransaction in this test because we want to call Rollback
  // explicitly.
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
  // Insert SingerIds 100, 102, and 199.
  auto inserter = [](Client const&, Transaction const&) {
    auto isb =
        InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"});
    isb.AddRow(MakeRow(100, "first-name-100", "last-name-100"));
    isb.AddRow(MakeRow(102, "first-name-102", "last-name-102"));
    isb.AddRow(MakeRow(199, "first-name-199", "last-name-199"));
    return Mutations{isb.Build()};
  };
  auto insert_result = RunTransaction(*client_, {}, inserter);
  EXPECT_STATUS_OK(insert_result);
  EXPECT_NE(Timestamp{}, insert_result->commit_timestamp);

  // Delete SingerId 102.
  auto deleter = [](Client const&, Transaction const&) {
    auto ksb = KeySetBuilder<Row<std::int64_t>>().Add(MakeRow(102));
    auto mutation = MakeDeleteMutation("Singers", ksb.Build());
    return Mutations{mutation};
  };
  auto delete_result = RunTransaction(*client_, {}, deleter);
  EXPECT_STATUS_OK(delete_result);
  EXPECT_LT(insert_result->commit_timestamp, delete_result->commit_timestamp);

  // Read SingerIds [100 ... 200).
  std::vector<std::int64_t> ids;
  auto ksb = KeySetBuilder<Row<std::int64_t>>().Add(MakeKeyRange(
      MakeKeyBoundClosed(MakeRow(100)), MakeKeyBoundOpen(MakeRow(200))));
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
  auto insert_result = RunTransaction(
      *client_, {}, [](Client client, Transaction txn) -> StatusOr<Mutations> {
        auto insert = client.ExecuteSql(
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

  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> expected_rows;
  auto commit_result = RunTransaction(
      *client_, {},
      [&expected_rows](Client client,
                       Transaction const& txn) -> StatusOr<Mutations> {
        expected_rows.clear();
        for (int i = 2; i != 10; ++i) {
          auto s = std::to_string(i);
          auto insert = client.ExecuteSql(
              txn, SqlStatement(R"sql(
        INSERT INTO Singers (SingerId, FirstName, LastName)
        VALUES (@id, @fname, @lname))sql",
                                {{"id", Value(i)},
                                 {"fname", Value("test-fname-" + s)},
                                 {"lname", Value("test-lname-" + s)}}));
          if (!insert) return std::move(insert).status();
          expected_rows.push_back(
              MakeRow(i, "test-fname-" + s, "test-lname-" + s));
        }

        auto delete_result =
            client.ExecuteSql(txn, SqlStatement(R"sql(
        DELETE FROM Singers WHERE SingerId = @id)sql",
                                                {{"id", Value(1)}}));
        if (!delete_result) return std::move(delete_result).status();

        return Mutations{};
      });
  ASSERT_STATUS_OK(commit_result);

  auto reader = client_->ExecuteSql(
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers", {}));
  EXPECT_STATUS_OK(reader);

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

void CheckReadWithOptions(
    Client client,
    std::function<Transaction::SingleUseOptions(CommitResult const&)> const&
        options_generator) {
  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> expected_rows;
  auto commit = RunTransaction(
      client, Transaction::ReadWriteOptions{},
      [&expected_rows](Client const&,
                       Transaction const&) -> StatusOr<Mutations> {
        expected_rows.clear();
        InsertMutationBuilder insert("Singers",
                                     {"SingerId", "FirstName", "LastName"});
        for (int i = 1; i != 10; ++i) {
          auto s = std::to_string(i);
          auto row = MakeRow(i, "test-fname-" + s, "test-lname-" + s);
          insert.AddRow(row);
          expected_rows.push_back(row);
        }
        return Mutations{std::move(insert).Build()};
      });
  ASSERT_STATUS_OK(commit);

  auto reader =
      client.Read(options_generator(*commit), "Singers", KeySet::All(),
                  {"SingerId", "FirstName", "LastName"});
  ASSERT_STATUS_OK(reader);

  std::vector<RowType> actual_rows;
  if (reader) {
    int row_number = 0;
    for (auto& row : reader->Rows<std::int64_t, std::string, std::string>()) {
      SCOPED_TRACE("Reading row[" + std::to_string(row_number++) + "]");
      EXPECT_STATUS_OK(row);
      if (!row) break;
      actual_rows.push_back(*std::move(row));
    }
  }
  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
}

/// @test Test Read() with bounded staleness set by a timestamp.
TEST_F(ClientIntegrationTest, Read_BoundedStaleness_Timestamp) {
  CheckReadWithOptions(*client_, [](CommitResult const& result) {
    return Transaction::SingleUseOptions(
        /*min_read_timestamp=*/result.commit_timestamp);
  });
}

/// @test Test Read() with bounded staleness set by duration.
TEST_F(ClientIntegrationTest, Read_BoundedStaleness_Duration) {
  CheckReadWithOptions(*client_, [](CommitResult const&) {
    // We want a duration sufficiently recent to include the latest commit.
    return Transaction::SingleUseOptions(
        /*max_staleness=*/std::chrono::nanoseconds(1));
  });
}

/// @test Test Read() with exact staleness set to "all previous transactions".
TEST_F(ClientIntegrationTest, Read_ExactStaleness_Latest) {
  CheckReadWithOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions());
  });
}

/// @test Test Read() with exact staleness set by a timestamp.
TEST_F(ClientIntegrationTest, Read_ExactStaleness_Timestamp) {
  CheckReadWithOptions(*client_, [](CommitResult const& result) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*read_timestamp=*/result.commit_timestamp));
  });
}

/// @test Test Read() with exact staleness set by duration.
TEST_F(ClientIntegrationTest, Read_ExactStaleness_Duration) {
  CheckReadWithOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*exact_staleness=*/Timestamp::duration(0)));
  });
}

void CheckExecuteSqlWithSingleUseOptions(
    Client client,
    std::function<Transaction::SingleUseOptions(CommitResult const&)> const&
        options_generator) {
  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> expected_rows;
  auto commit = RunTransaction(
      client, Transaction::ReadWriteOptions{},
      [&expected_rows](Client const&,
                       Transaction const&) -> StatusOr<Mutations> {
        InsertMutationBuilder insert("Singers",
                                     {"SingerId", "FirstName", "LastName"});
        for (int i = 1; i != 10; ++i) {
          auto s = std::to_string(i);
          auto row = MakeRow(i, "test-fname-" + s, "test-lname-" + s);
          insert.AddRow(row);
          expected_rows.push_back(row);
        }
        return Mutations{std::move(insert).Build()};
      });
  ASSERT_STATUS_OK(commit);

  auto reader = client.ExecuteSql(
      options_generator(*commit),
      SqlStatement("SELECT SingerId, FirstName, LastName FROM Singers"));
  ASSERT_STATUS_OK(reader);

  std::vector<RowType> actual_rows;
  if (reader) {
    int row_number = 0;
    for (auto& row : reader->Rows<std::int64_t, std::string, std::string>()) {
      SCOPED_TRACE("Reading row[" + std::to_string(row_number++) + "]");
      EXPECT_STATUS_OK(row);
      if (!row) break;
      actual_rows.push_back(*std::move(row));
    }
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(expected_rows));
}

/// @test Test ExecuteSql() with bounded staleness set by a timestamp.
TEST_F(ClientIntegrationTest, ExecuteSql_BoundedStaleness_Timestamp) {
  CheckExecuteSqlWithSingleUseOptions(*client_, [](CommitResult const& result) {
    return Transaction::SingleUseOptions(
        /*min_read_timestamp=*/result.commit_timestamp);
  });
}

/// @test Test ExecuteSql() with bounded staleness set by duration.
TEST_F(ClientIntegrationTest, ExecuteSql_BoundedStaleness_Duration) {
  CheckExecuteSqlWithSingleUseOptions(*client_, [](CommitResult const&) {
    // We want a duration sufficiently recent to include the latest commit.
    return Transaction::SingleUseOptions(
        /*max_staleness=*/std::chrono::nanoseconds(1));
  });
}

/// @test Test ExecuteSql() with exact staleness set to "all previous
/// transactions".
TEST_F(ClientIntegrationTest, ExecuteSql_ExactStaleness_Latest) {
  CheckExecuteSqlWithSingleUseOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions());
  });
}

/// @test Test ExecuteSql() with exact staleness set by a timestamp.
TEST_F(ClientIntegrationTest, ExecuteSql_ExactStaleness_Timestamp) {
  CheckExecuteSqlWithSingleUseOptions(*client_, [](CommitResult const& result) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*read_timestamp=*/result.commit_timestamp));
  });
}

/// @test Test ExecuteSql() with exact staleness set by duration.
TEST_F(ClientIntegrationTest, ExecuteSql_ExactStaleness_Duration) {
  CheckExecuteSqlWithSingleUseOptions(*client_, [](CommitResult const&) {
    return Transaction::SingleUseOptions(Transaction::ReadOnlyOptions(
        /*exact_staleness=*/Timestamp::duration(0)));
  });
}

StatusOr<std::vector<Row<std::int64_t, std::string, std::string>>>
AddSingerDataToTable(Client const& client) {
  std::vector<Row<std::int64_t, std::string, std::string>> expected_rows;
  auto commit = RunTransaction(
      client, Transaction::ReadWriteOptions{},
      [&expected_rows](Client const&,
                       Transaction const&) -> StatusOr<Mutations> {
        expected_rows.clear();
        InsertMutationBuilder insert("Singers",
                                     {"SingerId", "FirstName", "LastName"});
        for (int i = 1; i != 10; ++i) {
          auto s = std::to_string(i);
          auto row = MakeRow(i, "test-fname-" + s, "test-lname-" + s);
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
  for (auto& partition : *read_partitions) {
    auto serialized_partition = SerializeReadPartition(partition);
    ASSERT_STATUS_OK(serialized_partition);
    serialized_partitions.push_back(*serialized_partition);
  }

  std::vector<Row<std::int64_t, std::string, std::string>> actual_rows;
  int partition_number = 0;
  for (auto& partition : serialized_partitions) {
    int row_number = 0;
    auto deserialized_partition = DeserializeReadPartition(partition);
    ASSERT_STATUS_OK(deserialized_partition);
    auto partition_result_set = client_->Read(*deserialized_partition);
    ASSERT_STATUS_OK(partition_result_set);
    for (auto& row :
         partition_result_set->Rows<std::int64_t, std::string, std::string>()) {
      SCOPED_TRACE("Reading partition[" + std::to_string(partition_number++) +
                   "] row[" + std::to_string(row_number++) + "]");
      EXPECT_STATUS_OK(row);
      if (!row) break;
      actual_rows.push_back(*std::move(row));
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
      SqlStatement("select SingerId, FirstName, LastName from Singers"));
  ASSERT_STATUS_OK(query_partitions);

  std::vector<std::string> serialized_partitions;
  for (auto& partition : *query_partitions) {
    auto serialized_partition = SerializeQueryPartition(partition);
    ASSERT_STATUS_OK(serialized_partition);
    serialized_partitions.push_back(*serialized_partition);
  }

  std::vector<Row<std::int64_t, std::string, std::string>> actual_rows;
  int partition_number = 0;
  for (auto& partition : serialized_partitions) {
    int row_number = 0;
    auto deserialized_partition = DeserializeQueryPartition(partition);
    ASSERT_STATUS_OK(deserialized_partition);
    auto partition_result_set = client_->ExecuteSql(*deserialized_partition);
    ASSERT_STATUS_OK(partition_result_set);
    for (auto& row :
         partition_result_set->Rows<std::int64_t, std::string, std::string>()) {
      SCOPED_TRACE("Reading partition[" + std::to_string(partition_number++) +
                   "] row[" + std::to_string(row_number++) + "]");
      EXPECT_STATUS_OK(row);
      if (!row) break;
      actual_rows.push_back(*std::move(row));
    }
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(*expected_rows));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::google::cloud::testing_util::InitGoogleMock(argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::spanner_testing::DatabaseEnvironment());

  return RUN_ALL_TESTS();
}
