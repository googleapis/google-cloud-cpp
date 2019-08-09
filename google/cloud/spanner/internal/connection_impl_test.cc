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

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
namespace spanner_proto = ::google::spanner::v1;

// gmock makes clang-tidy very angry, disable a few warnings that we have no
// control over.
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

TEST(ConnectionImplTest, Commit_GetSessionFailure) {
  auto mock = std::make_shared<MockSpannerStub>();

  auto database_name =
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(database_name, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&database_name](
              grpc::ClientContext&,
              google::spanner::v1::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto commit = conn.Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, Commit_CommitFailure) {
  auto mock = std::make_shared<MockSpannerStub>();

  auto database_name =
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(database_name, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&database_name](
              grpc::ClientContext&,
              google::spanner::v1::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            google::spanner::v1::Session session;
            session.set_name("test-session-name");
            return session;
          }));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(::testing::Invoke(
          [](grpc::ClientContext&,
             google::spanner::v1::CommitRequest const& request) {
            EXPECT_EQ("test-session-name", request.session());
            return Status(StatusCode::kPermissionDenied, "uh-oh in Commit");
          }));
  auto commit = conn.Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in Commit"));
}

TEST(ConnectionImplTest, Commit_TransactionId) {
  auto mock = std::make_shared<MockSpannerStub>();

  auto database_name =
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(database_name, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&database_name](
              grpc::ClientContext&,
              google::spanner::v1::CreateSessionRequest const& request) {
            EXPECT_EQ(database_name, request.database());
            google::spanner::v1::Session session;
            session.set_name("test-session-name");
            return session;
          }));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(::testing::Invoke(
          [](grpc::ClientContext&,
             google::spanner::v1::CommitRequest const& request) {
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

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
