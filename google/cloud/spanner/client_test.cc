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
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

namespace spanner_proto = ::google::spanner::v1;

using ::google::cloud::spanner_mocks::MockConnection;
using ::google::cloud::spanner_mocks::MockResultSetSource;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;

TEST(ClientTest, CopyAndMove) {
  auto conn1 = std::make_shared<MockConnection>();
  auto conn2 = std::make_shared<MockConnection>();

  Client c1(conn1);
  Client c2(conn2);
  EXPECT_NE(c1, c2);

  // Copy construction
  Client c3 = c1;
  EXPECT_EQ(c3, c1);

  // Copy assignment
  c3 = c2;
  EXPECT_EQ(c3, c2);

  // Move construction
  Client c4 = std::move(c3);
  EXPECT_EQ(c4, c2);

  // Move assignment
  c1 = std::move(c4);
  EXPECT_EQ(c1, c2);
}

TEST(ClientTest, ReadSuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
      fields: {
        name: "Id",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Steve", 12)))
      .WillOnce(Return(MakeTestRow("Ann", 42)))
      .WillOnce(Return(Row()));

  EXPECT_CALL(*conn, Read(_))
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.Read("table", std::move(keys), {"column1", "column2"});

  using RowType = std::tuple<std::string, std::int64_t>;
  auto expected = std::vector<RowType>{
      RowType("Steve", 12),
      RowType("Ann", 42),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
}

TEST(ClientTest, ReadFailure) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Steve")))
      .WillOnce(Return(MakeTestRow("Ann")))
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "deadline!")));

  EXPECT_CALL(*conn, Read(_))
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.Read("table", std::move(keys), {"column1"});

  auto tups = StreamOf<std::tuple<std::string>>(rows);
  auto iter = tups.begin();
  EXPECT_NE(iter, tups.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Steve");

  ++iter;
  EXPECT_NE(iter, tups.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Ann");

  ++iter;
  EXPECT_FALSE((*iter).ok());
  EXPECT_EQ((*iter).status().code(), StatusCode::kDeadlineExceeded);
}

TEST(ClientTest, ExecuteQuerySuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
      fields: {
        name: "Id",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Steve", 12)))
      .WillOnce(Return(MakeTestRow("Ann", 42)))
      .WillOnce(Return(Row()));

  EXPECT_CALL(*conn, ExecuteQuery(_))
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.ExecuteQuery(SqlStatement("select * from table;"));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto expected = std::vector<RowType>{
      RowType("Steve", 12),
      RowType("Ann", 42),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
}

TEST(ClientTest, ExecuteQueryFailure) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Steve")))
      .WillOnce(Return(MakeTestRow("Ann")))
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "deadline!")));

  EXPECT_CALL(*conn, ExecuteQuery(_))
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.ExecuteQuery(SqlStatement("select * from table;"));

  auto tups = StreamOf<std::tuple<std::string>>(rows);
  auto iter = tups.begin();
  EXPECT_NE(iter, tups.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Steve");

  ++iter;
  EXPECT_NE(iter, tups.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Ann");

  ++iter;
  EXPECT_FALSE((*iter).ok());
  EXPECT_EQ((*iter).status().code(), StatusCode::kDeadlineExceeded);
}

TEST(ClientTest, ExecuteBatchDmlSuccess) {
  auto request = {
      SqlStatement("UPDATE Foo SET Bar = 1"),
      SqlStatement("UPDATE Foo SET Bar = 1"),
      SqlStatement("UPDATE Foo SET Bar = 1"),
  };

  BatchDmlResult result;
  result.stats = std::vector<BatchDmlResult::Stats>{
      {10},
      {10},
      {10},
  };

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, ExecuteBatchDml(_)).WillOnce(Return(result));

  Client client(conn);
  auto txn = MakeReadWriteTransaction();
  auto actual = client.ExecuteBatchDml(txn, request);

  EXPECT_STATUS_OK(actual);
  EXPECT_STATUS_OK(actual->status);
  EXPECT_EQ(actual->stats.size(), request.size());
}

TEST(ClientTest, ExecuteBatchDmlError) {
  auto request = {
      SqlStatement("UPDATE Foo SET Bar = 1"),
      SqlStatement("UPDATE Foo SET Bar = 1"),
      SqlStatement("UPDATE Foo SET Bar = 1"),
  };

  BatchDmlResult result;
  result.status = Status(StatusCode::kUnknown, "some error");
  result.stats = std::vector<BatchDmlResult::Stats>{
      {10},
      // Oops: Only one SqlStatement was processed, then "some error"
  };

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, ExecuteBatchDml(_)).WillOnce(Return(result));

  Client client(conn);
  auto txn = MakeReadWriteTransaction();
  auto actual = client.ExecuteBatchDml(txn, request);

  EXPECT_STATUS_OK(actual);
  EXPECT_FALSE(actual->status.ok());
  EXPECT_EQ(actual->status.message(), "some error");
  EXPECT_NE(actual->stats.size(), request.size());
  EXPECT_EQ(actual->stats.size(), 1);
}

TEST(ClientTest, ExecutePartitionedDmlSuccess) {
  auto source = absl::make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata metadata;
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow()).WillRepeatedly(Return(Row()));

  std::string const sql_statement = "UPDATE Singers SET MarketingBudget = 1000";
  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, ExecutePartitionedDml(_))
      .WillOnce([&sql_statement](
                    Connection::ExecutePartitionedDmlParams const& epdp) {
        EXPECT_EQ(sql_statement, epdp.statement.sql());
        return PartitionedDmlResult{7};
      });

  Client client(conn);
  auto result = client.ExecutePartitionedDml(SqlStatement(sql_statement));
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(7, result->row_count_lower_bound);
}

TEST(ClientTest, CommitSuccess) {
  auto conn = std::make_shared<MockConnection>();

  auto ts = MakeTimestamp(std::chrono::system_clock::from_time_t(123)).value();
  CommitResult result;
  result.commit_timestamp = ts;

  Client client(conn);
  EXPECT_CALL(*conn, Commit(_)).WillOnce(Return(result));

  auto txn = MakeReadWriteTransaction();
  auto commit = client.Commit(txn, {});
  EXPECT_STATUS_OK(commit);
  EXPECT_EQ(ts, commit->commit_timestamp);
}

TEST(ClientTest, CommitError) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "blah")));

  auto txn = MakeReadWriteTransaction();
  auto commit = client.Commit(txn, {});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("blah"));
}

TEST(ClientTest, RollbackSuccess) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Rollback(_)).WillOnce(Return(Status()));

  auto txn = MakeReadWriteTransaction();
  auto rollback = client.Rollback(txn);
  EXPECT_STATUS_OK(rollback);
}

TEST(ClientTest, RollbackError) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Rollback(_))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "oops")));

  auto txn = MakeReadWriteTransaction();
  auto rollback = client.Rollback(txn);
  EXPECT_EQ(StatusCode::kInvalidArgument, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("oops"));
}

TEST(ClientTest, MakeConnectionOptionalArguments) {
  Database db("foo", "bar", "baz");
  auto conn = MakeConnection(db);
  EXPECT_NE(conn, nullptr);

  conn = MakeConnection(db, ConnectionOptions());
  EXPECT_NE(conn, nullptr);

  conn = MakeConnection(db, ConnectionOptions(), SessionPoolOptions());
  EXPECT_NE(conn, nullptr);
}

TEST(ClientTest, CommitMutatorSuccess) {
  auto timestamp = internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // dummy
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {}, {}};
  Connection::CommitParams actual_commit_params{txn, {}};

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: STRING }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Bob")))
      .WillOnce(Return(Row()));

  EXPECT_CALL(*conn, Read(_))
      .WillOnce(DoAll(SaveArg<0>(&actual_read_params),
                      Return(ByMove(RowStream(std::move(source))))));
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce(DoAll(SaveArg<0>(&actual_commit_params),
                      Return(CommitResult{*timestamp})));

  Client client(conn);
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto mutator = [&client, &mutation](Transaction txn) -> StatusOr<Mutations> {
    auto rows = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    for (auto& row : StreamOf<std::tuple<std::string>>(rows)) {
      if (!row) return row.status();
    }
    return Mutations{mutation};
  };

  auto result = client.Commit(mutator);
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
  EXPECT_THAT(actual_commit_params.mutations, ElementsAre(mutation));
}

TEST(ClientTest, CommitMutatorRollback) {
  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // dummy
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {}, {}};

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  EXPECT_CALL(*conn, Read(_))
      .WillOnce(DoAll(SaveArg<0>(&actual_read_params),
                      Return(ByMove(RowStream(std::move(source))))));
  EXPECT_CALL(*conn, Rollback(_)).WillOnce(Return(Status()));

  Client client(conn);
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto mutator = [&client, &mutation](Transaction txn) -> StatusOr<Mutations> {
    auto rows = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    for (auto& row : rows) {
      if (!row) return row.status();
    }
    return Mutations{mutation};
  };

  auto result = client.Commit(mutator);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(StatusCode::kInvalidArgument, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("blah"));

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
}

TEST(ClientTest, CommitMutatorRollbackError) {
  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // dummy
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {}, {}};

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  EXPECT_CALL(*conn, Read(_))
      .WillOnce(DoAll(SaveArg<0>(&actual_read_params),
                      Return(ByMove(RowStream(std::move(source))))));
  EXPECT_CALL(*conn, Rollback(_))
      .WillOnce(Return(Status(StatusCode::kInternal, "oops")));

  Client client(conn);
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto mutator = [&client, &mutation](Transaction txn) -> StatusOr<Mutations> {
    auto rows = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    for (auto& row : rows) {
      if (!row) return row.status();
    }
    return Mutations{mutation};
  };

  auto result = client.Commit(mutator);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(StatusCode::kInvalidArgument, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("blah"));

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(ClientTest, CommitMutatorException) {
  auto conn = std::make_shared<MockConnection>();

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  EXPECT_CALL(*conn, Read(_))
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));
  EXPECT_CALL(*conn, Rollback(_)).WillOnce(Return(Status()));

  Client client(conn);
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto mutator = [&client, &mutation](Transaction txn) -> StatusOr<Mutations> {
    auto rows = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    for (auto& row : rows) {
      if (!row) throw "Read() error";
    }
    return Mutations{mutation};
  };

  try {
    auto result = client.Commit(mutator);
    FAIL();
  } catch (char const* e) {
    EXPECT_STREQ(e, "Read() error");
  } catch (...) {
    FAIL();
  }
}

TEST(ClientTest, CommitMutatorRuntimeStatusException) {
  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Rollback(_)).WillRepeatedly(Return(Status()));
  Client client(conn);
  try {
    auto result = client.Commit([](Transaction const&) -> StatusOr<Mutations> {
      throw RuntimeStatusError(Status());  // OK
    });
    EXPECT_EQ(StatusCode::kUnknown, result.status().code());
    EXPECT_THAT(result.status().message(), HasSubstr("OK Status thrown"));
  } catch (...) {
    FAIL();  // exception consumed
  }
  try {
    auto result = client.Commit([](Transaction const&) -> StatusOr<Mutations> {
      throw RuntimeStatusError(Status(StatusCode::kCancelled, "uh oh"));
    });
    EXPECT_EQ(StatusCode::kCancelled, result.status().code());
    EXPECT_EQ(result.status().message(), "uh oh");
  } catch (...) {
    FAIL();  // exception consumed
  }
}
#endif

TEST(ClientTest, CommitMutatorRerunTransientFailures) {
  auto timestamp = internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce([](Connection::CommitParams const&) {
        return Status(StatusCode::kAborted, "Aborted transaction");
      })
      .WillOnce([&timestamp](Connection::CommitParams const&) {
        return CommitResult{*timestamp};
      });

  auto mutator = [](Transaction const&) -> StatusOr<Mutations> {
    return Mutations{MakeDeleteMutation("table", KeySet::All())};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitMutatorTooManyFailures) {
  int commit_attempts = 0;
  int const maximum_failures = 2;

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit(_))
      .WillRepeatedly([&commit_attempts](Connection::CommitParams const&) {
        ++commit_attempts;
        return Status(StatusCode::kAborted, "Aborted transaction");
      });

  auto mutator = [](Transaction const&) -> StatusOr<Mutations> {
    return Mutations{MakeDeleteMutation("table", KeySet::All())};
  };

  Client client(conn);
  // Use a rerun policy with a limited number of errors, or this will wait for a
  // long time, also change the backoff policy to sleep for very short periods,
  // so the unit tests run faster.
  auto result = client.Commit(
      mutator,
      LimitedErrorCountTransactionRerunPolicy(maximum_failures).clone(),
      ExponentialBackoffPolicy(std::chrono::microseconds(10),
                               std::chrono::microseconds(10), 2.0)
          .clone());
  EXPECT_EQ(StatusCode::kAborted, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("Aborted transaction"));
  EXPECT_EQ(maximum_failures + 1, commit_attempts);  // one too many
}

TEST(ClientTest, CommitMutatorPermanentFailure) {
  int commit_attempts = 0;

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce([&commit_attempts](Connection::CommitParams const&) {
        ++commit_attempts;
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  auto mutator = [](Transaction const&) -> StatusOr<Mutations> {
    return Mutations{MakeDeleteMutation("table", KeySet::All())};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("uh-oh"));
  EXPECT_EQ(1, commit_attempts);  // no reruns
}

TEST(ClientTest, CommitMutations) {
  auto conn = std::make_shared<MockConnection>();
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto timestamp = internal::TimestampFromRFC3339("2020-02-28T04:49:17.335Z");
  ASSERT_STATUS_OK(timestamp);
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce([&mutation, &timestamp](Connection::CommitParams const& cp) {
        EXPECT_EQ(cp.mutations, Mutations{mutation});
        return CommitResult{*timestamp};
      });

  Client client(conn);
  auto result = client.Commit({mutation});
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

MATCHER(DoesNotHaveSession, "not bound to a session") {
  return internal::Visit(
      arg,
      [&](internal::SessionHolder& session,
          StatusOr<google::spanner::v1::TransactionSelector>&, std::int64_t) {
        if (session) {
          *result_listener << "has session " << session->session_name();
          return false;
        }
        return true;
      });
}

MATCHER_P(HasSession, name, "bound to expected session") {
  return internal::Visit(
      arg,
      [&](internal::SessionHolder& session,
          StatusOr<google::spanner::v1::TransactionSelector>&, std::int64_t) {
        if (!session) {
          *result_listener << "has no session but expected " << name;
          return false;
        }
        if (session->session_name() != name) {
          *result_listener << "has session " << session->session_name()
                           << " but expected " << name;
          return false;
        }
        return true;
      });
}

MATCHER(HasBegin, "not bound to a transaction-id nor invalidated") {
  return internal::Visit(
      arg,
      [&](internal::SessionHolder&,
          StatusOr<google::spanner::v1::TransactionSelector>& s, std::int64_t) {
        if (!s) {
          *result_listener << "has status " << s.status();
          return false;
        }
        if (!s->has_begin()) {
          if (s->has_single_use()) {
            *result_listener << "is single-use";
          } else {
            *result_listener << "has transaction-id " << s->id();
          }
          return false;
        }
        return true;
      });
}

bool SetSessionName(Transaction const& txn, std::string name) {
  return internal::Visit(
      txn, [&name](internal::SessionHolder& session,
                   StatusOr<google::spanner::v1::TransactionSelector>&,
                   std::int64_t) {
        session = internal::MakeDissociatedSessionHolder(std::move(name));
        return true;
      });
}

bool SetTransactionId(Transaction const& txn, std::string id) {
  return internal::Visit(
      txn, [&id](internal::SessionHolder&,
                 StatusOr<google::spanner::v1::TransactionSelector>& s,
                 std::int64_t) {
        s->set_id(std::move(id));  // only valid when s.ok()
        return true;
      });
}

TEST(ClientTest, CommitMutatorSessionAffinity) {
  int const num_aborts = 10;  // how many aborts before success

  // After assigning a session during the first aborted transaction, we
  // should see the same session in a new transaction on every rerun.
  std::string const session_name = "CommitMutatorLockPriority.Session";

  auto timestamp = internal::TimestampFromRFC3339("2019-11-11T20:05:36.345Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  // Eventually the Commit() will succeed.
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce(
          [&session_name, &timestamp](Connection::CommitParams const& cp) {
            EXPECT_THAT(cp.transaction, HasSession(session_name));
            EXPECT_THAT(cp.transaction, HasBegin());
            SetTransactionId(cp.transaction, "last-transaction-id");
            return CommitResult{*timestamp};
          });
  // But only after some aborts, the first of which sets the session.
  EXPECT_CALL(*conn, Commit(_))
      .Times(num_aborts)
      .WillOnce([&session_name](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, DoesNotHaveSession());
        EXPECT_THAT(cp.transaction, HasBegin());
        SetSessionName(cp.transaction, session_name);
        SetTransactionId(cp.transaction, "first-transaction-id");
        return Status(StatusCode::kAborted, "Aborted transaction");
      })
      .WillRepeatedly([&session_name](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession(session_name));
        EXPECT_THAT(cp.transaction, HasBegin());
        SetTransactionId(cp.transaction, "mid-transaction-id");
        return Status(StatusCode::kAborted, "Aborted transaction");
      })
      .RetiresOnSaturation();

  Client client(conn);
  auto const zero_duration = std::chrono::microseconds(0);
  auto result = client.Commit(
      [](Transaction const&) { return Mutations{}; },
      LimitedErrorCountTransactionRerunPolicy(num_aborts).clone(),
      ExponentialBackoffPolicy(zero_duration, zero_duration, 2).clone());
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitMutatorSessionNotFound) {
  auto timestamp = internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce([&timestamp](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession("session-3"));
        return CommitResult{*timestamp};
      });

  int n = 0;
  auto mutator = [&n](Transaction const& txn) -> StatusOr<Mutations> {
    EXPECT_THAT(txn, DoesNotHaveSession());
    SetSessionName(txn, "session-" + std::to_string(++n));
    if (n < 3) return Status(StatusCode::kNotFound, "Session not found");
    return Mutations{};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitSessionNotFound) {
  auto timestamp = internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce([](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession("session-1"));
        return Status(StatusCode::kNotFound, "Session not found");
      })
      .WillOnce([&timestamp](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession("session-2"));
        return CommitResult{*timestamp};
      });

  int n = 0;
  auto mutator = [&n](Transaction const& txn) -> StatusOr<Mutations> {
    EXPECT_THAT(txn, DoesNotHaveSession());
    SetSessionName(txn, "session-" + std::to_string(++n));
    return Mutations{};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, ProfileQuerySuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText0 = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
      fields: {
        name: "Id",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText0, &metadata));
  auto constexpr kText1 = R"pb(
    query_plan: { plan_nodes: { display_name: "test-node" } }
    query_stats {
      fields {
        key: "elapsed_time"
        value { string_value: "42 secs" }
      }
    }
  )pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText1, &stats));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Ann", 42)))
      .WillOnce(Return(Row()));
  EXPECT_CALL(*source, Stats()).WillRepeatedly(Return(stats));

  EXPECT_CALL(*conn, ProfileQuery(_))
      .WillOnce(Return(ByMove(ProfileQueryResult(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.ProfileQuery(SqlStatement("select * from table;"));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto expected = std::vector<RowType>{
      RowType("Ann", 42),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
  auto actual_plan = rows.ExecutionPlan();
  ASSERT_TRUE(actual_plan);
  EXPECT_THAT(*actual_plan, IsProtoEqual(stats.query_plan()));

  auto actual_stats = rows.ExecutionStats();
  ASSERT_TRUE(actual_stats);
  std::unordered_map<std::string, std::string> expected_stats{
      {"elapsed_time", "42 secs"}};
  EXPECT_EQ(expected_stats, *actual_stats);
}

TEST(ClientTest, ProfileQueryWithOptionsSuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText0 = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
      fields: {
        name: "Id",
        type: { code: INT64 }
      }
    }
  )pb";
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText0, &metadata));
  auto constexpr kText1 = R"pb(
    query_plan: { plan_nodes: { display_name: "test-node" } }
    query_stats {
      fields {
        key: "elapsed_time"
        value { string_value: "42 secs" }
      }
    }
  )pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText1, &stats));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(MakeTestRow("Ann", 42)))
      .WillOnce(Return(Row()));
  EXPECT_CALL(*source, Stats()).WillRepeatedly(Return(stats));

  EXPECT_CALL(*conn, ProfileQuery(_))
      .WillOnce(Return(ByMove(ProfileQueryResult(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.ProfileQuery(
      Transaction::SingleUseOptions(
          /*max_staleness=*/std::chrono::nanoseconds(std::chrono::minutes(5))),
      SqlStatement("select * from table;"));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto expected = std::vector<RowType>{
      RowType("Ann", 42),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());

  auto actual_plan = rows.ExecutionPlan();
  ASSERT_TRUE(actual_plan);
  EXPECT_THAT(*actual_plan, IsProtoEqual(stats.query_plan()));

  auto actual_stats = rows.ExecutionStats();
  ASSERT_TRUE(actual_stats);
  std::unordered_map<std::string, std::string> expected_stats{
      {"elapsed_time", "42 secs"}};
  EXPECT_EQ(expected_stats, *actual_stats);
}

TEST(ClientTest, QueryOptionsOverlayPrecedence) {
  struct Levels {
    absl::optional<std::string> env;
    absl::optional<std::string> client;
    absl::optional<std::string> function;
    absl::optional<std::string> expected;
  };
  std::vector<Levels> levels = {
      {{}, {}, {}, {}},
      {"env", {}, {}, "env"},
      {{}, "client", {}, "client"},
      {{}, {}, "function", "function"},
      {"env", "client", {}, "client"},
      {"env", {}, "function", "function"},
      {{}, "client", "function", "function"},
      {"env", "client", "function", "function"},
  };

  auto constexpr kQueryOptionsField = &Connection::SqlParams::query_options;
  auto constexpr kEnvName = "SPANNER_OPTIMIZER_VERSION";
  auto conn = std::make_shared<MockConnection>();
  for (auto const& level : levels) {
    google::cloud::testing_util::ScopedEnvironment env(kEnvName, level.env);
    auto client_qo = QueryOptions().set_optimizer_version(level.client);
    Client client(conn, ClientOptions().set_query_options(client_qo));
    auto expected = QueryOptions().set_optimizer_version(level.expected);
    EXPECT_CALL(*conn, ExecuteQuery(Field(kQueryOptionsField, Eq(expected))))
        .Times(AnyNumber());

    auto const qo = QueryOptions().set_optimizer_version(level.function);
    auto const ro = Transaction::ReadOnlyOptions{};
    auto const su = Transaction::SingleUseOptions{ro};

    // Call all the overloads that accept QueryOptions to ensure they all work.
    client.ExecuteQuery(SqlStatement{}, qo);
    client.ExecuteQuery(su, SqlStatement{}, qo);
    client.ExecuteQuery(Transaction{ro}, SqlStatement{}, qo);
    client.ExecuteQuery(QueryPartition{}, qo);

    client.ProfileQuery(SqlStatement{}, qo);
    client.ProfileQuery(su, SqlStatement{}, qo);
    client.ProfileQuery(Transaction{ro}, SqlStatement{}, qo);

    client.ExecuteDml(Transaction{ro}, SqlStatement{}, qo);
    client.ProfileDml(Transaction{ro}, SqlStatement{}, qo);
    client.AnalyzeSql(Transaction{ro}, SqlStatement{}, qo);
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
