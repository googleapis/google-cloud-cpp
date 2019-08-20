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
#include "google/cloud/spanner/internal/time.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/result_set.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
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

using ::google::cloud::internal::make_unique;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;

class MockConnection : public Connection {
 public:
  MOCK_METHOD1(Read, StatusOr<ResultSet>(ReadParams));
  MOCK_METHOD1(ExecuteSql, StatusOr<ResultSet>(ExecuteSqlParams));
  MOCK_METHOD1(Commit, StatusOr<CommitResult>(CommitParams));
  MOCK_METHOD1(Rollback, Status(RollbackParams));
};

class MockResultSetSource : public internal::ResultSetSource {
 public:
  MOCK_METHOD0(NextValue, StatusOr<optional<Value>>());
  MOCK_METHOD0(Metadata, optional<spanner_proto::ResultSetMetadata>());
  MOCK_METHOD0(Stats, optional<spanner_proto::ResultSetStats>());
};

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

  auto source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
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
      )pb",
      &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, Stats())
      .WillRepeatedly(Return(optional<spanner_proto::ResultSetStats>()));
  EXPECT_CALL(*source, NextValue())
      .WillOnce(Return(optional<Value>("Steve")))
      .WillOnce(Return(optional<Value>(12)))
      .WillOnce(Return(optional<Value>("Ann")))
      .WillOnce(Return(optional<Value>(42)))
      .WillOnce(Return(optional<Value>()));

  ResultSet result_set(std::move(source));
  EXPECT_CALL(*conn, Read(_)).WillOnce(Return(ByMove(std::move(result_set))));

  KeySet keys = KeySet::All();
  auto result = client.Read("table", std::move(keys), {"column1", "column2"});
  EXPECT_STATUS_OK(result);

  std::array<std::pair<std::string, std::int64_t>, 2> expected = {
      std::make_pair("Steve", 12), std::make_pair("Ann", 42)};
  int row_number = 0;
  for (auto& row : result->Rows<std::string, std::int64_t>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 2);
    EXPECT_EQ(row->get<0>(), expected[row_number].first);
    EXPECT_EQ(row->get<1>(), expected[row_number].second);
    ++row_number;
  }
  EXPECT_EQ(row_number, 2);
}

TEST(ClientTest, ReadFailure) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        row_type: {
          fields: {
            name: "Name",
            type: { code: INT64 }
          }
        }
      )pb",
      &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, Stats())
      .WillRepeatedly(Return(optional<spanner_proto::ResultSetStats>()));
  EXPECT_CALL(*source, NextValue())
      .WillOnce(Return(optional<Value>("Steve")))
      .WillOnce(Return(optional<Value>("Ann")))
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "deadline!")));

  ResultSet result_set(std::move(source));
  EXPECT_CALL(*conn, Read(_)).WillOnce(Return(ByMove(std::move(result_set))));

  KeySet keys = KeySet::All();
  auto result = client.Read("table", std::move(keys), {"column1"});
  EXPECT_STATUS_OK(result);

  auto rows = result->Rows<std::string>();
  auto iter = rows.begin();
  EXPECT_NE(iter, rows.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ((*iter)->get<0>(), "Steve");

  ++iter;
  EXPECT_NE(iter, rows.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ((*iter)->get<0>(), "Ann");

  ++iter;
  EXPECT_FALSE((*iter).ok());
  EXPECT_EQ((*iter).status().code(), StatusCode::kDeadlineExceeded);
}

TEST(ClientTest, ExecuteSqlSuccess) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
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
      )pb",
      &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, Stats())
      .WillRepeatedly(Return(optional<spanner_proto::ResultSetStats>()));
  EXPECT_CALL(*source, NextValue())
      .WillOnce(Return(optional<Value>("Steve")))
      .WillOnce(Return(optional<Value>(12)))
      .WillOnce(Return(optional<Value>("Ann")))
      .WillOnce(Return(optional<Value>(42)))
      .WillOnce(Return(optional<Value>()));

  ResultSet result_set(std::move(source));
  EXPECT_CALL(*conn, ExecuteSql(_))
      .WillOnce(Return(ByMove(std::move(result_set))));

  KeySet keys = KeySet::All();
  auto result = client.ExecuteSql(SqlStatement("select * from table;"));
  EXPECT_STATUS_OK(result);

  std::array<std::pair<std::string, std::int64_t>, 2> expected = {
      std::make_pair("Steve", 12), std::make_pair("Ann", 42)};
  int row_number = 0;
  for (auto& row : result->Rows<std::string, std::int64_t>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 2);
    EXPECT_EQ(row->get<0>(), expected[row_number].first);
    EXPECT_EQ(row->get<1>(), expected[row_number].second);
    ++row_number;
  }
  EXPECT_EQ(row_number, 2);
}

TEST(ClientTest, ExecuteSqlFailure) {
  auto conn = std::make_shared<MockConnection>();
  Client client(conn);

  auto source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        row_type: {
          fields: {
            name: "Name",
            type: { code: INT64 }
          }
        }
      )pb",
      &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  EXPECT_CALL(*source, Stats())
      .WillRepeatedly(Return(optional<spanner_proto::ResultSetStats>()));
  EXPECT_CALL(*source, NextValue())
      .WillOnce(Return(optional<Value>("Steve")))
      .WillOnce(Return(optional<Value>("Ann")))
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "deadline!")));

  ResultSet result_set(std::move(source));
  EXPECT_CALL(*conn, ExecuteSql(_))
      .WillOnce(Return(ByMove(std::move(result_set))));

  KeySet keys = KeySet::All();
  auto result = client.ExecuteSql(SqlStatement("select * from table;"));
  EXPECT_STATUS_OK(result);

  auto rows = result->Rows<std::string>();
  auto iter = rows.begin();
  EXPECT_NE(iter, rows.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ((*iter)->get<0>(), "Steve");

  ++iter;
  EXPECT_NE(iter, rows.end());
  EXPECT_STATUS_OK(*iter);
  EXPECT_EQ((*iter)->get<0>(), "Ann");

  ++iter;
  EXPECT_FALSE((*iter).ok());
  EXPECT_EQ((*iter).status().code(), StatusCode::kDeadlineExceeded);
}

TEST(ClientTest, CommitSuccess) {
  auto conn = std::make_shared<MockConnection>();

  auto ts = Timestamp(std::chrono::seconds(123));
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
  EXPECT_TRUE(rollback.ok());
}

TEST(ClientTest, RollbackError) {
  auto conn = std::make_shared<MockConnection>();

  Client client(conn);
  EXPECT_CALL(*conn, Rollback(_))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "blah")));

  auto txn = MakeReadWriteTransaction();
  auto rollback = client.Rollback(txn);
  EXPECT_EQ(StatusCode::kInvalidArgument, rollback.code());
}

TEST(ClientTest, MakeConnectionOptionalArguments) {
  Database db("foo", "bar", "baz");
  auto conn = MakeConnection(db);
  EXPECT_NE(conn, nullptr);

  conn = MakeConnection(db, grpc::GoogleDefaultCredentials());
  EXPECT_NE(conn, nullptr);

  conn = MakeConnection(db, grpc::GoogleDefaultCredentials(), "localhost");
  EXPECT_NE(conn, nullptr);
}

TEST(ClientTest, RunTransactionCommit) {
  auto timestamp = internal::TimestampFromString("2019-08-14T21:16:21.123Z");
  ASSERT_STATUS_OK(timestamp);

  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // dummy
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {}};
  Connection::CommitParams actual_commit_params{txn, {}};
  EXPECT_CALL(*conn, Read(_))
      .WillOnce(
          DoAll(SaveArg<0>(&actual_read_params), Return(ByMove(ResultSet{}))));
  EXPECT_CALL(*conn, Commit(_))
      .WillOnce(DoAll(SaveArg<0>(&actual_commit_params),
                      Return(CommitResult{*timestamp})));

  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto f = [&mutation](Client client, Transaction txn) {
    auto read = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    if (!read) return TransactionAction{TransactionAction::kRollback, {}};
    return TransactionAction{TransactionAction::kCommit, {mutation}};
  };

  Client client(conn);
  auto result = RunTransaction(client, Transaction::ReadWriteOptions{}, f);
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(*timestamp, result->commit_timestamp);

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
  EXPECT_THAT(actual_commit_params.mutations, ElementsAre(mutation));
}

TEST(ClientTest, RunTransactionRollback) {
  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // dummy
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {}};
  EXPECT_CALL(*conn, Read(_))
      .WillOnce(
          DoAll(SaveArg<0>(&actual_read_params),
                Return(ByMove(Status(StatusCode::kInvalidArgument, "blah")))));
  EXPECT_CALL(*conn, Rollback(_)).WillOnce(Return(Status()));

  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto f = [&mutation](Client client, Transaction txn) {
    auto read = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    if (!read) return TransactionAction{TransactionAction::kRollback, {}};
    return TransactionAction{TransactionAction::kCommit, {mutation}};
  };

  Client client(conn);
  auto result = RunTransaction(client, Transaction::ReadWriteOptions{}, f);
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(Timestamp{}, result->commit_timestamp);

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
}

TEST(ClientTest, RunTransactionRollbackError) {
  auto conn = std::make_shared<MockConnection>();
  Transaction txn = MakeReadWriteTransaction();  // dummy
  Connection::ReadParams actual_read_params{txn, {}, {}, {}, {}};
  EXPECT_CALL(*conn, Read(_))
      .WillOnce(
          DoAll(SaveArg<0>(&actual_read_params),
                Return(ByMove(Status(StatusCode::kInvalidArgument, "blah")))));
  EXPECT_CALL(*conn, Rollback(_))
      .WillOnce(Return(Status(StatusCode::kInternal, "blah blah")));

  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto f = [&mutation](Client client, Transaction txn) {
    auto read = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    if (!read) return TransactionAction{TransactionAction::kRollback, {}};
    return TransactionAction{TransactionAction::kCommit, {mutation}};
  };

  Client client(conn);
  auto result = RunTransaction(client, Transaction::ReadWriteOptions{}, f);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(StatusCode::kInternal, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("blah blah"));

  EXPECT_EQ("T", actual_read_params.table);
  EXPECT_EQ(KeySet::All(), actual_read_params.keys);
  EXPECT_THAT(actual_read_params.columns, ElementsAre("C"));
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(ClientTest, RunTransactionException) {
  auto conn = std::make_shared<MockConnection>();
  EXPECT_CALL(*conn, Read(_))
      .WillOnce(Return(ByMove(Status(StatusCode::kInvalidArgument, "blah"))));
  EXPECT_CALL(*conn, Rollback(_)).WillOnce(Return(Status()));

  auto mutation = MakeDeleteMutation("table", KeySet::All());
  auto f = [&mutation](Client client, Transaction txn) {
    auto read = client.Read(std::move(txn), "T", KeySet::All(), {"C"});
    if (!read) throw "Read() error";
    return TransactionAction{TransactionAction::kCommit, {mutation}};
  };

  try {
    Client client(conn);
    auto result = RunTransaction(client, Transaction::ReadWriteOptions{}, f);
    FAIL();
  } catch (char const* e) {
    EXPECT_STREQ(e, "Read() error");
  } catch (...) {
    FAIL();
  }
}
#endif

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
