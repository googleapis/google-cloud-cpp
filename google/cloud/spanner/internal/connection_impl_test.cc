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
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

// We compile with `-Wextra` which enables `-Wmissing-field-initializers`. This
// warning triggers when aggregate initialization is used with too few
// arguments. For example
//
//   struct A { int a; int b; int c; };  // 3 fields
//   A a = {1, 2};  // <-- Warning, missing initializer for A::c.
//
// To make the test code in this file more readable, we disable this warning
// and rely on the guranteed behavior of aggregate initialization.
// https://en.cppreference.com/w/cpp/language/aggregate_initialization
// Note: "pragma GCC" works for both GCC and clang. MSVC doesn't warn.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

using ::google::cloud::spanner_testing::HasSessionAndTransactionId;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StartsWith;
using ::testing::UnorderedPointwise;
using ::testing::Unused;

namespace spanner_proto = ::google::spanner::v1;

// Matchers for mock calls.
MATCHER_P(HasSession, session, "request has expected session name") {
  return arg.session() == session;
}

MATCHER_P(HasTransactionId, transaction_id,
          "request has expected transaction id") {
  return arg.transaction().id() == transaction_id;
}

// As above, but for Commit and Rollback requests, which don't have a
// `TransactionSelector` but just store the "naked" ID directly in the proto.
MATCHER_P(HasNakedTransactionId, transaction_id,
          "commit or rollback request has expected transaction id") {
  return arg.transaction_id() == transaction_id;
}

MATCHER(HasBeginTransaction, "request has begin TransactionSelector set") {
  return arg.transaction().has_begin();
}

MATCHER_P(HasDatabase, database, "Request has expected database") {
  return arg.database() == database.FullName();
}

// Matches a `spanner::Transaction` that is bound to a "bad" session.
MATCHER(HasBadSession, "bound to a session that's marked bad") {
  return internal::Visit(
      arg,
      [&](internal::SessionHolder& session,
          StatusOr<google::spanner::v1::TransactionSelector>&, std::int64_t) {
        if (!session) {
          *result_listener << "has no session";
          return false;
        }
        if (!session->is_bad()) {
          *result_listener << "session expected to be bad, but was not";
          return false;
        }
        return true;
      });
}

// Ideally this would be a matcher, but matcher args are `const` and `RowStream`
// only has non-const methods.
bool ContainsNoRows(RowStream& rows) { return rows.begin() == rows.end(); }

// Helper to set the Transaction's ID. Requires selector.ok().
void SetTransactionId(Transaction& txn, std::string tid) {
  internal::Visit(txn,
                  [&tid](SessionHolder&,
                         StatusOr<spanner_proto::TransactionSelector>& selector,
                         std::int64_t) {
                    selector->set_id(std::move(tid));
                    return 0;
                  });
}

// Helper to mark the Transaction as invalid.
void SetTransactionInvalid(Transaction& txn, Status status) {
  internal::Visit(
      txn, [&status](SessionHolder&,
                     StatusOr<spanner_proto::TransactionSelector>& selector,
                     std::int64_t) {
        selector = std::move(status);
        return 0;
      });
}

// Helper to create a Transaction proto with a specified (or default) id.
spanner_proto::Transaction MakeTestTransaction(std::string id = "1234567890") {
  spanner_proto::Transaction txn;
  txn.set_id(std::move(id));
  return txn;
}

// Create a `BatchCreateSessionsResponse` with the given `sessions`.
spanner_proto::BatchCreateSessionsResponse MakeSessionsResponse(
    std::vector<std::string> sessions) {
  spanner_proto::BatchCreateSessionsResponse response;
  for (auto& session : sessions) {
    response.add_session()->set_name(std::move(session));
  }
  return response;
}

// Create a `CommitResponse with the given `commit_timestamp`.
spanner_proto::CommitResponse MakeCommitResponse(Timestamp commit_timestamp) {
  spanner_proto::CommitResponse response;
  *response.mutable_commit_timestamp() =
      commit_timestamp.get<protobuf::Timestamp>().value();
  return response;
}

// Create a `Connection` suitable for use in tests that continue retrying
// until the retry policy is exhausted - attempting that with the default
// policies would take too long (10 minutes).
// Other tests can use this method or just call `MakeConnection()` directly.
std::shared_ptr<Connection> MakeLimitedRetryConnection(
    Database const& db,
    std::shared_ptr<spanner_testing::MockSpannerStub> mock) {
  return MakeConnection(
      db, {std::move(mock)},
      ConnectionOptions{grpc::InsecureChannelCredentials()},
      SessionPoolOptions{},
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

// Creates a MockGrpcReader that yields the specified `PartialResultSet`
// `responses` (which can be given in proto or text format) in sequence.
std::unique_ptr<MockGrpcReader> MakeReader(
    std::vector<spanner_proto::PartialResultSet> responses) {
  auto reader = absl::make_unique<MockGrpcReader>();
  Sequence s;
  for (auto& response : responses) {
    EXPECT_CALL(*reader, Read(_))
        .InSequence(s)
        .WillOnce(DoAll(SetArgPointee<0>(std::move(response)), Return(true)));
  }
  EXPECT_CALL(*reader, Read(_)).InSequence(s).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).InSequence(s).WillOnce(Return(grpc::Status()));
  return reader;
}

std::unique_ptr<MockGrpcReader> MakeReader(
    std::vector<std::string> const& responses) {
  std::vector<spanner_proto::PartialResultSet> response_protos;
  response_protos.resize(responses.size());
  for (std::size_t i = 0; i < responses.size(); ++i) {
    if (!TextFormat::ParseFromString(responses[i], &response_protos[i])) {
      ADD_FAILURE() << "Failed to parse proto " << responses[i];
    }
  }
  return MakeReader(std::move(response_protos));
}

// Creates a MockGrpcReader that fails and yields the specified `status`.
std::unique_ptr<MockGrpcReader> MakeFailingReader(grpc::Status status) {
  auto reader = absl::make_unique<MockGrpcReader>();
  EXPECT_CALL(*reader, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(std::move(status)));
  return reader;
}

TEST(ConnectionImplTest, ReadGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  auto rows =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"column1"}});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GetSession")));
  }
}

TEST(ConnectionImplTest, ReadStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  grpc::Status finish_status(grpc::StatusCode::PERMISSION_DENIED,
                             "uh-oh in GrpcReader::Finish");
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(finish_status))));

  auto rows =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"column1"}});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GrpcReader::Finish")));
  }
}

TEST(ConnectionImplTest, ReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));

  grpc::Status retry_status(grpc::StatusCode::UNAVAILABLE, "try-again");
  auto constexpr kText = R"pb(
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
  )pb";
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(retry_status))))
      .WillOnce(Return(ByMove(MakeReader({kText}))));

  auto rows =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"UserId", "UserName"}});
  using RowType = std::tuple<std::int64_t, std::string>;
  auto expected = std::vector<RowType>{
      RowType(12, "Steve"),
      RowType(42, "Ann"),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
}

TEST(ConnectionImplTest, ReadPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(
          MakeFailingReader({grpc::StatusCode::PERMISSION_DENIED, "uh-oh"}))));

  auto rows =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"UserId", "UserName"}});
  for (auto& row : rows) {
    EXPECT_THAT(row,
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  }
}

TEST(ConnectionImplTest, ReadTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .Times(AtLeast(2))
      // This won't compile without `Unused` despite what the gMock docs say.
      .WillRepeatedly([](Unused, Unused) {
        return MakeFailingReader({grpc::StatusCode::UNAVAILABLE, "try-again"});
      });

  auto rows =
      conn->Read({MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
                  "table",
                  KeySet::All(),
                  {"UserId", "UserName"}});
  for (auto& row : rows) {
    EXPECT_THAT(row,
                StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
  }
}

/// @test Verify implicit "begin transaction" in Read() works.
TEST(ConnectionImplTest, ReadImplicitBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kText = R"pb(metadata: { transaction: { id: "ABCDEF00" } })pb";
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(MakeReader({kText}))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto rows = conn->Read({txn, "table", KeySet::All(), {"UserId", "UserName"}});
  EXPECT_TRUE(ContainsNoRows(rows));
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "ABCDEF00"));
}

TEST(ConnectionImplTest, ReadImplicitBeginTransactionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  grpc::Status grpc_status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh");
  auto reader1 = MakeFailingReader(grpc_status);
  auto reader2 = MakeFailingReader(grpc_status);
  // n.b. these calls are explicitly sequenced because using the scoped
  // `InSequence` object causes gMock to get confused by the reader calls.
  Sequence s;
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .InSequence(s)
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasBeginTransaction())))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(reader1))));
  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .InSequence(s)
      .WillOnce(Return(MakeTestTransaction("FEDCBA98")));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasTransactionId("FEDCBA98"))))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(reader2))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto rows = conn->Read({txn, "table", KeySet::All(), {"UserId", "UserName"}});
  for (auto& row : rows) {
    EXPECT_THAT(row,
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  }
}

TEST(ConnectionImplTest, ExecuteQueryGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GetSession")));
  }
}

TEST(ConnectionImplTest, ExecuteQueryStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(
          Return(ByMove(MakeFailingReader({grpc::StatusCode::PERMISSION_DENIED,
                                           "uh-oh in GrpcReader::Finish"}))));

  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GrpcReader::Finish")));
  }
}

TEST(ConnectionImplTest, ExecuteQueryReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kText = R"pb(
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
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeReader({kText}))));

  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  using RowType = std::tuple<std::int64_t, std::string>;
  auto expected = std::vector<RowType>{
      RowType(12, "Steve"),
      RowType(42, "Ann"),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());
}

/// @test Verify implicit "begin transaction" in ExecuteQuery() works.
TEST(ConnectionImplTest, ExecuteQueryImplicitBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kText = R"pb(metadata: { transaction: { id: "00FEDCBA" } })pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeReader({kText}))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto rows = conn->ExecuteQuery({txn, SqlStatement("select * from table")});
  EXPECT_TRUE(ContainsNoRows(rows));
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "00FEDCBA"));
}

TEST(ConnectionImplTest, QueryOptions) {
  auto constexpr kQueryOptionsProp =
      &spanner_proto::ExecuteSqlRequest::query_options;
  std::vector<absl::optional<std::string>> const optimizer_versions = {
      {}, "", "some-version"};

  for (auto const& version : optimizer_versions) {
    spanner_proto::ExecuteSqlRequest::QueryOptions qo;
    if (version) qo.set_optimizer_version(*version);
    auto m = Property(kQueryOptionsProp, IsProtoEqual(qo));
    auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

    auto txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
    auto query_options = QueryOptions().set_optimizer_version(version);
    auto params = Connection::SqlParams{txn, SqlStatement{}, query_options};
    auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
    auto conn = MakeConnection(
        db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));

    // We wrap MockGrpcReader in NiceMock, because we don't really care how
    // it's called in this test (aside from needing to return a transaction in
    // the first call), and we want to minimize gMock's "uninteresting mock
    // function call" warnings.
    using ::testing::NiceMock;
    auto grpc_reader = absl::make_unique<NiceMock<MockGrpcReader>>();
    auto constexpr kResponseText =
        R"pb(metadata: { transaction: { id: "ABCDEF00" } })pb";
    spanner_proto::PartialResultSet response;
    ASSERT_TRUE(TextFormat::ParseFromString(kResponseText, &response));
    EXPECT_CALL(*grpc_reader, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));

    // Calls the 5 Connection::* methods that take SqlParams and ensures that
    // the protos being sent contain the expected options.
    EXPECT_CALL(*mock, ExecuteStreamingSql(_, m))
        .WillOnce(Return(ByMove(std::move(grpc_reader))))
        .WillOnce(
            Return(ByMove(absl::make_unique<NiceMock<MockGrpcReader>>())));
    (void)conn->ExecuteQuery(params);
    (void)conn->ProfileQuery(params);

    EXPECT_CALL(*mock, ExecuteSql(_, m)).Times(3);
    (void)conn->ExecuteDml(params);
    (void)conn->ProfileDml(params);
    (void)conn->AnalyzeSql(params);
  }
}

TEST(ConnectionImplTest, ExecuteDmlGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ExecuteDml({txn, SqlStatement("delete * from table")});

  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in GetSession")));
}

TEST(ConnectionImplTest, ExecuteDmlDeleteSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  auto constexpr kText = R"pb(
    metadata: { transaction: { id: "1234567890" } }
    stats: { row_count_exact: 42 }
  )pb";
  spanner_proto::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  EXPECT_CALL(*mock, ExecuteSql(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(response));
  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ExecuteDml({txn, SqlStatement("delete * from table")});

  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->RowsModified(), 42);
}

TEST(ConnectionImplTest, ExecuteDmlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ExecuteDml({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteDml")));
}

TEST(ConnectionImplTest, ExecuteDmlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kUnavailable, "try-again in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ExecuteDml({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteDml")));
}

// Tests that a Transaction that fails to begin does not successfully commit.
// That would violate atomicity since the first DML statement did not execute.
TEST(ConnectionImplTest, ExecuteDmlTransactionAtomicity) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  Status op_status(StatusCode::kInvalidArgument, "ExecuteSql status");
  Status begin_status(StatusCode::kInvalidArgument, "BeginTransaction status");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));

    // The first `ExecuteDml` call tries to implicitly begin the transaction
    // via `ExecuteSql`, and then explicitly via `BeginTransaction`. Both fail,
    // and we should receive no further RPC calls - since the transaction is
    // not valid the client fails any subsequent operations itself.
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(op_status));
    EXPECT_CALL(*mock, BeginTransaction(_, _)).WillOnce(Return(begin_status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  // The first operation fails with the status of the operation's RPC
  // (`ExecuteSql` in this case).
  EXPECT_THAT(conn->ExecuteDml({txn, SqlStatement("some statement")}),
              StatusIs(op_status.code(), HasSubstr(op_status.message())));
  // Subsequent operations fail with the status of `BeginTransaction`.
  EXPECT_THAT(conn->ExecuteDml({txn, SqlStatement("another statement")}),
              StatusIs(begin_status.code(), HasSubstr(begin_status.message())));
  EXPECT_THAT(conn->Commit({txn, {}}),
              StatusIs(begin_status.code(), HasSubstr(begin_status.message())));
}

TEST(ConnectionImplTest, ExecuteDmlTransactionMissing) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  // Return an otherwise valid response that does not contain a transaction.
  spanner_proto::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString("metadata: {}", &response));
  EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(response));

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  EXPECT_THAT(
      conn->ExecuteDml({txn, SqlStatement("select 1")}),
      StatusIs(StatusCode::kInternal,
               HasSubstr(
                   "Begin transaction requested but no transaction returned")));
}

TEST(ConnectionImplTest, ProfileQuerySuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  auto constexpr kText = R"pb(
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
    stats: {
      query_plan { plan_nodes: { index: 42 } }
      query_stats {
        fields {
          key: "elapsed_time"
          value { string_value: "42 secs" }
        }
      }
    }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeReader({kText}))));

  auto result = conn->ProfileQuery(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  using RowType = std::tuple<std::int64_t, std::string>;
  auto expected = std::vector<RowType>{
      RowType(12, "Steve"),
      RowType(42, "Ann"),
  };
  int row_number = 0;
  for (auto& row : StreamOf<RowType>(result)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(*row, expected[row_number]);
    ++row_number;
  }
  EXPECT_EQ(row_number, expected.size());

  auto constexpr kTextExpectedPlan = R"pb(
    plan_nodes: { index: 42 }
  )pb";
  google::spanner::v1::QueryPlan expected_plan;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextExpectedPlan, &expected_plan));

  auto plan = result.ExecutionPlan();
  ASSERT_TRUE(plan);
  EXPECT_THAT(*plan, IsProtoEqual(expected_plan));

  std::vector<std::pair<const std::string, std::string>> expected_stats;
  expected_stats.emplace_back("elapsed_time", "42 secs");
  auto execution_stats = result.ExecutionStats();
  ASSERT_TRUE(execution_stats);
  EXPECT_THAT(*execution_stats, UnorderedPointwise(Eq(), expected_stats));
}

TEST(ConnectionImplTest, ProfileQueryGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  auto result = conn->ProfileQuery(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  for (auto& row : result) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GetSession")));
  }
}

TEST(ConnectionImplTest, ProfileQueryStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  grpc::Status finish_status(grpc::StatusCode::PERMISSION_DENIED,
                             "uh-oh in GrpcReader::Finish");
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(finish_status))));

  auto result = conn->ProfileQuery(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  for (auto& row : result) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GrpcReader::Finish")));
  }
}

TEST(ConnectionImplTest, ProfileDmlGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ProfileDml({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in GetSession")));
}

TEST(ConnectionImplTest, ProfileDmlDeleteSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  auto constexpr kText = R"pb(
    metadata: { transaction: { id: "1234567890" } }
    stats: {
      row_count_exact: 42
      query_plan { plan_nodes: { index: 42 } }
      query_stats {
        fields {
          key: "elapsed_time"
          value { string_value: "42 secs" }
        }
      }
    }
  )pb";
  spanner_proto::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  EXPECT_CALL(*mock, ExecuteSql(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(response));
  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ProfileDml({txn, SqlStatement("delete * from table")});

  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->RowsModified(), 42);

  auto constexpr kTextExpectedPlan = R"pb(
    plan_nodes: { index: 42 }
  )pb";
  google::spanner::v1::QueryPlan expected_plan;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextExpectedPlan, &expected_plan));

  auto plan = result->ExecutionPlan();
  ASSERT_TRUE(plan);
  EXPECT_THAT(*plan, IsProtoEqual(expected_plan));

  std::vector<std::pair<const std::string, std::string>> expected_stats;
  expected_stats.emplace_back("elapsed_time", "42 secs");
  auto execution_stats = result->ExecutionStats();
  ASSERT_TRUE(execution_stats);
  EXPECT_THAT(*execution_stats, UnorderedPointwise(Eq(), expected_stats));
}

TEST(ConnectionImplTest, ProfileDmlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ProfileDml({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteDml")));
}

TEST(ConnectionImplTest, ProfileDmlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kUnavailable, "try-again in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->ProfileDml({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteDml")));
}

TEST(ConnectionImplTest, AnalyzeSqlSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  auto constexpr kText = R"pb(
    metadata: {}
    stats: { query_plan { plan_nodes: { index: 42 } } }
  )pb";
  spanner_proto::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteSql(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(ByMove(std::move(response))));

  auto result = conn->AnalyzeSql(
      {MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});

  auto constexpr kTextExpectedPlan = R"pb(
    plan_nodes: { index: 42 }
  )pb";
  google::spanner::v1::QueryPlan expected_plan;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextExpectedPlan, &expected_plan));

  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, IsProtoEqual(expected_plan));
}

TEST(ConnectionImplTest, AnalyzeSqlGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->AnalyzeSql({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in GetSession")));
}

TEST(ConnectionImplTest, AnalyzeSqlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql(_, _)).WillOnce(Return(status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->AnalyzeSql({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteDml")));
}

TEST(ConnectionImplTest, AnalyzeSqlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kUnavailable, "try-again in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  Transaction txn = MakeReadWriteTransaction(Transaction::ReadWriteOptions());
  auto result = conn->AnalyzeSql({txn, SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteDml")));
}

TEST(ConnectionImplTest, ExecuteBatchDmlSuccess) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  auto constexpr kText = R"pb(
    result_sets: {
      metadata: { transaction: { id: "1234567890" } }
      stats: { row_count_exact: 0 }
    }
    result_sets: { stats: { row_count_exact: 1 } }
    result_sets: { stats: { row_count_exact: 2 } }
  )pb";
  spanner_proto::ExecuteBatchDmlResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteBatchDml(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(response));

  auto request = {
      SqlStatement("update ..."),
      SqlStatement("update ..."),
      SqlStatement("update ..."),
  };

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
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

TEST(ConnectionImplTest, ExecuteBatchDmlPartialFailure) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  auto constexpr kText = R"pb(
    result_sets: {
      metadata: { transaction: { id: "1234567890" } }
      stats: { row_count_exact: 42 }
    }
    result_sets: { stats: { row_count_exact: 43 } }
    status: { code: 2 message: "oops" }
  )pb";
  spanner_proto::ExecuteBatchDmlResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteBatchDml(_, _)).WillOnce(Return(response));

  auto request = {
      SqlStatement("update ..."),
      SqlStatement("update ..."),
      SqlStatement("update ..."),
  };

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  auto txn = MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(result->status, StatusIs(StatusCode::kUnknown, "oops"));
  EXPECT_NE(result->stats.size(), request.size());
  EXPECT_EQ(result->stats.size(), 2);
  EXPECT_EQ(result->stats[0].row_count, 42);
  EXPECT_EQ(result->stats[1].row_count, 43);
  EXPECT_THAT(txn, HasSessionAndTransactionId("session-name", "1234567890"));
}

TEST(ConnectionImplTest, ExecuteBatchDmlPermanentFailure) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));

    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteBatchDml");
    EXPECT_CALL(*mock, ExecuteBatchDml(_, _)).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteBatchDml(_, _)).WillOnce(Return(status));
  }

  auto request = {
      SqlStatement("update ..."),
      SqlStatement("update ..."),
      SqlStatement("update ..."),
  };

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteBatchDml")));
}

TEST(ConnectionImplTest, ExecuteBatchDmlTooManyTransientFailures) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));

    Status status(StatusCode::kUnavailable, "try-again in ExecuteBatchDml");
    EXPECT_CALL(*mock, ExecuteBatchDml(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteBatchDml(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  auto request = {
      SqlStatement("update ..."),
      SqlStatement("update ..."),
      SqlStatement("update ..."),
  };

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteBatchDml")));
}

TEST(ConnectionImplTest, ExecuteBatchDmlNoResultSets) {
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, _))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    // The `ExecuteBatchDml` call can succeed, but with no `ResultSet`s and an
    // error status in the response.
    auto constexpr kText =
        R"pb(status: { code: 6 message: "failed to insert ..." })pb";
    spanner_proto::ExecuteBatchDmlResponse response;
    ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
    EXPECT_CALL(*mock, ExecuteBatchDml(_, AllOf(HasSession("session-name"),
                                                HasBeginTransaction())))
        .WillOnce(Return(response));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction("BD000001")));
    EXPECT_CALL(*mock, ExecuteBatchDml(_, AllOf(HasSession("session-name"),
                                                HasTransactionId("BD000001"))))
        .WillOnce(Return(response));
  }

  auto request = {SqlStatement("update ...")};
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  auto txn = MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_STATUS_OK(result);
  EXPECT_THAT(result->status, StatusIs(StatusCode::kAlreadyExists,
                                       HasSubstr("failed to insert ...")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlDeleteSuccess) {
  google::cloud::LogSink::EnableStdClog();
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeTestTransaction()));

  auto constexpr kTextResponse = R"pb(metadata: {}
                                      stats: { row_count_lower_bound: 42 })pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(
          ByMove(MakeFailingReader({grpc::StatusCode::UNAVAILABLE,
                                    "try-again in ExecutePartitionedDml"}))))
      .WillOnce(Return(ByMove(MakeReader({kTextResponse}))));

  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});

  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->row_count_lower_bound, 42);
}

TEST(ConnectionImplTest, ExecutePartitionedDmlGetSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in GetSession")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeTestTransaction()));

  // A `kInternal` status can be treated as transient based on the message.
  // This tests that other `kInternal` errors are treated as permanent.
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(
          {grpc::StatusCode::INTERNAL, "permanent failure"}))));
  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInternal, HasSubstr("permanent failure")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeTestTransaction()));

  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .Times(AtLeast(2))
      // This won't compile without `Unused` despite what the gMock docs say.
      .WillRepeatedly([](Unused, Unused) {
        return MakeFailingReader({grpc::StatusCode::UNAVAILABLE,
                                  "try-again in ExecutePartitionedDml"});
      });

  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kUnavailable,
                       HasSubstr("try-again in ExecutePartitionedDml")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlRetryableInternalErrors) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  EXPECT_CALL(*mock, BatchCreateSessions(_, _))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(MakeTestTransaction("2345678901")));

  auto constexpr kTextResponse =
      R"pb(metadata: {}
           stats: { row_count_lower_bound: 99999 })pb";

  // `kInternal` is usually a permanent failure, but if the message indicates
  // the gRPC connection has been closed (which these do), they are treated as
  // transient failures.
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(
          {grpc::StatusCode::INTERNAL,
           "Received unexpected EOS on DATA frame from server"}))))
      .WillOnce(Return(ByMove(MakeFailingReader(
          {grpc::StatusCode::INTERNAL, "HTTP/2 error code: INTERNAL_ERROR"}))))
      .WillOnce(Return(ByMove(MakeReader({kTextResponse}))));

  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});

  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->row_count_lower_bound, 99999);
}

TEST(ConnectionImplTest,
     ExecutePartitionedDmlDeleteBeginTransactionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied,
                              "uh-oh in ExecutePartitionedDml")));

  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecutePartitionedDml")));
}

TEST(ConnectionImplTest,
     ExecutePartitionedDmlDeleteBeginTransactionTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable,
                                    "try-again in ExecutePartitionedDml")));

  auto result =
      conn->ExecutePartitionedDml({SqlStatement("delete * from table")});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kUnavailable,
                       HasSubstr("try-again in ExecutePartitionedDml")));
}

TEST(ConnectionImplTest, CommitGetSessionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));

  auto commit = conn->Commit({MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in GetSession")));
}

TEST(ConnectionImplTest, CommitGetSessionTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kUnavailable, "try-again in GetSession")));

  auto commit = conn->Commit({MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in GetSession")));
}

TEST(ConnectionImplTest, CommitGetSessionRetry) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  spanner_proto::Transaction txn = MakeTestTransaction();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kUnavailable, "try-again in GetSession")))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction(_, _)).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Commit")));
  auto commit = conn->Commit({MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in Commit")));
}

TEST(ConnectionImplTest, CommitBeginTransactionRetry) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  spanner_proto::Transaction txn = MakeTestTransaction();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(txn));
  auto const commit_timestamp =
      MakeTimestamp(std::chrono::system_clock::from_time_t(123)).value();
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(Return(MakeCommitResponse(commit_timestamp)));

  auto commit = conn->Commit({MakeReadWriteTransaction()});
  EXPECT_STATUS_OK(commit);
  EXPECT_EQ(commit_timestamp, commit->commit_timestamp);
}

TEST(ConnectionImplTest, CommitBeginTransactionSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));
  auto txn = MakeReadWriteTransaction();
  auto commit = conn->Commit({txn});
  EXPECT_FALSE(commit.ok());
  auto status = commit.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, CommitBeginTransactionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(
          Status(StatusCode::kInvalidArgument, "BeginTransaction failed")));
  auto txn = MakeReadWriteTransaction();
  EXPECT_THAT(conn->Commit({txn}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  // Retrying the operation should also fail with the same error, without making
  // an additional `BeginTransaction` call.
  EXPECT_THAT(conn->Commit({txn}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));
}

TEST(ConnectionImplTest, CommitCommitPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  spanner_proto::Transaction txn = MakeTestTransaction();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction(_, _)).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Commit")));
  auto commit = conn->Commit({MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in Commit")));
}

TEST(ConnectionImplTest, CommitCommitTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  spanner_proto::Transaction txn = MakeTestTransaction();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction(_, _)).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Commit")));
  auto commit = conn->Commit({MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in Commit")));
}

TEST(ConnectionImplTest, CommitCommitInvalidatedTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, _)).Times(0);
  EXPECT_CALL(*mock, BeginTransaction(_, _)).Times(0);
  EXPECT_CALL(*mock, Commit(_, _)).Times(0);

  // Committing an invalidated transaction is a unilateral error.
  auto txn = MakeReadWriteTransaction();
  SetTransactionInvalid(txn,
                        Status(StatusCode::kAlreadyExists, "constraint error"));

  auto commit = conn->Commit({txn});
  EXPECT_FALSE(commit.ok());
  auto status = commit.status();
  EXPECT_THAT(commit, StatusIs(StatusCode::kAlreadyExists,
                               HasSubstr("constraint error")));
}

TEST(ConnectionImplTest, CommitCommitIdempotentTransientSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto const commit_timestamp =
      MakeTimestamp(std::chrono::system_clock::from_time_t(123)).value();
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId("test-txn-id"))))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeCommitResponse(commit_timestamp)));

  // Set the id because that makes the commit idempotent.
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");

  auto commit = conn->Commit({txn});
  EXPECT_STATUS_OK(commit);
  EXPECT_EQ(commit_timestamp, commit->commit_timestamp);
}

TEST(ConnectionImplTest, CommitSuccessWithTransactionId) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId("test-txn-id"))))
      .WillOnce(Return(MakeCommitResponse(
          MakeTimestamp(std::chrono::system_clock::from_time_t(123)).value())));

  // Set the id because that makes the commit idempotent.
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");

  auto commit = conn->Commit({txn});
  EXPECT_STATUS_OK(commit);
}

TEST(ConnectionImplTest, RollbackGetSessionFailure) {
  auto db = Database("project", "instance", "database");

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in GetSession")));
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kPermissionDenied,
                                 HasSubstr("uh-oh in GetSession")));
}

TEST(ConnectionImplTest, RollbackBeginTransaction) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "RollbackBeginTransaction";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  EXPECT_CALL(*mock, BeginTransaction(_, _))
      .WillOnce(Return(MakeTestTransaction(transaction_id)));
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .WillOnce(Return(Status()));

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  auto txn = MakeReadWriteTransaction();
  auto rollback = conn->Rollback({txn});
  EXPECT_STATUS_OK(rollback);
}

TEST(ConnectionImplTest, RollbackSingleUseTransaction) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, _)).Times(0);
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  auto txn = internal::MakeSingleUseTransaction(
      Transaction::SingleUseOptions{Transaction::ReadOnlyOptions{}});
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("Cannot rollback")));
}

TEST(ConnectionImplTest, RollbackPermanentFailure) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Rollback")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, transaction_id);
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kPermissionDenied,
                                 HasSubstr("uh-oh in Rollback")));
}

TEST(ConnectionImplTest, RollbackTooManyTransientFailures) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kUnavailable, "try-again in Rollback")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, transaction_id);
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kUnavailable,
                                 HasSubstr("try-again in Rollback")));
}

TEST(ConnectionImplTest, RollbackSuccess) {
  auto db = Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  std::string const transaction_id = "test-txn-id";

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(Status()));

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, transaction_id);
  auto rollback = conn->Rollback({txn});
  EXPECT_STATUS_OK(rollback);
}

TEST(ConnectionImplTest, RollbackInvalidatedTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, _)).Times(0);
  EXPECT_CALL(*mock, Rollback(_, _)).Times(0);

  // Rolling back an invalidated transaction is a unilateral success.
  auto txn = MakeReadWriteTransaction();
  SetTransactionInvalid(txn,
                        Status(StatusCode::kAlreadyExists, "constraint error"));

  auto rollback_status = conn->Rollback({txn});
  EXPECT_THAT(rollback_status, StatusIs(StatusCode::kAlreadyExists,
                                        HasSubstr("constraint error")));
}

TEST(ConnectionImplTest, PartitionReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kTextPartitionResponse = R"pb(
    partitions: { partition_token: "BADDECAF" }
    partitions: { partition_token: "DEADBEEF" }
    transaction: { id: "CAFEDEAD" }
  )pb";
  google::spanner::v1::PartitionResponse partition_response;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextPartitionResponse, &partition_response));

  auto constexpr kTextPartitionRequest = R"pb(
    session: "test-session-name"
    transaction: {
      begin { read_only { strong: true return_read_timestamp: true } }
    }
    table: "table"
    columns: "UserId"
    columns: "UserName"
    key_set: { all: true }
    partition_options: {}
  )pb";
  google::spanner::v1::PartitionReadRequest partition_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextPartitionRequest, &partition_request));

  EXPECT_CALL(*mock, PartitionRead(_, IsProtoEqual(partition_request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(partition_response));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  StatusOr<std::vector<ReadPartition>> result = conn->PartitionRead(
      {{txn, "table", KeySet::All(), {"UserId", "UserName"}}});
  ASSERT_STATUS_OK(result);
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

TEST(ConnectionImplTest, PartitionReadPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh");
    EXPECT_CALL(*mock, PartitionRead(_, _)).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionRead(_, _)).WillOnce(Return(status));
  }

  StatusOr<std::vector<ReadPartition>> result = conn->PartitionRead(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
        "table",
        KeySet::All(),
        {"UserId", "UserName"}}});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(ConnectionImplTest, PartitionReadTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    Status status(StatusCode::kUnavailable, "try-again");
    EXPECT_CALL(*mock, PartitionRead(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionRead(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  StatusOr<std::vector<ReadPartition>> result = conn->PartitionRead(
      {{MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
        "table",
        KeySet::All(),
        {"UserId", "UserName"}}});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(ConnectionImplTest, PartitionQuerySuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kTextPartitionResponse = R"pb(
    partitions: { partition_token: "BADDECAF" }
    partitions: { partition_token: "DEADBEEF" }
    transaction: { id: "CAFEDEAD" }
  )pb";
  google::spanner::v1::PartitionResponse partition_response;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextPartitionResponse, &partition_response));

  auto constexpr kTextPartitionRequest = R"pb(
    session: "test-session-name"
    transaction: {
      begin { read_only { strong: true return_read_timestamp: true } }
    }
    sql: "select * from table"
    params: {}
    partition_options: {}
  )pb";
  google::spanner::v1::PartitionQueryRequest partition_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextPartitionRequest, &partition_request));
  EXPECT_CALL(*mock, PartitionQuery(_, IsProtoEqual(partition_request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(partition_response));

  SqlStatement sql_statement("select * from table");
  StatusOr<std::vector<QueryPartition>> result = conn->PartitionQuery(
      {MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()), sql_statement});
  ASSERT_STATUS_OK(result);

  std::vector<QueryPartition> expected_query_partitions = {
      internal::MakeQueryPartition("CAFEDEAD", "test-session-name", "BADDECAF",
                                   sql_statement),
      internal::MakeQueryPartition("CAFEDEAD", "test-session-name", "DEADBEEF",
                                   sql_statement)};

  EXPECT_THAT(*result, testing::UnorderedPointwise(testing::Eq(),
                                                   expected_query_partitions));
}

TEST(ConnectionImplTest, PartitionQueryPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  Status failed_status = Status(StatusCode::kPermissionDenied, "End of line.");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    EXPECT_CALL(*mock, PartitionQuery(_, _)).WillOnce(Return(failed_status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionQuery(_, _)).WillOnce(Return(failed_status));
  }

  StatusOr<std::vector<QueryPartition>> result = conn->PartitionQuery(
      {MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr(failed_status.message())));
}

TEST(ConnectionImplTest, PartitionQueryTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeLimitedRetryConnection(db, mock);

  Status failed_status =
      Status(StatusCode::kUnavailable, "try-again in PartitionQuery");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    EXPECT_CALL(*mock, PartitionQuery(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(failed_status));
    EXPECT_CALL(*mock, BeginTransaction(_, _))
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionQuery(_, _))
        .Times(AtLeast(2))
        .WillRepeatedly(Return(failed_status));
  }

  StatusOr<std::vector<QueryPartition>> result = conn->PartitionQuery(
      {MakeReadOnlyTransaction(Transaction::ReadOnlyOptions()),
       SqlStatement("select * from table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr(failed_status.message())));
}

TEST(ConnectionImplTest, MultipleThreads) {
  auto db = Database("project", "instance", "database");
  std::string const session_prefix = "test-session-prefix-";
  std::string const transaction_id = "test-txn-id";
  std::atomic<int> session_counter(0);

  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillRepeatedly(
          [&session_prefix, &session_counter](
              grpc::ClientContext&,
              spanner_proto::BatchCreateSessionsRequest const& request) {
            spanner_proto::BatchCreateSessionsResponse response;
            for (int i = 0; i < request.session_count(); ++i) {
              response.add_session()->set_name(
                  session_prefix + std::to_string(++session_counter));
            }
            return response;
          });
  EXPECT_CALL(*mock, Rollback(_, _))
      .WillRepeatedly(
          [session_prefix](grpc::ClientContext&,
                           spanner_proto::RollbackRequest const& request) {
            EXPECT_THAT(request.session(), StartsWith(session_prefix));
            return Status();
          });

  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

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
      SetTransactionId(
          txn, "txn-" + std::to_string(thread_id) + ":" + std::to_string(i));
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
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-1"})))
      .WillOnce(Return(MakeSessionsResponse({"session-2"})));

  constexpr int kNumResponses = 4;
  std::array<std::unique_ptr<MockGrpcReader>, kNumResponses> readers;
  for (int i = 0; i < kNumResponses; ++i) {
    auto constexpr kText = R"pb(
      metadata: {
        row_type: {
          fields: {
            name: "Number",
            type: { code: INT64 }
          }
        }
      }
    )pb";
    spanner_proto::PartialResultSet response;
    ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
    // The first two responses are reads from two different "begin"
    // transactions.
    switch (i) {
      case 0:
        *response.mutable_metadata()->mutable_transaction() =
            MakeTestTransaction("ABCDEF01");
        break;
      case 1:
        *response.mutable_metadata()->mutable_transaction() =
            MakeTestTransaction("ABCDEF02");
        break;
    }
    response.add_values()->set_string_value(std::to_string(i));
    readers[i] = MakeReader({std::move(response)});
  }

  // Ensure the StreamingRead calls have the expected session and transaction
  // IDs or "begin" set as appropriate.
  {
    InSequence s;
    EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("session-1"),
                                              HasBeginTransaction())))
        .WillOnce(Return(ByMove(std::move(readers[0]))));
    EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("session-2"),
                                              HasBeginTransaction())))
        .WillOnce(Return(ByMove(std::move(readers[1]))));
    EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("session-1"),
                                              HasTransactionId("ABCDEF01"))))
        .WillOnce(Return(ByMove(std::move(readers[2]))));
    EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("session-2"),
                                              HasTransactionId("ABCDEF02"))))
        .WillOnce(Return(ByMove(std::move(readers[3]))));
  }

  // Now do the actual reads and verify the results.
  Transaction txn1 = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto rows = conn->Read({txn1, "table", KeySet::All(), {"Number"}});
  EXPECT_THAT(txn1, HasSessionAndTransactionId("session-1", "ABCDEF01"));
  for (auto& row : StreamOf<std::tuple<std::int64_t>>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 0);
  }

  Transaction txn2 = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  rows = conn->Read({txn2, "table", KeySet::All(), {"Number"}});
  EXPECT_THAT(txn2, HasSessionAndTransactionId("session-2", "ABCDEF02"));
  for (auto& row : StreamOf<std::tuple<std::int64_t>>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 1);
  }

  rows = conn->Read({txn1, "table", KeySet::All(), {"Number"}});
  EXPECT_THAT(txn1, HasSessionAndTransactionId("session-1", "ABCDEF01"));
  for (auto& row : StreamOf<std::tuple<std::int64_t>>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 2);
  }

  rows = conn->Read({txn2, "table", KeySet::All(), {"Number"}});
  EXPECT_THAT(txn2, HasSessionAndTransactionId("session-2", "ABCDEF02"));
  for (auto& row : StreamOf<std::tuple<std::int64_t>>(rows)) {
    EXPECT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 3);
  }
}

/**
 * @test Verify if a `Transaction` outlives the `ConnectionImpl` it was used
 * with, it does not call back into the deleted `ConnectionImpl` to release
 * the associated `Session` (which would be detected in ASAN/MSAN builds.)
 */
TEST(ConnectionImplTest, TransactionOutlivesConnection) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));

  auto constexpr kText = R"pb(metadata: { transaction: { id: "ABCDEF00" } })pb";
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(MakeReader({kText}))));

  Transaction txn = MakeReadOnlyTransaction(Transaction::ReadOnlyOptions());
  auto rows = conn->Read({txn, "table", KeySet::All(), {"UserId", "UserName"}});
  EXPECT_TRUE(ContainsNoRows(rows));
  EXPECT_THAT(txn, HasSessionAndTransactionId("test-session-name", "ABCDEF00"));

  // `conn` is the only reference to the `ConnectionImpl`, so dropping it will
  // cause the `ConnectionImpl` object to be deleted, while `txn` and its
  // associated `Session` continues to live on.
  conn.reset();
}

TEST(ConnectionImplTest, ReadSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  grpc::Status finish_status(grpc::StatusCode::NOT_FOUND, "Session not found");
  EXPECT_CALL(*mock, StreamingRead(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(finish_status))));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto params = Connection::ReadParams{txn};
  auto response = GetSingularRow(conn->Read(std::move(params)));
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, PartitionReadSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, PartitionRead(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto params = Connection::ReadParams{txn};
  auto response = conn->PartitionRead({params});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecuteQuerySessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  grpc::Status finish_status(grpc::StatusCode::NOT_FOUND, "Session not found");
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(finish_status))));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = GetSingularRow(conn->ExecuteQuery({txn}));
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ProfileQuerySessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  grpc::Status finish_status(grpc::StatusCode::NOT_FOUND, "Session not found");
  EXPECT_CALL(*mock, ExecuteStreamingSql(_, _))
      .WillOnce(Return(ByMove(MakeFailingReader(finish_status))));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = GetSingularRow(conn->ProfileQuery({txn}));
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecuteDmlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteSql(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->ExecuteDml({txn});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ProfileDmlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteSql(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->ProfileDml({txn});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, AnalyzeSqlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteSql(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->AnalyzeSql({txn});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, PartitionQuerySessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, PartitionQuery(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->PartitionQuery({txn});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecuteBatchDmlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteBatchDml(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->ExecuteBatchDml({txn});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecutePartitionedDmlSessionNotFound) {
  // NOTE: There's no test here because this method does not accept a
  // Transaction and so there's no way to extract the Session to check if it's
  // bad. We could modify the API to inject/extract this, but this is a
  // user-facing API that we don't want to mess up.
}

TEST(ConnectionImplTest, CommitSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Commit(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->Commit({txn});
  EXPECT_FALSE(response.ok());
  auto status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, RollbackSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Rollback(_, _))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Session not found")));

  auto conn = MakeLimitedRetryConnection(db, mock);
  auto txn = MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto status = conn->Rollback({txn});
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, OperationsFailOnInvalidatedTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = Database("dummy_project", "dummy_instance", "dummy_database_id");
  auto conn = MakeConnection(
      db, {mock}, ConnectionOptions{grpc::InsecureChannelCredentials()});

  // Committing an invalidated transaction is a unilateral error.
  auto txn = MakeReadWriteTransaction();
  SetTransactionInvalid(
      txn, Status(StatusCode::kInvalidArgument, "BeginTransaction failed"));
  // All operations on an invalid transaction should return the error that
  // invalidated it, without actually making a RPC.

  EXPECT_THAT(
      conn->Read({txn, "table", KeySet::All(), {"Column"}}).begin()->status(),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->PartitionRead({{txn, "table", KeySet::All(), {"Column"}}}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(
      conn->ExecuteQuery({txn, SqlStatement("select 1")}).begin()->status(),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->ExecuteDml({txn, SqlStatement("delete * from table")}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(
      conn->ProfileQuery({txn, SqlStatement("select 1")}).begin()->status(),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->ProfileDml({txn, SqlStatement("delete * from table")}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->AnalyzeSql({txn, SqlStatement("select * from table")}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  // ExecutePartitionedDml creates its own transaction so it's not tested here.

  EXPECT_THAT(conn->PartitionQuery({txn, SqlStatement("select * from table")}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->ExecuteBatchDml({txn}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->Commit({txn}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->Rollback({txn}),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
