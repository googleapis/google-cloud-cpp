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

#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/internal/time.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <atomic>
#include <future>
#include <string>
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::internal::make_unique;
using ::google::cloud::spanner_testing::HasSessionAndTransactionId;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StartsWith;

namespace spanner_proto = ::google::spanner::v1;

// Matches a spanner_proto::ReadRequest with the specified `session` and
// `transaction_id`.
MATCHER_P2(ReadRequestHasSessionAndTransactionId, session, transaction_id,
           "ReadRequest has expected session and transaction") {
  return arg.session() == session && arg.transaction().id() == transaction_id;
}

// Matches a spanner_proto::ReadRequest with the specified `session` and
// 'begin` set in the TransactionSelector.
MATCHER_P(ReadRequestHasSessionAndBeginTransaction, session,
          "ReadRequest has expected session and transaction") {
  return arg.session() == session && arg.transaction().has_begin();
}

// Matches a spanner_proto::CreateSessionRequest with the specified `database`.
MATCHER_P(CreateSessionRequestHasDatabase, database,
          "CreateSessionRequest has expected database") {
  return arg.database() == database;
}

std::shared_ptr<Connection> MakeTestConnection(
    Database const& db,
    std::shared_ptr<spanner_testing::MockSpannerStub> mock) {
  return std::make_shared<ConnectionImpl>(
      db, std::move(mock),
      LimitedErrorCountRetryPolicy(/*maximum_failures=*/2).clone(),
      ExponentialBackoffPolicy(/*initial_delay=*/std::chrono::microseconds(1),
                               /*maximum_delay=*/std::chrono::microseconds(1),
                               /*scaling=*/2.0)
          .clone());
}

class MockGrpcReader
    : public ::grpc::ClientReaderInterface<spanner_proto::PartialResultSet> {
 public:
  MOCK_METHOD1(Read, bool(spanner_proto::PartialResultSet*));
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD0(WaitForInitialMetadata, void());
};

std::shared_ptr<Connection> MakeConnection(Database const& db,
                                           std::shared_ptr<SpannerStub> stub) {
  return std::make_shared<ConnectionImpl>(db, std::move(stub));
}

TEST(ConnectionImplTest, ReadGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto result =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"column1"},
                  ReadOptions()});
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, ReadStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  EXPECT_CALL(*grpc_reader, Read(_)).WillOnce(Return(false));
  grpc::Status finish_status(grpc::StatusCode::PERMISSION_DENIED,
                             "uh-oh in GrpcReader::Finish");
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(finish_status));
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  auto result =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"column1"},
                  ReadOptions()});
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(),
              HasSubstr("uh-oh in GrpcReader::Finish"));
}

TEST(ConnectionImplTest, ReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "UserId",
              type: { code: INT64 }
            }
            fields: {
              name: "UserName",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "12" }
        values: { string_value: "Steve" }
        values: { string_value: "42" }
        values: { string_value: "Ann" }
      )pb",
      &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  auto result =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"UserId", "UserName"},
                  ReadOptions()});
  EXPECT_STATUS_OK(result);
  using RowType = Row<std::int64_t, std::string>;
  auto expected = std::vector<RowType>{
      RowType(12, "Steve"),
      RowType(42, "Ann"),
  };
  int row_number = 0;
  for (auto& row : result->Rows<RowType>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
}

/// @test Verify implicit "begin transaction" in Read() works.
TEST(ConnectionImplTest, ReadImplicitBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(metadata: { transaction: { id: "ABCDEF00" } })pb", &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto result = conn->Read(
      {txn, "table", KeySet::All(), {"UserId", "UserName"}, ReadOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "ABCDEF00"));
}

TEST(ConnectionImplTest, ExecuteSqlGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto result = conn->ExecuteSql(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, ExecuteSqlStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  EXPECT_CALL(*grpc_reader, Read(_)).WillOnce(Return(false));
  grpc::Status finish_status(grpc::StatusCode::PERMISSION_DENIED,
                             "uh-oh in GrpcReader::Finish");
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(finish_status));
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  auto result = conn->ExecuteSql(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(),
              HasSubstr("uh-oh in GrpcReader::Finish"));
}

TEST(ConnectionImplTest, ExecuteSqlReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        metadata: {
          row_type: {
            fields: {
              name: "UserId",
              type: { code: INT64 }
            }
            fields: {
              name: "UserName",
              type: { code: STRING }
            }
          }
        }
        values: { string_value: "12" }
        values: { string_value: "Steve" }
        values: { string_value: "42" }
        values: { string_value: "Ann" }
      )pb",
      &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  auto result = conn->ExecuteSql(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  EXPECT_STATUS_OK(result);
  using RowType = Row<std::int64_t, std::string>;
  auto expected = std::vector<RowType>{
      RowType(12, "Steve"),
      RowType(42, "Ann"),
  };
  int row_number = 0;
  for (auto& row : result->Rows<RowType>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
}

/// @test Verify implicit "begin transaction" in ExecuteSql() works.
TEST(ConnectionImplTest, ExecuteSqlImplicitBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(metadata: { transaction: { id: "00FEDCBA" } })pb", &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto result = conn->ExecuteSql({txn, SqlStatement("select * from table")});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "00FEDCBA"));
}

TEST(ConnectionImplTest, ExecuteBatchDmlSuccess) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  spanner_proto::Session session;
  session.set_name("session-name");
  EXPECT_CALL(*mock, CreateSession(_, _)).WillOnce(Return(session));

  spanner_proto::ExecuteBatchDmlResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        result_sets: {
          metadata: { transaction: { id: "1234567890" } }
          stats: { row_count_exact: 0 }
        }
        result_sets: { stats: { row_count_exact: 1 } }
        result_sets: { stats: { row_count_exact: 2 } }
      )pb",
      &response));
  EXPECT_CALL(*mock, ExecuteBatchDml(_, _)).WillOnce(Return(response));

  auto request = {
      SqlStatement("update ..."),
      SqlStatement("update ..."),
      SqlStatement("update ..."),
  };

  auto conn = MakeConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_STATUS_OK(result);
  EXPECT_STATUS_OK(result->status);
  EXPECT_EQ(result->stats.size(), request.size());
  EXPECT_EQ(result->stats.size(), 3);
  EXPECT_EQ(result->stats[0].row_count, 0);
  EXPECT_EQ(result->stats[1].row_count, 1);
  EXPECT_EQ(result->stats[2].row_count, 2);
  EXPECT_THAT(txn, HasSessionAndTransactionId("session-name", "1234567890"));
}

TEST(ConnectionImplTest, ExecuteBatchDmlFailure) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  spanner_proto::Session session;
  session.set_name("session-name");
  EXPECT_CALL(*mock, CreateSession(_, _)).WillOnce(Return(session));

  spanner_proto::ExecuteBatchDmlResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        result_sets: {
          metadata: { transaction: { id: "1234567890" } }
          stats: { row_count_exact: 42 }
        }
        result_sets: { stats: { row_count_exact: 43 } }
        status: { code: 2 message: "oops" }
      )pb",
      &response));
  EXPECT_CALL(*mock, ExecuteBatchDml(_, _)).WillOnce(Return(response));

  auto request = {
      SqlStatement("update ..."),
      SqlStatement("update ..."),
      SqlStatement("update ..."),
  };

  auto conn = MakeConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_STATUS_OK(result);
  EXPECT_EQ(result->status.code(), StatusCode::kUnknown);
  EXPECT_EQ(result->status.message(), "oops");
  EXPECT_NE(result->stats.size(), request.size());
  EXPECT_EQ(result->stats.size(), 2);
  EXPECT_EQ(result->stats[0].row_count, 42);
  EXPECT_EQ(result->stats[1].row_count, 43);
  EXPECT_THAT(txn, HasSessionAndTransactionId("session-name", "1234567890"));
}

TEST(ConnectionImplTest, CommitGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(
          Invoke([&db](grpc::ClientContext&,
                       spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto commit = conn->Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, CommitCommitFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(
          Invoke([&db](grpc::ClientContext&,
                       spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(Invoke([](grpc::ClientContext&,
                          spanner_proto::CommitRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        EXPECT_TRUE(request.has_single_use_transaction());
        return Status(StatusCode::kPermissionDenied, "uh-oh in Commit");
      }));
  auto commit = conn->Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in Commit"));
}

TEST(ConnectionImplTest, CommitSuccessWithTransactionId) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(
          Invoke([&db](grpc::ClientContext&,
                       spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(Invoke([](grpc::ClientContext&,
                          spanner_proto::CommitRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        EXPECT_EQ("test-txn-id", request.transaction_id());
        spanner_proto::CommitResponse response;
        *response.mutable_commit_timestamp() =
            internal::ToProto(Timestamp{std::chrono::seconds(123)});
        return response;
      }));

  auto txn = MakeReadWriteTransaction();
  internal::Visit(txn, [](SessionHolder&, spanner_proto::TransactionSelector& s,
                          std::int64_t) {
    s.set_id("test-txn-id");
    return 0;
  });

  auto commit = conn->Commit({txn, {}});
  EXPECT_STATUS_OK(commit);
  EXPECT_EQ(commit->commit_timestamp, Timestamp{std::chrono::seconds(123)});
}

TEST(ConnectionImplTest, RollbackGetSessionFailure) {
  auto db = Database("project", "instance", "database");

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(
          Invoke([&db](grpc::ClientContext&,
                       spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  auto conn = MakeConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn->Rollback({txn});
  EXPECT_EQ(StatusCode::kPermissionDenied, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, RollbackBeginTransaction) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&db, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(db.FullName(), request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  auto conn = MakeConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn->Rollback({txn});
  EXPECT_STATUS_OK(rollback);
}

TEST(ConnectionImplTest, RollbackSingleUseTransaction) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&db, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(db.FullName(), request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  auto conn = MakeConnection(db, mock);
  auto txn = internal::MakeSingleUseTransaction(
      Transaction::SingleUseOptions{Transaction::ReadOnlyOptions{}});
  auto rollback = conn->Rollback({txn});
  EXPECT_EQ(StatusCode::kInvalidArgument, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("Cannot rollback"));
}

TEST(ConnectionImplTest, Rollback_PermanentFailure) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&db, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(db.FullName(), request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _))
      .WillOnce(Invoke([&session_name, &transaction_id](
                           grpc::ClientContext&,
                           spanner_proto::RollbackRequest const& request) {
        EXPECT_EQ(session_name, request.session());
        EXPECT_EQ(transaction_id, request.transaction_id());
        return Status(StatusCode::kPermissionDenied, "uh-oh in Rollback");
      }));

  auto conn = MakeTestConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto begin_transaction =
      [&transaction_id](SessionHolder&, spanner_proto::TransactionSelector& s,
                        std::int64_t) {
        s.set_id(transaction_id);
        return 0;
      };
  internal::Visit(txn, begin_transaction);
  auto rollback = conn->Rollback({txn});
  EXPECT_EQ(StatusCode::kPermissionDenied, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("uh-oh in Rollback"));
}

TEST(ConnectionImplTest, Rollback_TooManyTransientFailures) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&db, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(db.FullName(), request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(
          Invoke([&session_name, &transaction_id](
                     grpc::ClientContext&,
                     spanner_proto::RollbackRequest const& request) {
            EXPECT_EQ(session_name, request.session());
            EXPECT_EQ(transaction_id, request.transaction_id());
            return Status(StatusCode::kUnavailable, "try-again in Rollback");
          }));

  auto conn = MakeTestConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto begin_transaction =
      [&transaction_id](SessionHolder&, spanner_proto::TransactionSelector& s,
                        std::int64_t) {
        s.set_id(transaction_id);
        return 0;
      };
  internal::Visit(txn, begin_transaction);
  auto rollback = conn->Rollback({txn});
  EXPECT_EQ(StatusCode::kUnavailable, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("try-again in Rollback"));
}

TEST(ConnectionImplTest, Rollback_Success) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&db, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(db.FullName(), request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Invoke([&session_name, &transaction_id](
                           grpc::ClientContext&,
                           spanner_proto::RollbackRequest const& request) {
        EXPECT_EQ(session_name, request.session());
        EXPECT_EQ(transaction_id, request.transaction_id());
        return Status();
      }));

  auto conn = MakeConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto begin_transaction =
      [&transaction_id](SessionHolder&, spanner_proto::TransactionSelector& s,
                        std::int64_t) {
        s.set_id(transaction_id);
        return 0;
      };
  internal::Visit(txn, begin_transaction);
  auto rollback = conn->Rollback({txn});
  EXPECT_STATUS_OK(rollback);
}

TEST(ConnectionImplTest, PartitionReadSuccess) {
  auto mock_spanner_stub = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock_spanner_stub);
  EXPECT_CALL(*mock_spanner_stub, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  google::spanner::v1::PartitionResponse partition_response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        partitions: { partition_token: "BADDECAF" }
        partitions: { partition_token: "DEADBEEF" }
        transaction: { id: "CAFEDEAD" }
      )pb",
      &partition_response));

  google::spanner::v1::PartitionReadRequest partition_request;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        session: "test-session-name"
        transaction: {
          begin { read_only { strong: true return_read_timestamp: true } }
        }
        table: "table"
        columns: "UserId"
        columns: "UserName"
        key_set: { all: true }
        partition_options: {}
      )pb",
      &partition_request));

  EXPECT_CALL(
      *mock_spanner_stub,
      PartitionRead(_, spanner_testing::IsProtoEqual(partition_request)))
      .WillOnce(Return(partition_response));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  StatusOr<std::vector<ReadPartition>> result = conn->PartitionRead(
      {{txn, "table", KeySet::All(), {"UserId", "UserName"}, ReadOptions()},
       PartitionOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "CAFEDEAD"));

  std::vector<ReadPartition> expected_read_partitions = {
      internal::MakeReadPartition("CAFEDEAD", "test-session-name", "BADDECAF",
                                  "table", KeySet::All(),
                                  {"UserId", "UserName"}),
      internal::MakeReadPartition("CAFEDEAD", "test-session-name", "DEADBEEF",
                                  "table", KeySet::All(),
                                  {"UserId", "UserName"})};

  EXPECT_THAT(*result, testing::UnorderedPointwise(testing::Eq(),
                                                   expected_read_partitions));
}

TEST(ConnectionImplTest, PartitionReadFailure) {
  auto mock_spanner_stub = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock_spanner_stub);
  EXPECT_CALL(*mock_spanner_stub, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  Status failed_status = Status(StatusCode::kPermissionDenied, "End of line.");
  EXPECT_CALL(*mock_spanner_stub, PartitionRead(_, _))
      .WillOnce(Return(failed_status));

  StatusOr<std::vector<ReadPartition>> result = conn->PartitionRead(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
        "table",
        KeySet::All(),
        {"UserId", "UserName"},
        ReadOptions()},
       PartitionOptions()});
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), failed_status);
}

TEST(ConnectionImplTest, PartitionQuerySuccess) {
  auto mock_spanner_stub = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock_spanner_stub);
  EXPECT_CALL(*mock_spanner_stub, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  google::spanner::v1::PartitionResponse partition_response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        partitions: { partition_token: "BADDECAF" }
        partitions: { partition_token: "DEADBEEF" }
        transaction: { id: "CAFEDEAD" }
      )pb",
      &partition_response));

  google::spanner::v1::PartitionQueryRequest partition_request;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        session: "test-session-name"
        transaction: {
          begin { read_only { strong: true return_read_timestamp: true } }
        }
        sql: "select * from table"
        params: {}
        partition_options: {}
      )pb",
      &partition_request));
  EXPECT_CALL(
      *mock_spanner_stub,
      PartitionQuery(_, spanner_testing::IsProtoEqual(partition_request)))
      .WillOnce(Return(partition_response));

  SqlStatement sql_statement("select * from table");
  StatusOr<std::vector<QueryPartition>> result = conn->PartitionQuery(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()), sql_statement},
       PartitionOptions()});
  EXPECT_STATUS_OK(result);

  std::vector<QueryPartition> expected_query_partitions = {
      internal::MakeQueryPartition("CAFEDEAD", "test-session-name", "BADDECAF",
                                   sql_statement),
      internal::MakeQueryPartition("CAFEDEAD", "test-session-name", "DEADBEEF",
                                   sql_statement)};

  EXPECT_THAT(*result, testing::UnorderedPointwise(testing::Eq(),
                                                   expected_query_partitions));
}

TEST(ConnectionImplTest, PartitionQueryFailure) {
  auto mock_spanner_stub = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock_spanner_stub);
  EXPECT_CALL(*mock_spanner_stub, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  Status failed_status = Status(StatusCode::kPermissionDenied, "End of line.");
  EXPECT_CALL(*mock_spanner_stub, PartitionQuery(_, _))
      .WillOnce(Return(failed_status));

  StatusOr<std::vector<QueryPartition>> result = conn->PartitionQuery(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
        SqlStatement("select * from table")},
       PartitionOptions()});
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), failed_status);
}

TEST(ConnectionImplTest, MultipleThreads) {
  auto db = Database("project", "instance", "database");
  std::string const session_prefix = "test-session-prefix-";
  std::string const transaction_id = "test-txn-id";
  std::atomic<int> session_counter(0);

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillRepeatedly(
          Invoke([&db, &session_prefix, &session_counter](
                     grpc::ClientContext&,
                     spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name(session_prefix +
                             std::to_string(++session_counter));
            return session;
          }));
  EXPECT_CALL(*mock, Rollback(_, _))
      .WillRepeatedly(Invoke(
          [session_prefix](grpc::ClientContext&,
                           spanner_proto::RollbackRequest const& request) {
            EXPECT_THAT(request.session(), StartsWith(session_prefix));
            return Status();
          }));

  auto conn = MakeConnection(db, mock);

  int const per_thread_iterations = 1000;
  auto const thread_count = []() -> unsigned {
    if (std::thread::hardware_concurrency() == 0) {
      return 16;
    }
    return std::thread::hardware_concurrency();
  }();

  auto runner = [](int thread_id, int iterations, Connection* conn) {
    for (int i = 0; i != iterations; ++i) {
      auto txn = MakeReadWriteTransaction();
      auto begin_transaction = [thread_id, i](
                                   SessionHolder&,
                                   spanner_proto::TransactionSelector& s,
                                   std::int64_t) {
        s.set_id("txn-" + std::to_string(thread_id) + ":" + std::to_string(i));
        return 0;
      };
      internal::Visit(txn, begin_transaction);
      auto rollback = conn->Rollback({txn});
      EXPECT_STATUS_OK(rollback);
    }
  };

  std::vector<std::future<void>> tasks;
  for (unsigned i = 0; i != thread_count; ++i) {
    tasks.push_back(std::async(std::launch::async, runner, i,
                               per_thread_iterations, conn.get()));
  }

  for (auto& f : tasks) {
    f.get();
  }
}

/**
 * @test Verify Transactions remain bound to a single Session.
 *
 * This test makes interleaved Read() calls using two separate Transactions,
 * and ensures each Transaction uses the same session consistently.
 */
TEST(ConnectionImplTest, TransactionSessionBinding) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  spanner_proto::Session session1;
  session1.set_name("session-1");
  spanner_proto::Session session2;
  session2.set_name("session-2");
  EXPECT_CALL(*mock,
              CreateSession(_, CreateSessionRequestHasDatabase(db.FullName())))
      .WillOnce(Return(session1))
      .WillOnce(Return(session2));

  constexpr int kNumResponses = 4;
  std::array<spanner_proto::PartialResultSet, kNumResponses> responses;
  std::array<std::unique_ptr<MockGrpcReader>, kNumResponses> readers;
  for (int i = 0; i < kNumResponses; ++i) {
    ASSERT_TRUE(TextFormat::ParseFromString(
        R"pb(
          metadata: {
            row_type: {
              fields: {
                name: "Number",
                type: { code: INT64 }
              }
            }
          }
        )pb",
        &responses[i]));
    // The first two responses are reads from two different "begin"
    // transactions.
    switch (i) {
      case 0:
        responses[i].mutable_metadata()->mutable_transaction()->set_id(
            "ABCDEF01");
        break;
      case 1:
        responses[i].mutable_metadata()->mutable_transaction()->set_id(
            "ABCDEF02");
        break;
    }
    responses[i].add_values()->set_string_value(std::to_string(i));

    readers[i] = make_unique<MockGrpcReader>();
    EXPECT_CALL(*readers[i], Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(responses[i]), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(*readers[i], Finish()).WillOnce(Return(grpc::Status()));
  }

  // Ensure the StreamingRead calls have the expected session and transaction
  // IDs or "begin" set as appropriate.
  {
    InSequence s;
    EXPECT_CALL(
        *mock,
        StreamingRead(_, ReadRequestHasSessionAndBeginTransaction("session-1")))
        .WillOnce(Return(ByMove(std::move(readers[0]))));
    EXPECT_CALL(
        *mock,
        StreamingRead(_, ReadRequestHasSessionAndBeginTransaction("session-2")))
        .WillOnce(Return(ByMove(std::move(readers[1]))));
    EXPECT_CALL(*mock, StreamingRead(_, ReadRequestHasSessionAndTransactionId(
                                            "session-1", "ABCDEF01")))
        .WillOnce(Return(ByMove(std::move(readers[2]))));
    EXPECT_CALL(*mock, StreamingRead(_, ReadRequestHasSessionAndTransactionId(
                                            "session-2", "ABCDEF02")))
        .WillOnce(Return(ByMove(std::move(readers[3]))));
  }

  // Now do the actual reads and verify the results.
  Transaction txn1 = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto result =
      conn->Read({txn1, "table", KeySet::All(), {"Number"}, ReadOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn1, HasSessionAndTransactionId("session-1", "ABCDEF01"));
  for (auto& row : result->Rows<Row<std::int64_t>>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 1);
    EXPECT_EQ(row->get<0>(), 0);
  }

  Transaction txn2 = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  result =
      conn->Read({txn2, "table", KeySet::All(), {"Number"}, ReadOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn2, HasSessionAndTransactionId("session-2", "ABCDEF02"));
  for (auto& row : result->Rows<Row<std::int64_t>>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 1);
    EXPECT_EQ(row->get<0>(), 1);
  }

  result =
      conn->Read({txn1, "table", KeySet::All(), {"Number"}, ReadOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn1, HasSessionAndTransactionId("session-1", "ABCDEF01"));
  for (auto& row : result->Rows<Row<std::int64_t>>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 1);
    EXPECT_EQ(row->get<0>(), 2);
  }

  result =
      conn->Read({txn2, "table", KeySet::All(), {"Number"}, ReadOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn2, HasSessionAndTransactionId("session-2", "ABCDEF02"));
  for (auto& row : result->Rows<Row<std::int64_t>>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 1);
    EXPECT_EQ(row->get<0>(), 3);
  }
}

/**
 * @test Verify if a `Transaction` outlives the `ConnectionImpl` it was used
 * with, it does not call back into the deleted `ConnectionImpl` to release
 * the associated `Session` (which would be detected in asan/msan builds.)
 */
TEST(ConnectionImplTest, TransactionOutlivesConnection) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));

  auto grpc_reader = make_unique<MockGrpcReader>();
  spanner_proto::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(metadata: { transaction: { id: "ABCDEF00" } })pb", &response));
  EXPECT_CALL(*grpc_reader, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(Return(grpc::Status()));
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(std::move(grpc_reader))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto result = conn->Read(
      {txn, "table", KeySet::All(), {"UserId", "UserName"}, ReadOptions()});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "ABCDEF00"));

  // `conn` is the only reference to the `ConnectionImpl`, so dropping it will
  // cause the `ConnectionImpl` object to be deleted, while `txn` and its
  // associated `Session` continues to live on.
  conn.reset();
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
