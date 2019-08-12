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
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
namespace spanner_proto = ::google::spanner::v1;

class MockSpannerStub : public internal::SpannerStub {
 public:
  MOCK_METHOD2(CreateSession, StatusOr<spanner_proto::Session>(
                                  grpc::ClientContext&,
                                  spanner_proto::CreateSessionRequest const&));

  MOCK_METHOD2(GetSession, StatusOr<spanner_proto::Session>(
                               grpc::ClientContext&,
                               spanner_proto::GetSessionRequest const&));

  MOCK_METHOD2(ListSessions, StatusOr<spanner_proto::ListSessionsResponse>(
                                 grpc::ClientContext&,
                                 spanner_proto::ListSessionsRequest const&));

  MOCK_METHOD2(DeleteSession,
               Status(grpc::ClientContext&,
                      spanner_proto::DeleteSessionRequest const&));

  MOCK_METHOD2(ExecuteSql, StatusOr<spanner_proto::ResultSet>(
                               grpc::ClientContext&,
                               spanner_proto::ExecuteSqlRequest const&));

  MOCK_METHOD2(
      ExecuteStreamingSql,
      std::unique_ptr<
          grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>(
          grpc::ClientContext&, spanner_proto::ExecuteSqlRequest const&));

  MOCK_METHOD2(ExecuteBatchDml,
               StatusOr<spanner_proto::ExecuteBatchDmlResponse>(
                   grpc::ClientContext&,
                   spanner_proto::ExecuteBatchDmlRequest const&));

  MOCK_METHOD2(Read,
               StatusOr<spanner_proto::ResultSet>(
                   grpc::ClientContext&, spanner_proto::ReadRequest const&));

  MOCK_METHOD2(
      StreamingRead,
      std::unique_ptr<
          grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>(
          grpc::ClientContext&, spanner_proto::ReadRequest const&));

  MOCK_METHOD2(BeginTransaction,
               StatusOr<spanner_proto::Transaction>(
                   grpc::ClientContext&,
                   spanner_proto::BeginTransactionRequest const&));

  MOCK_METHOD2(Commit,
               StatusOr<spanner_proto::CommitResponse>(
                   grpc::ClientContext&, spanner_proto::CommitRequest const&));

  MOCK_METHOD2(Rollback, Status(grpc::ClientContext&,
                                spanner_proto::RollbackRequest const&));

  MOCK_METHOD2(PartitionQuery,
               StatusOr<spanner_proto::PartitionResponse>(
                   grpc::ClientContext&,
                   spanner_proto::PartitionQueryRequest const&));

  MOCK_METHOD2(PartitionRead, StatusOr<spanner_proto::PartitionResponse>(
                                  grpc::ClientContext&,
                                  spanner_proto::PartitionReadRequest const&));
};

TEST(ConnectionImplTest, CommitGetSessionFailure) {
  auto mock = std::make_shared<MockSpannerStub>();

  auto database_name =
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(database_name, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke(
          [&database_name](grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto commit = conn.Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, CommitCommitFailure) {
  auto mock = std::make_shared<MockSpannerStub>();

  auto database_name =
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(database_name, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke(
          [&database_name](grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(Invoke([](grpc::ClientContext&,
                          spanner_proto::CommitRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        return Status(StatusCode::kPermissionDenied, "uh-oh in Commit");
      }));
  auto commit = conn.Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in Commit"));
}

TEST(ConnectionImplTest, CommitTransactionId) {
  auto mock = std::make_shared<MockSpannerStub>();

  auto database_name =
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(database_name, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke(
          [&database_name](grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            spanner_proto::Session session;
            session.set_name("test-session-name");
            return session;
          }));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(Invoke([](grpc::ClientContext&,
                          spanner_proto::CommitRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        EXPECT_EQ("test-txn-id", request.transaction_id());
        return Status(StatusCode::kPermissionDenied, "uh-oh in Commit");
      }));

  auto txn = MakeReadWriteTransaction();
  internal::Visit(txn, [](spanner_proto::TransactionSelector& s, std::int64_t) {
    s.set_id("test-txn-id");
    return 0;
  });

  auto commit = conn.Commit({txn, {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in Commit"));
}

TEST(ConnectionImplTest, RollbackGetSessionFailure) {
  auto database_name = MakeDatabaseName("project", "instance", "database");

  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke(
          [&database_name](grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  ConnectionImpl conn(database_name, mock);
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn.Rollback({txn});
  EXPECT_EQ(StatusCode::kPermissionDenied, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, RollbackBeginTransaction) {
  auto database_name = MakeDatabaseName("project", "instance", "database");
  std::string const session_name = "test-session-name";

  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&database_name, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(database_name, request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  ConnectionImpl conn(database_name, mock);
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn.Rollback({txn});
  EXPECT_TRUE(rollback.ok());
}

TEST(ConnectionImplTest, RollbackSingleUseTransaction) {
  auto database_name = MakeDatabaseName("project", "instance", "database");
  std::string const session_name = "test-session-name";

  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&database_name, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(database_name, request.database());
        spanner_proto::Session session;
        session.set_name(session_name);
        return session;
      }));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  ConnectionImpl conn(database_name, mock);
  auto txn = internal::MakeSingleUseTransaction(
      Transaction::SingleUseOptions{Transaction::ReadOnlyOptions{}});
  auto rollback = conn.Rollback({txn});
  EXPECT_EQ(StatusCode::kInvalidArgument, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("Cannot rollback"));
}

TEST(ConnectionImplTest, RollbackFailure) {
  auto database_name = MakeDatabaseName("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&database_name, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(database_name, request.database());
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

  ConnectionImpl conn(database_name, mock);
  auto txn = MakeReadWriteTransaction();
  auto begin_transaction =
      [&transaction_id](spanner_proto::TransactionSelector& s, std::int64_t) {
        s.set_id(transaction_id);
        return 0;
      };
  internal::Visit(txn, begin_transaction);
  auto rollback = conn.Rollback({txn});
  EXPECT_EQ(StatusCode::kPermissionDenied, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("uh-oh in Rollback"));
}

TEST(ConnectionImplTest, RollbackSuccess) {
  auto database_name = MakeDatabaseName("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(Invoke([&database_name, &session_name](
                           grpc::ClientContext&,
                           spanner_proto::CreateSessionRequest const& request) {
        EXPECT_EQ(database_name, request.database());
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
        return Status();
      }));

  ConnectionImpl conn(database_name, mock);
  auto txn = MakeReadWriteTransaction();
  auto begin_transaction =
      [&transaction_id](spanner_proto::TransactionSelector& s, std::int64_t) {
        s.set_id(transaction_id);
        return 0;
      };
  internal::Visit(txn, begin_transaction);
  auto rollback = conn.Rollback({txn});
  EXPECT_TRUE(rollback.ok());
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
