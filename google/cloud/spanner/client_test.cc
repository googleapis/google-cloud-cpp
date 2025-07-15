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

#include "google/cloud/spanner/client.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/testing/status_utils.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::spanner_mocks::MockConnection;
using ::google::cloud::spanner_mocks::MockResultSetSource;
using ::google::cloud::spanner_testing::SessionNotFoundError;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

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
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));

  EXPECT_CALL(*conn, Read)
      .WillOnce([&metadata](Connection::ReadParams const& params) {
        EXPECT_THAT(
            params.directed_read_option,
            VariantWith<IncludeReplicas>(AllOf(
                Property(&IncludeReplicas::replica_selections,
                         ElementsAre(ReplicaSelection(ReplicaType::kReadOnly))),
                Property(&IncludeReplicas::auto_failover_disabled, true))));
        auto source = std::make_unique<MockResultSetSource>();
        EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
        EXPECT_CALL(*source, NextRow())
            .WillOnce(Return(spanner_mocks::MakeRow("Steve", 12)))
            .WillOnce(Return(spanner_mocks::MakeRow("Ann", 42)))
            .WillOnce(Return(Row()));
        return RowStream(std::move(source));
      });

  KeySet keys = KeySet::All();
  auto rows = client.Read("table", std::move(keys), {"column1", "column2"},
                          Options{}.set<DirectedReadOption>(IncludeReplicas(
                              {ReplicaSelection(ReplicaType::kReadOnly)},
                              /*auto_failover_disabled=*/true)));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto stream = StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType("Steve", 12)),
                                  IsOkAndHolds(RowType("Ann", 42))));
}

TEST(ClientTest, ReadWithLockHint) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

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
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));

  EXPECT_CALL(*conn, Read)
      .WillOnce([&metadata](Connection::ReadParams const& params) {
        EXPECT_THAT(
            params.directed_read_option,
            VariantWith<IncludeReplicas>(AllOf(
                Property(&IncludeReplicas::replica_selections,
                         ElementsAre(ReplicaSelection(ReplicaType::kReadOnly))),
                Property(&IncludeReplicas::auto_failover_disabled, true))));
        EXPECT_THAT(params.lock_hint, Eq(LockHint::kLockHintShared));
        auto source = std::make_unique<MockResultSetSource>();
        EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
        EXPECT_CALL(*source, NextRow())
            .WillOnce(Return(spanner_mocks::MakeRow("Steve", 12)))
            .WillOnce(Return(spanner_mocks::MakeRow("Ann", 42)))
            .WillOnce(Return(Row()));
        return RowStream(std::move(source));
      });

  KeySet keys = KeySet::All();
  auto rows = client.Read("table", std::move(keys), {"column1", "column2"},
                          Options{}
                              .set<DirectedReadOption>(IncludeReplicas(
                                  {ReplicaSelection(ReplicaType::kReadOnly)},
                                  /*auto_failover_disabled=*/true))
                              .set<LockHintOption>(LockHint::kLockHintShared));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto stream = StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType("Steve", 12)),
                                  IsOkAndHolds(RowType("Ann", 42))));
}

TEST(ClientTest, ReadFailure) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = std::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow("Steve")))
      .WillOnce(Return(spanner_mocks::MakeRow("Ann")))
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "deadline!")));

  EXPECT_CALL(*conn, Read)
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.Read("table", std::move(keys), {"column1"});

  auto tups = StreamOf<std::tuple<std::string>>(rows);
  auto iter = tups.begin();
  EXPECT_NE(iter, tups.end());
  ASSERT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Steve");

  ++iter;
  EXPECT_NE(iter, tups.end());
  ASSERT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Ann");

  ++iter;
  EXPECT_THAT(*iter, StatusIs(StatusCode::kDeadlineExceeded));
}

TEST(ClientTest, ExecuteQuerySuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

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
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));

  EXPECT_CALL(*conn, ExecuteQuery)
      .WillOnce([&metadata](Connection::SqlParams const& params) {
        EXPECT_THAT(
            params.directed_read_option,
            VariantWith<IncludeReplicas>(AllOf(
                Property(&IncludeReplicas::replica_selections,
                         ElementsAre(ReplicaSelection("us-east4"))),
                Property(&IncludeReplicas::auto_failover_disabled, false))));
        auto source = std::make_unique<MockResultSetSource>();
        EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
        EXPECT_CALL(*source, NextRow())
            .WillOnce(Return(spanner_mocks::MakeRow("Steve", 12)))
            .WillOnce(Return(spanner_mocks::MakeRow("Ann", 42)))
            .WillOnce(Return(Row()));
        return RowStream(std::move(source));
      });

  KeySet keys = KeySet::All();
  auto rows =
      client.ExecuteQuery(SqlStatement("SELECT * FROM Table;"),
                          Options{}.set<DirectedReadOption>(IncludeReplicas(
                              {ReplicaSelection("us-east4")},
                              /*auto_failover_disabled=*/false)));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto stream = StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType("Steve", 12)),
                                  IsOkAndHolds(RowType("Ann", 42))));
}

TEST(ClientTest, ExecuteQueryFailure) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = std::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow("Steve")))
      .WillOnce(Return(spanner_mocks::MakeRow("Ann")))
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "deadline!")));

  EXPECT_CALL(*conn, ExecuteQuery)
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.ExecuteQuery(SqlStatement("SELECT * FROM Table;"));

  auto tups = StreamOf<std::tuple<std::string>>(rows);
  auto iter = tups.begin();
  EXPECT_NE(iter, tups.end());
  ASSERT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Steve");

  ++iter;
  EXPECT_NE(iter, tups.end());
  ASSERT_STATUS_OK(*iter);
  EXPECT_EQ(std::get<0>(**iter), "Ann");

  ++iter;
  EXPECT_THAT(*iter, StatusIs(StatusCode::kDeadlineExceeded));
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
  EXPECT_CALL(*conn, ExecuteBatchDml).WillOnce(Return(result));

  Client client(conn);
  auto txn = MakeReadWriteTransaction();
  auto actual = client.ExecuteBatchDml(txn, request);

  ASSERT_STATUS_OK(actual);
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
  EXPECT_CALL(*conn, ExecuteBatchDml).WillOnce(Return(result));

  Client client(conn);
  auto txn = MakeReadWriteTransaction();
  auto actual = client.ExecuteBatchDml(txn, request);

  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(actual->status, StatusIs(StatusCode::kUnknown, "some error"));
  EXPECT_NE(actual->stats.size(), request.size());
  EXPECT_EQ(actual->stats.size(), 1);
}

TEST(ClientTest, ExecutePartitionedDmlSuccess) {
  auto source = std::make_unique<MockResultSetSource>();
  google::spanner::v1::ResultSetMetadata metadata;
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow()).WillRepeatedly(Return(Row()));

  std::string const sql_statement = "UPDATE Singers SET MarketingBudget = 1000";
  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, ExecutePartitionedDml)
      .WillOnce([&sql_statement](
                    Connection::ExecutePartitionedDmlParams const& epdp) {
        EXPECT_EQ(sql_statement, epdp.statement.sql());
        return PartitionedDmlResult{7};
      });

  Client client(conn);
  auto result = client.ExecutePartitionedDml(SqlStatement(sql_statement));
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(7, result->row_count_lower_bound);
}

TEST(ClientTest, CommitSuccess) {
  auto conn = std::make_shared<MockConnection>();

  auto ts = MakeTimestamp(std::chrono::system_clock::from_time_t(123)).value();
  CommitResult result;
  result.commit_timestamp = ts;

  Client client(conn);
  EXPECT_CALL(*conn, Commit).WillOnce(Return(result));

  auto txn = MakeReadWriteTransaction();
  auto commit = client.Commit(txn, {});
  ASSERT_STATUS_OK(commit);
  EXPECT_EQ(ts, commit->commit_timestamp);
}

TEST(ClientTest, CommitError) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Commit)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "blah")));

  auto txn = MakeReadWriteTransaction();
  EXPECT_THAT(client.Commit(txn, {}),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("blah")));
}

TEST(ClientTest, RollbackSuccess) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Rollback).WillOnce(Return(Status()));

  auto txn = MakeReadWriteTransaction();
  auto rollback = client.Rollback(txn);
  EXPECT_STATUS_OK(rollback);
}

TEST(ClientTest, RollbackError) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Rollback)
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "oops")));

  auto txn = MakeReadWriteTransaction();
  EXPECT_THAT(client.Rollback(txn),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("oops")));
}

TEST(ClientTest, CommitMutatorSuccess) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // placeholder
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {},
                                            {},  {}, {}, {}};
  Connection::CommitParams actual_commit_params{txn, {}, {}};

  auto source = std::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: STRING }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow("Bob")))
      .WillOnce(Return(Row()));

  EXPECT_CALL(*conn, Read)
      .WillOnce(DoAll(SaveArg<0>(&actual_read_params),
                      Return(ByMove(RowStream(std::move(source))))));
  EXPECT_CALL(*conn, Commit)
      .WillOnce(DoAll(SaveArg<0>(&actual_commit_params),
                      Return(CommitResult{*timestamp, absl::nullopt})));

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
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
  EXPECT_THAT(actual_commit_params.mutations, ElementsAre(mutation));
}

TEST(ClientTest, CommitMutatorRollback) {
  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // placeholder
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {},
                                            {},  {}, {}, {}};

  auto source = std::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  EXPECT_CALL(*conn, Read)
      .WillOnce(DoAll(SaveArg<0>(&actual_read_params),
                      Return(ByMove(RowStream(std::move(source))))));
  EXPECT_CALL(*conn, Rollback).WillOnce(Return(Status()));

  Client client(conn);
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto mutator = [&client, &mutation](Transaction txn) -> StatusOr<Mutations> {
    auto rows = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    for (auto& row : rows) {
      if (!row) return row.status();
    }
    return Mutations{mutation};
  };

  EXPECT_THAT(client.Commit(mutator),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("blah")));
  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
}

TEST(ClientTest, CommitMutatorRollbackError) {
  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // placeholder
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {},
                                            {},  {}, {}, {}};

  auto source = std::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  EXPECT_CALL(*conn, Read)
      .WillOnce(DoAll(SaveArg<0>(&actual_read_params),
                      Return(ByMove(RowStream(std::move(source))))));
  EXPECT_CALL(*conn, Rollback)
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

  EXPECT_THAT(client.Commit(mutator),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("blah")));
  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(ClientTest, CommitMutatorException) {
  auto conn = std::make_shared<MockConnection>();

  auto source = std::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Name",
        type: { code: INT64 }
      }
    }
  )pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  EXPECT_CALL(*conn, Read)
      .WillOnce(Return(ByMove(RowStream(std::move(source)))));
  EXPECT_CALL(*conn, Rollback).WillOnce(Return(Status()));

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
  EXPECT_CALL(*conn, Rollback).WillRepeatedly(Return(Status()));
  Client client(conn);
  try {
    auto result = client.Commit([](Transaction const&) -> StatusOr<Mutations> {
      throw RuntimeStatusError(Status());  // OK
    });
    EXPECT_THAT(result,
                StatusIs(StatusCode::kUnknown, HasSubstr("OK Status thrown")));
  } catch (...) {
    FAIL();  // exception consumed
  }
  try {
    auto result = client.Commit([](Transaction const&) -> StatusOr<Mutations> {
      throw RuntimeStatusError(Status(StatusCode::kCancelled, "uh oh"));
    });
    EXPECT_THAT(result, StatusIs(StatusCode::kCancelled, HasSubstr("uh oh")));
  } catch (...) {
    FAIL();  // exception consumed
  }
}
#endif

TEST(ClientTest, CommitMutatorRerunTransientFailures) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([](Connection::CommitParams const&) {
        return Status(StatusCode::kAborted, "Aborted transaction");
      })
      .WillOnce([&timestamp](Connection::CommitParams const&) {
        return CommitResult{*timestamp, absl::nullopt};
      });

  auto mutator = [](Transaction const&) -> StatusOr<Mutations> {
    return Mutations{MakeDeleteMutation("table", KeySet::All())};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitMutatorTooManyFailures) {
  int commit_attempts = 0;
  int const maximum_failures = 2;

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
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
  EXPECT_THAT(result,
              StatusIs(StatusCode::kAborted, HasSubstr("Aborted transaction")));
  EXPECT_EQ(maximum_failures + 1, commit_attempts);  // one too many
}

TEST(ClientTest, CommitMutatorPermanentFailure) {
  int commit_attempts = 0;

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&commit_attempts](Connection::CommitParams const&) {
        ++commit_attempts;
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  auto mutator = [](Transaction const&) -> StatusOr<Mutations> {
    return Mutations{MakeDeleteMutation("table", KeySet::All())};
  };

  Client client(conn);
  EXPECT_THAT(client.Commit(mutator),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  EXPECT_EQ(1, commit_attempts);  // no reruns
}

TEST(ClientTest, CommitMutations) {
  auto conn = std::make_shared<MockConnection>();
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2020-02-28T04:49:17.335Z");
  ASSERT_STATUS_OK(timestamp);
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&mutation, &timestamp](Connection::CommitParams const& cp) {
        EXPECT_EQ(cp.mutations, Mutations{mutation});
        return CommitResult{*timestamp, absl::nullopt};
      });

  Client client(conn);
  auto result = client.Commit({mutation});
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

MATCHER(DoesNotHaveSession, "not bound to a session") {
  return spanner_internal::Visit(
      arg, [&](spanner_internal::SessionHolder& session,
               StatusOr<google::spanner::v1::TransactionSelector>&,
               spanner_internal::TransactionContext const&) {
        if (session) {
          *result_listener << "has session " << session->session_name();
          return false;
        }
        return true;
      });
}

MATCHER_P(HasSession, name, "bound to expected session") {
  return spanner_internal::Visit(
      arg, [&](spanner_internal::SessionHolder& session,
               StatusOr<google::spanner::v1::TransactionSelector>&,
               spanner_internal::TransactionContext const&) {
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

MATCHER_P(HasTag, value, "bound to expected transaction tag") {
  return spanner_internal::Visit(
      arg, [&](spanner_internal::SessionHolder&,
               StatusOr<google::spanner::v1::TransactionSelector>&,
               spanner_internal::TransactionContext const& ctx) {
        if (ctx.tag != value) {
          *result_listener << "has tag " << ctx.tag << " but expected "
                           << value;
          return false;
        }
        return true;
      });
}

MATCHER(HasBegin, "bound to a new (begin) transaction") {
  return spanner_internal::Visit(
      arg, [&](spanner_internal::SessionHolder&,
               StatusOr<google::spanner::v1::TransactionSelector>& s,
               spanner_internal::TransactionContext const&) {
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

MATCHER(HasSingleUse, "bound to a temporary (single-use) transaction") {
  return spanner_internal::Visit(
      arg, [&](spanner_internal::SessionHolder&,
               StatusOr<google::spanner::v1::TransactionSelector>& s,
               spanner_internal::TransactionContext const&) {
        if (!s) {
          *result_listener << "has status " << s.status();
          return false;
        }
        if (!s->has_single_use()) {
          if (s->has_begin()) {
            *result_listener << "is begin";
          } else {
            *result_listener << "has transaction-id " << s->id();
          }
          return false;
        }
        return true;
      });
}

bool SetSessionName(Transaction const& txn, std::string name) {
  return spanner_internal::Visit(
      txn, [&name](spanner_internal::SessionHolder& session,
                   StatusOr<google::spanner::v1::TransactionSelector>&,
                   spanner_internal::TransactionContext const&) {
        session =
            spanner_internal::MakeDissociatedSessionHolder(std::move(name));
        return true;
      });
}

bool SetTransactionId(Transaction const& txn, std::string id) {
  return spanner_internal::Visit(
      txn, [&id](spanner_internal::SessionHolder&,
                 StatusOr<google::spanner::v1::TransactionSelector>& s,
                 spanner_internal::TransactionContext const&) {
        s->set_id(std::move(id));  // only valid when s.ok()
        return true;
      });
}

TEST(ClientTest, CommitMutatorWithTags) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2021-04-26T17:25:36.321Z");
  ASSERT_STATUS_OK(timestamp);
  std::string const transaction_tag = "app=cart,env=dev";

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, ExecuteQuery)
      .WillOnce([&](Connection::SqlParams const& params) {
        EXPECT_EQ(params.query_options.request_tag(), "action=ExecuteQuery");
        EXPECT_THAT(params.transaction, HasTag(transaction_tag));
        return RowStream(std::make_unique<MockResultSetSource>());
      });
  EXPECT_CALL(*conn, ExecuteBatchDml)
      .WillOnce([&](Connection::ExecuteBatchDmlParams const& params) {
        EXPECT_EQ(params.options.get<RequestTagOption>(),
                  "action=ExecuteBatchDml");
        EXPECT_THAT(params.transaction, HasTag(transaction_tag));
        return BatchDmlResult{};
      });
  EXPECT_CALL(*conn, Read).WillOnce([&](Connection::ReadParams const& params) {
    EXPECT_EQ(params.read_options.request_tag, "action=Read");
    EXPECT_THAT(params.transaction, HasTag(transaction_tag));
    return RowStream(std::make_unique<MockResultSetSource>());
  });
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&](Connection::CommitParams const& params) {
        EXPECT_EQ(params.options.transaction_tag(), transaction_tag);
        EXPECT_THAT(params.transaction, HasTag(transaction_tag));
        return CommitResult{*timestamp, absl::nullopt};
      });

  Client client(conn);
  auto mutator = [&client](Transaction const& txn) -> StatusOr<Mutations> {
    auto query_rows = client.ExecuteQuery(
        txn, SqlStatement("SELECT * FROM Table;"),
        QueryOptions{}.set_request_tag("action=ExecuteQuery"));
    auto result = client.ExecuteBatchDml(
        txn, {SqlStatement("UPDATE Foo SET Bar = 2")},
        Options{}.set<spanner::RequestTagOption>("action=ExecuteBatchDml"));
    ReadOptions read_options;
    read_options.request_tag = "action=Read";
    auto read_rows = client.Read(txn, "table", KeySet::All(), {"column"},
                                 std::move(read_options));
    return Mutations{};
  };
  auto result = client.Commit(
      mutator, Options{}.set<TransactionTagOption>(transaction_tag));
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitMutatorSessionAffinity) {
  int const num_aborts = 10;  // how many aborts before success

  // After assigning a session during the first aborted transaction, we
  // should see the same session in a new transaction on every rerun.
  std::string const session_name = "CommitMutatorLockPriority.Session";

  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2019-11-11T20:05:36.345Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  // Eventually the Commit() will succeed.
  EXPECT_CALL(*conn, Commit)
      .WillOnce(
          [&session_name, &timestamp](Connection::CommitParams const& cp) {
            EXPECT_THAT(cp.transaction, HasSession(session_name));
            EXPECT_THAT(cp.transaction, HasBegin());
            SetTransactionId(cp.transaction, "last-transaction-id");
            return CommitResult{*timestamp, absl::nullopt};
          });
  // But only after some aborts, the first of which sets the session.
  EXPECT_CALL(*conn, Commit)
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
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitMutatorSessionNotFound) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&timestamp](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession("session-3"));
        return CommitResult{*timestamp, absl::nullopt};
      });

  int n = 0;
  auto mutator = [&n](Transaction const& txn) -> StatusOr<Mutations> {
    EXPECT_THAT(txn, DoesNotHaveSession());
    auto session_name = "session-" + std::to_string(++n);
    SetSessionName(txn, session_name);
    if (n < 3) return SessionNotFoundError(std::move(session_name));
    return Mutations{};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitSessionNotFound) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession("session-1"));
        return SessionNotFoundError("session-1");
      })
      .WillOnce([&timestamp](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSession("session-2"));
        return CommitResult{*timestamp, absl::nullopt};
      });

  int n = 0;
  auto mutator = [&n](Transaction const& txn) -> StatusOr<Mutations> {
    EXPECT_THAT(txn, DoesNotHaveSession());
    SetSessionName(txn, "session-" + std::to_string(++n));
    return Mutations{};
  };

  Client client(conn);
  auto result = client.Commit(mutator);
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitStats) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2020-10-20T02:20:09.123Z");
  ASSERT_STATUS_OK(timestamp);
  CommitStats stats{42};

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&timestamp, &stats](Connection::CommitParams const& cp) {
        EXPECT_TRUE(cp.options.return_stats());
        return CommitResult{*timestamp, stats};
      });

  Client client(conn);
  auto result =
      client.Commit(Mutations{}, Options{}.set<CommitReturnStatsOption>(true));
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
  ASSERT_TRUE(result->commit_stats.has_value());
  EXPECT_EQ(42, result->commit_stats->mutation_count);
}

TEST(ClientTest, MaxCommitDelay) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2020-10-20T02:20:09.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&timestamp](Connection::CommitParams const& cp) {
        EXPECT_EQ(cp.options.max_commit_delay(),
                  std::chrono::milliseconds(100));
        return CommitResult{*timestamp, absl::nullopt};
      });

  Client client(conn);
  Options options;
  options.set<MaxCommitDelayOption>(std::chrono::milliseconds(100));
  auto result = client.Commit(Mutations{}, options);
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitAtLeastOnce) {
  auto timestamp =
      spanner_internal::TimestampFromRFC3339("2023-06-02T07:36:52.808Z");
  ASSERT_STATUS_OK(timestamp);
  auto mutation = MakeDeleteMutation("table", KeySet::All());
  std::string const transaction_tag = "app=cart,env=dev";

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Commit)
      .WillOnce([&mutation, &transaction_tag,
                 &timestamp](Connection::CommitParams const& cp) {
        EXPECT_THAT(cp.transaction, HasSingleUse());
        EXPECT_EQ(cp.mutations, Mutations{mutation});
        EXPECT_FALSE(cp.options.return_stats());
        EXPECT_FALSE(cp.options.request_priority().has_value());
        EXPECT_FALSE(cp.options.max_commit_delay().has_value());
        EXPECT_EQ(cp.options.transaction_tag(), transaction_tag);
        return CommitResult{*timestamp, absl::nullopt};
      });

  Client client(conn);
  auto result = client.CommitAtLeastOnce(
      Transaction::ReadWriteOptions{}, {mutation},
      Options{}.set<TransactionTagOption>(transaction_tag));
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);
}

TEST(ClientTest, CommitAtLeastOnceBatched) {
  std::string const request_tag = "action=upsert";
  std::string const transaction_tag = "app=cart,env=dev";
  auto const timestamp =
      spanner_internal::TimestampFromRFC3339("2023-09-27T06:11:34.335Z");
  ASSERT_STATUS_OK(timestamp);
  std::vector<Mutations> const mutation_groups = {
      {MakeInsertOrUpdateMutation("table", {"col1", "col2"}, 10, 20)},
      {MakeInsertOrUpdateMutation("table", {"col1", "col2"}, 11, 21)},
  };

  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, options()).WillRepeatedly(Return(Options{}));
  EXPECT_CALL(*conn, BatchWrite)
      .WillOnce([&](Connection::BatchWriteParams const& params) {
        EXPECT_THAT(params.mutation_groups, SizeIs(2));
        EXPECT_FALSE(params.options.has<RequestPriorityOption>());
        EXPECT_FALSE(params.options.has<RequestTagOption>());
        EXPECT_FALSE(params.options.has<TransactionTagOption>());
        return mocks::MakeStreamRange<BatchedCommitResult>(
            {{{0, 1}, timestamp}});
      })
      .WillOnce([&](Connection::BatchWriteParams const& params) {
        EXPECT_THAT(params.mutation_groups, SizeIs(2));
        EXPECT_TRUE(params.options.has<RequestPriorityOption>());
        EXPECT_EQ(params.options.get<RequestPriorityOption>(),
                  RequestPriority::kHigh);
        EXPECT_TRUE(params.options.has<RequestTagOption>());
        EXPECT_EQ(params.options.get<RequestTagOption>(), request_tag);
        EXPECT_FALSE(params.options.has<TransactionTagOption>());
        return mocks::MakeStreamRange<BatchedCommitResult>(
            {{{0, 1}, timestamp}});
      })
      .WillOnce([&](Connection::BatchWriteParams const& params) {
        EXPECT_THAT(params.mutation_groups, SizeIs(2));
        EXPECT_FALSE(params.options.has<RequestPriorityOption>());
        EXPECT_FALSE(params.options.has<RequestTagOption>());
        EXPECT_TRUE(params.options.has<TransactionTagOption>());
        EXPECT_EQ(params.options.get<TransactionTagOption>(), transaction_tag);
        return mocks::MakeStreamRange<BatchedCommitResult>(
            {{{0, 1}, timestamp}});
      })
      .WillOnce([](Connection::BatchWriteParams const& params) {
        EXPECT_THAT(params.mutation_groups, SizeIs(2));
        EXPECT_FALSE(params.options.has<RequestPriorityOption>());
        EXPECT_FALSE(params.options.has<RequestTagOption>());
        EXPECT_FALSE(params.options.has<TransactionTagOption>());
        return mocks::MakeStreamRange<BatchedCommitResult>(
            {}, Status(StatusCode::kInvalidArgument, "oops"));
      });

  Client client(conn);
  for (auto const& opts : {
           Options{},
           Options{}
               .set<RequestPriorityOption>(RequestPriority::kHigh)
               .set<RequestTagOption>(request_tag),
           Options{}.set<TransactionTagOption>(transaction_tag),
       }) {
    auto commit_results = client.CommitAtLeastOnce(mutation_groups, opts);
    auto it = commit_results.begin();
    ASSERT_NE(it, commit_results.end());
    EXPECT_THAT(
        *it,
        IsOkAndHolds(AllOf(
            Field(&BatchedCommitResult::indexes, ElementsAre(0, 1)),
            Field(&BatchedCommitResult::commit_timestamp, Eq(timestamp)))));
    EXPECT_EQ(++it, commit_results.end());
  }
  auto commit_results = client.CommitAtLeastOnce(mutation_groups, Options{});
  auto it = commit_results.begin();
  ASSERT_NE(it, commit_results.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kInvalidArgument, "oops"));
  EXPECT_EQ(++it, commit_results.end());
}

TEST(ClientTest, ProfileQuerySuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

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
  google::spanner::v1::ResultSetMetadata metadata;
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

  EXPECT_CALL(*conn, ProfileQuery)
      .WillOnce([&metadata, &stats](Connection::SqlParams const& params) {
        EXPECT_THAT(params.directed_read_option,
                    VariantWith<ExcludeReplicas>(Property(
                        &ExcludeReplicas::replica_selections,
                        ElementsAre(ReplicaSelection(ReplicaType::kReadWrite),
                                    ReplicaSelection("us-east4")))));
        auto source = std::make_unique<MockResultSetSource>();
        EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
        EXPECT_CALL(*source, NextRow())
            .WillOnce(Return(spanner_mocks::MakeRow("Ann", 42)))
            .WillOnce(Return(Row()));
        EXPECT_CALL(*source, Stats()).WillRepeatedly(Return(stats));
        return ProfileQueryResult(std::move(source));
      });

  KeySet keys = KeySet::All();
  auto rows = client.ProfileQuery(
      SqlStatement("SELECT * FROM Table;"),
      Options{}.set<DirectedReadOption>(
          ExcludeReplicas({ReplicaSelection(ReplicaType::kReadWrite),
                           ReplicaSelection("us-east4")})));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto stream = StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType("Ann", 42))));

  auto actual_plan = rows.ExecutionPlan();
  ASSERT_TRUE(actual_plan);
  EXPECT_THAT(*actual_plan, IsProtoEqual(stats.query_plan()));

  auto actual_stats = rows.ExecutionStats();
  ASSERT_TRUE(actual_stats);
  EXPECT_THAT(*actual_stats,
              UnorderedElementsAre(Pair("elapsed_time", "42 secs")));
}

TEST(ClientTest, ProfileQueryWithOptionsSuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = std::make_unique<MockResultSetSource>();
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
  google::spanner::v1::ResultSetMetadata metadata;
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
      .WillOnce(Return(spanner_mocks::MakeRow("Ann", 42)))
      .WillOnce(Return(Row()));
  EXPECT_CALL(*source, Stats()).WillRepeatedly(Return(stats));

  EXPECT_CALL(*conn, ProfileQuery)
      .WillOnce(Return(ByMove(ProfileQueryResult(std::move(source)))));

  KeySet keys = KeySet::All();
  auto rows = client.ProfileQuery(
      Transaction::SingleUseOptions(
          /*max_staleness=*/std::chrono::nanoseconds(std::chrono::minutes(5))),
      SqlStatement("SELECT * FROM Table;"));

  using RowType = std::tuple<std::string, std::int64_t>;
  auto stream = StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType("Ann", 42))));

  auto actual_plan = rows.ExecutionPlan();
  ASSERT_TRUE(actual_plan);
  EXPECT_THAT(*actual_plan, IsProtoEqual(stats.query_plan()));

  auto actual_stats = rows.ExecutionStats();
  ASSERT_TRUE(actual_stats);
  EXPECT_THAT(*actual_stats,
              UnorderedElementsAre(Pair("elapsed_time", "42 secs")));
}

struct StringOption {
  using Type = std::string;
};

TEST(ClientTest, UsesConnectionOptions) {
  auto conn = std::make_shared<MockConnection>();
  auto txn = MakeReadWriteTransaction();

  EXPECT_CALL(*conn, options).WillOnce([] {
    return Options{}.set<StringOption>("connection");
  });
  EXPECT_CALL(*conn, Rollback)
      .WillOnce([txn](Connection::RollbackParams const& params) {
        auto const& options = internal::CurrentOptions();
        EXPECT_THAT(options.get<StringOption>(), Eq("connection"));
        EXPECT_THAT(params.transaction, Eq(txn));
        return Status();
      });

  Client client(conn, Options{});
  auto rollback = client.Rollback(txn, Options{});
  EXPECT_STATUS_OK(rollback);
}

TEST(ClientTest, UsesClientOptions) {
  auto conn = std::make_shared<MockConnection>();
  auto txn = MakeReadWriteTransaction();

  EXPECT_CALL(*conn, options).WillOnce([] {
    return Options{}.set<StringOption>("connection");
  });
  EXPECT_CALL(*conn, Rollback)
      .WillOnce([txn](Connection::RollbackParams const& params) {
        auto const& options = internal::CurrentOptions();
        EXPECT_THAT(options.get<StringOption>(), Eq("client"));
        EXPECT_THAT(params.transaction, Eq(txn));
        return Status();
      });

  Client client(conn, Options{}.set<StringOption>("client"));
  auto rollback = client.Rollback(txn, Options{});
  EXPECT_STATUS_OK(rollback);
}

TEST(ClientTest, UsesOperationOptions) {
  auto conn = std::make_shared<MockConnection>();
  auto txn = MakeReadWriteTransaction();

  EXPECT_CALL(*conn, options).WillOnce([] {
    return Options{}.set<StringOption>("connection");
  });
  EXPECT_CALL(*conn, Rollback)
      .WillOnce([txn](Connection::RollbackParams const& params) {
        auto const& options = internal::CurrentOptions();
        EXPECT_THAT(options.get<StringOption>(), Eq("operation"));
        EXPECT_THAT(params.transaction, Eq(txn));
        return Status();
      });

  Client client(conn, Options{}.set<StringOption>("client"));
  auto rollback =
      client.Rollback(txn, Options{}.set<StringOption>("operation"));
  EXPECT_STATUS_OK(rollback);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
