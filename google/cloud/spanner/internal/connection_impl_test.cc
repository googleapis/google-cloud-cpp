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
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
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
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StartsWith;

namespace spanner_proto = ::google::spanner::v1;

class MockGrpcReader
    : public ::grpc::ClientReaderInterface<spanner_proto::PartialResultSet> {
 public:
  MOCK_METHOD1(Read, bool(spanner_proto::PartialResultSet*));
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD0(WaitForInitialMetadata, void());
};

TEST(ConnectionImplTest, ReadGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto result =
      conn.Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
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
  ConnectionImpl conn(db, mock);
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
      conn.Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
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
  ConnectionImpl conn(db, mock);
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
      conn.Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                 "table",
                 KeySet::All(),
                 {"UserId", "UserName"},
                 ReadOptions()});
  EXPECT_STATUS_OK(result);
  std::array<std::pair<std::int64_t, std::string>, 2> expected = {
      {std::make_pair(12, "Steve"), std::make_pair(42, "Ann")}};
  int row_number = 0;
  for (auto& row : result->Rows<std::int64_t, std::string>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 2);
    EXPECT_EQ(row->get<0>(), expected[row_number].first);
    EXPECT_EQ(row->get<1>(), expected[row_number].second);
    ++row_number;
  }
  EXPECT_EQ(row_number, 2);
}

TEST(ConnectionImplTest, ExecuteSqlGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(::testing::Invoke(
          [&db](grpc::ClientContext&,
                spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto result =
      conn.ExecuteSql({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                       SqlStatement("select * from table")});
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, ExecuteSqlStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
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

  auto result =
      conn.ExecuteSql({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                       SqlStatement("select * from table")});
  EXPECT_EQ(StatusCode::kPermissionDenied, result.status().code());
  EXPECT_THAT(result.status().message(),
              HasSubstr("uh-oh in GrpcReader::Finish"));
}

TEST(ConnectionImplTest, ExecuteSqlReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
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

  auto result =
      conn.ExecuteSql({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                       SqlStatement("select * from table")});
  EXPECT_STATUS_OK(result);
  std::array<std::pair<std::int64_t, std::string>, 2> expected = {
      {std::make_pair(12, "Steve"), std::make_pair(42, "Ann")}};
  int row_number = 0;
  for (auto& row : result->Rows<std::int64_t, std::string>()) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(row->size(), 2);
    EXPECT_EQ(row->get<0>(), expected[row_number].first);
    EXPECT_EQ(row->get<1>(), expected[row_number].second);
    ++row_number;
  }
  EXPECT_EQ(row_number, 2);
}

TEST(ConnectionImplTest, CommitGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
  EXPECT_CALL(*mock, CreateSession(_, _))
      .WillOnce(
          Invoke([&db](grpc::ClientContext&,
                       spanner_proto::CreateSessionRequest const& request) {
            EXPECT_EQ(db.FullName(), request.database());
            return Status(StatusCode::kPermissionDenied, "uh-oh in GetSession");
          }));

  auto commit = conn.Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in GetSession"));
}

TEST(ConnectionImplTest, CommitCommitFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
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
        return Status(StatusCode::kPermissionDenied, "uh-oh in Commit");
      }));
  auto commit = conn.Commit({MakeReadWriteTransaction(), {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, commit.status().code());
  EXPECT_THAT(commit.status().message(), HasSubstr("uh-oh in Commit"));
}

TEST(ConnectionImplTest, CommitTransactionId) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock);
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

  ConnectionImpl conn(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn.Rollback({txn});
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

  ConnectionImpl conn(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn.Rollback({txn});
  EXPECT_TRUE(rollback.ok());
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

  ConnectionImpl conn(db, mock);
  auto txn = internal::MakeSingleUseTransaction(
      Transaction::SingleUseOptions{Transaction::ReadOnlyOptions{}});
  auto rollback = conn.Rollback({txn});
  EXPECT_EQ(StatusCode::kInvalidArgument, rollback.code());
  EXPECT_THAT(rollback.message(), HasSubstr("Cannot rollback"));
}

TEST(ConnectionImplTest, RollbackFailure) {
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

  ConnectionImpl conn(db, mock);
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
        return Status();
      }));

  ConnectionImpl conn(db, mock);
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

TEST(ConnectionImplTest, PartitionReadSuccess) {
  auto mock_spanner_stub = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  ConnectionImpl conn(db, mock_spanner_stub);
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
  EXPECT_CALL(*mock_spanner_stub, PartitionRead(_, _))
      .WillOnce(Return(partition_response));

  StatusOr<std::vector<ReadPartition>> result = conn.PartitionRead(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
        "table",
        KeySet::All(),
        {"UserId", "UserName"},
        ReadOptions()},
       PartitionOptions()});
  EXPECT_STATUS_OK(result);

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
  ConnectionImpl conn(db, mock_spanner_stub);
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

  StatusOr<std::vector<ReadPartition>> result = conn.PartitionRead(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
        "table",
        KeySet::All(),
        {"UserId", "UserName"},
        ReadOptions()},
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

  ConnectionImpl conn(db, mock);

  int const per_thread_iterations = 1000;
  auto const thread_count = []() -> unsigned {
    if (std::thread::hardware_concurrency() == 0) {
      return 16;
    }
    return std::thread::hardware_concurrency();
  }();

  auto runner = [](int thread_id, int iterations, Connection& conn) {
    for (int i = 0; i != iterations; ++i) {
      auto txn = MakeReadWriteTransaction();
      auto begin_transaction = [thread_id, i](
                                   spanner_proto::TransactionSelector& s,
                                   std::int64_t) {
        s.set_id("txn-" + std::to_string(thread_id) + ":" + std::to_string(i));
        return 0;
      };
      internal::Visit(txn, begin_transaction);
      auto rollback = conn.Rollback({txn});
      EXPECT_TRUE(rollback.ok());
    }
  };

  std::vector<std::future<void>> tasks;
  for (unsigned i = 0; i != thread_count; ++i) {
    tasks.push_back(std::async(std::launch::async, runner, i,
                               per_thread_iterations, std::ref(conn)));
  }

  for (auto& f : tasks) {
    f.get();
  }
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
