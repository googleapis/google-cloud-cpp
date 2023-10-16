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

#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/numeric.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/spanner/testing/status_utils.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/non_constructible.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// We compile with `-Wextra` which enables `-Wmissing-field-initializers`.
// This warning triggers when aggregate initialization is used with too few
// arguments. For example
//
//   struct A { int a; int b; int c; };  // 3 fields
//   A a = {1, 2};  // <-- Warning, missing initializer for A::c.
//
// To make the test code in this file more readable, we disable this warning
// and rely on the guaranteed behavior of aggregate initialization.
// https://en.cppreference.com/w/cpp/language/aggregate_initialization
// Note: "pragma GCC" works for both GCC and clang. MSVC doesn't warn.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

using ::google::cloud::spanner_testing::HasSessionAndTransaction;
using ::google::cloud::spanner_testing::SessionNotFoundError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedPointwise;
using ::testing::Unused;

// Streaming RPC response types.
using BatchWriteResponse = ::google::spanner::v1::BatchWriteResponse;
using PartialResultSet = ::google::spanner::v1::PartialResultSet;

// A simple protobuf builder.
template <typename T>
class ProtoBuilder {
 public:
  ProtoBuilder() = default;
  explicit ProtoBuilder(T t) : t_(std::move(t)) {}

  template <typename ArgT0, typename... ArgT>
  ProtoBuilder&& Apply(void (T::*f)(ArgT0&&, ArgT&&...), ArgT0&& v,
                       ArgT&&... args) && {
    (t_.*f)(std::forward<ArgT0>(v), std::forward<ArgT>(args)...);
    return std::move(*this);
  }
  template <typename ArgT>
  ProtoBuilder&& Apply(void (T::*f)(ArgT), ArgT v) && {
    (t_.*f)(std::move(v));
    return std::move(*this);
  }
  ProtoBuilder&& Apply(void (T::*f)()) && {
    (t_.*f)();
    return std::move(*this);
  }

  T Build() && { return std::move(t_); }

 private:
  T t_;
};

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

MATCHER_P(HasReturnStats, return_commit_stats,
          "commit request has expected return-stats value") {
  return arg.return_commit_stats() == return_commit_stats;
}

MATCHER(HasBeginTransaction, "request has begin TransactionSelector set") {
  return arg.transaction().has_begin();
}

MATCHER_P(HasDatabase, database, "request has expected database") {
  return arg.database() == database.FullName();
}

MATCHER_P(HasCreatorRole, role, "request has expected creator role") {
  return arg.session_template().creator_role() == role;
}

// Matches a `spanner::Transaction` that is bound to a "bad" session.
MATCHER(HasBadSession, "bound to a session that's marked bad") {
  return Visit(arg, [&](SessionHolder& session,
                        StatusOr<google::spanner::v1::TransactionSelector>&,
                        TransactionContext const&) {
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

MATCHER_P(HasPriority, priority, "request has expected priority") {
  return arg.request_options().priority() == priority;
}

MATCHER_P(HasRequestTag, tag, "request has expected request tag") {
  return arg.request_options().request_tag() == tag;
}

MATCHER_P(HasTransactionTag, tag, "request has expected transaction tag") {
  return arg.request_options().transaction_tag() == tag;
}

// Ideally this would be a matcher, but matcher args are `const` and `RowStream`
// only has non-const methods.
bool ContainsNoRows(spanner::RowStream& rows) {
  return rows.begin() == rows.end();
}

// Helper to set the Transaction's ID. Requires selector.ok().
void SetTransactionId(spanner::Transaction& txn, std::string tid) {
  Visit(txn,
        [&tid](SessionHolder&,
               StatusOr<google::spanner::v1::TransactionSelector>& selector,
               TransactionContext const&) {
          selector->set_id(std::move(tid));
          return 0;
        });
}

// Helper to mark the Transaction as invalid.
void SetTransactionInvalid(spanner::Transaction& txn, Status status) {
  Visit(txn,
        [&status](SessionHolder&,
                  StatusOr<google::spanner::v1::TransactionSelector>& selector,
                  TransactionContext const&) {
          selector = std::move(status);
          return 0;
        });
}

// Helper to create a Transaction proto with a specified (or default) id.
google::spanner::v1::Transaction MakeTestTransaction(
    std::string id = "1234567890") {
  google::spanner::v1::Transaction txn;
  txn.set_id(std::move(id));
  return txn;
}

// Create a `BatchCreateSessionsResponse` with the given `sessions`.
google::spanner::v1::BatchCreateSessionsResponse MakeSessionsResponse(
    std::vector<std::string> sessions) {
  google::spanner::v1::BatchCreateSessionsResponse response;
  for (auto& session : sessions) {
    response.add_session()->set_name(std::move(session));
  }
  return response;
}

// Create a `CommitResponse` with the given `commit_timestamp` and
// `commit_stats`.
google::spanner::v1::CommitResponse MakeCommitResponse(
    spanner::Timestamp commit_timestamp,
    absl::optional<spanner::CommitStats> commit_stats = absl::nullopt) {
  google::spanner::v1::CommitResponse response;
  *response.mutable_commit_timestamp() =
      commit_timestamp.get<protobuf::Timestamp>().value();
  if (commit_stats.has_value()) {
    auto* proto_stats = response.mutable_commit_stats();
    proto_stats->set_mutation_count(commit_stats->mutation_count);
  }
  return response;
}

Options MakeLimitedTimeOptions() {
  return Options{}
      .set<spanner::SpannerRetryPolicyOption>(
          std::make_shared<spanner::LimitedTimeRetryPolicy>(
              std::chrono::minutes(10)))
      .set<spanner::SpannerBackoffPolicyOption>(
          std::make_shared<spanner::ExponentialBackoffPolicy>(
              /*initial_delay=*/std::chrono::microseconds(1),
              /*maximum_delay=*/std::chrono::microseconds(1),
              /*scaling=*/2.0));
}

// Create `Options` suitable for use in tests that continue retrying until the
// retry policy is exhausted - attempting that with the default policies would
// take too long (10 minutes).
Options MakeLimitedRetryOptions() {
  return Options{}
      .set<spanner::SpannerRetryPolicyOption>(
          std::make_shared<spanner::LimitedErrorCountRetryPolicy>(
              /*maximum_failures=*/2))
      .set<spanner::SpannerBackoffPolicyOption>(
          std::make_shared<spanner::ExponentialBackoffPolicy>(
              /*initial_delay=*/std::chrono::microseconds(1),
              /*maximum_delay=*/std::chrono::microseconds(1),
              /*scaling=*/2.0));
}

std::shared_ptr<ConnectionImpl> MakeConnectionImpl(
    spanner::Database db,
    std::shared_ptr<spanner_testing::MockSpannerStub> mock, Options opts = {}) {
  // No actual credential needed for unit tests
  opts.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  opts = DefaultOptions(
      internal::MergeOptions(std::move(opts), MakeLimitedRetryOptions()));
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  std::vector<std::shared_ptr<SpannerStub>> stubs = {std::move(mock)};
  return std::make_shared<ConnectionImpl>(std::move(db), std::move(background),
                                          std::move(stubs), std::move(opts));
}

template <typename ResponseType>
class MockStreamingReadRpc : public internal::StreamingReadRpc<ResponseType> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::variant<Status, ResponseType>), Read, (), (override));
  MOCK_METHOD(internal::StreamingRpcMetadata, GetRequestMetadata, (),
              (const, override));
};

// Creates a MockStreamingReadRpc that yields the specified `responses`
// in sequence, and then yields the specified `status` (default OK).
template <typename ResponseType>
std::unique_ptr<MockStreamingReadRpc<ResponseType>> MakeReader(
    std::vector<ResponseType> responses, Status status = Status()) {
  auto reader = std::make_unique<MockStreamingReadRpc<ResponseType>>();
  Sequence s;
  for (auto& response : responses) {
    EXPECT_CALL(*reader, Read)
        .InSequence(s)
        .WillOnce(Return(std::move(response)));
  }
  EXPECT_CALL(*reader, Read).InSequence(s).WillOnce(Return(std::move(status)));
  return reader;
}

// Creates a MockStreamingReadRpc from an empty list.
template <typename ResponseType>
std::unique_ptr<MockStreamingReadRpc<ResponseType>> MakeReader(
    std::initializer_list<internal::NonConstructible>,
    Status status = Status()) {
  return MakeReader(std::vector<ResponseType>{}, std::move(status));
}

// Creates a MockStreamingReadRpc from protos in text format.
template <typename ResponseType>
std::unique_ptr<MockStreamingReadRpc<ResponseType>> MakeReader(
    std::vector<std::string> const& responses, Status status = Status()) {
  std::vector<ResponseType> response_protos;
  response_protos.reserve(responses.size());
  for (auto const& response : responses) {
    response_protos.push_back(ResponseType{});
    if (!TextFormat::ParseFromString(response, &response_protos.back())) {
      ADD_FAILURE() << "Failed to parse proto " << response;
    }
  }
  return MakeReader(std::move(response_protos), std::move(status));
}

TEST(ConnectionImplTest, ReadCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->Read(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       "table",
       spanner::KeySet::All(),
       {"column1"}});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in BatchCreateSessions")));
  }
}

TEST(ConnectionImplTest, ReadStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto finish_status =
      internal::PermissionDeniedError("uh-oh in GrpcReader::Finish");
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(
          Return(ByMove(MakeReader<PartialResultSet>({}, finish_status))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->Read(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       "table",
       spanner::KeySet::All(),
       {"column1"}});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GrpcReader::Finish")));
  }
}

TEST(ConnectionImplTest, ReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto retry_status = internal::UnavailableError("try-again");
  std::vector<std::string> responses = {
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
        resume_token: "restart-row-2"
      )pb",
      R"pb(
        values: { string_value: "42" }
        values: { string_value: "A" }
        chunked_value: true
      )pb",
      R"pb(
        values: { string_value: "nn" }
      )pb",
  };
  EXPECT_CALL(
      *mock,
      StreamingRead(
          _, HasPriority(google::spanner::v1::RequestOptions::PRIORITY_LOW)))
      .WillOnce(
          [&retry_status](std::shared_ptr<grpc::ClientContext> const&,
                          google::spanner::v1::ReadRequest const& request) {
            // The beginning of the row stream, but immediately fail.
            EXPECT_THAT(request.resume_token(), IsEmpty());
            return MakeReader<PartialResultSet>({}, retry_status);
          })
      .WillOnce([&responses, &retry_status](
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::spanner::v1::ReadRequest const& request) {
        // Restart from the beginning, but return the metadata and first
        // row before failing again.
        EXPECT_THAT(request.resume_token(), IsEmpty());
        return MakeReader<PartialResultSet>({responses[0]}, retry_status);
      })
      .WillOnce([&responses, &retry_status](
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::spanner::v1::ReadRequest const& request) {
        // Restart from the second row, but only return part of it before
        // failing once more.
        EXPECT_THAT(request.resume_token(), Eq("restart-row-2"));
        return MakeReader<PartialResultSet>({responses[1]}, retry_status);
      })
      .WillOnce([&responses](std::shared_ptr<grpc::ClientContext> const&,
                             google::spanner::v1::ReadRequest const& request) {
        // Restart from the second row, but now deliver it all in two chunks.
        EXPECT_THAT(request.resume_token(), Eq("restart-row-2"));
        return MakeReader<PartialResultSet>({responses[1], responses[2]});
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::ReadOptions read_options;
  read_options.request_priority = spanner::RequestPriority::kLow;
  auto rows = conn->Read(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       "table",
       spanner::KeySet::All(),
       {"UserId", "UserName"},
       read_options});

  using RowType = std::tuple<std::int64_t, std::string>;
  auto stream = spanner::StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType(12, "Steve")),
                                  IsOkAndHolds(RowType(42, "Ann"))));
}

TEST(ConnectionImplTest, ReadPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>(
          {}, internal::PermissionDeniedError("uh-oh")))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto rows = conn->Read(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       "table",
       spanner::KeySet::All(),
       {"UserId", "UserName"}});
  for (auto& row : rows) {
    EXPECT_THAT(row,
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  }
}

TEST(ConnectionImplTest, ReadTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead)
      .Times(AtLeast(2))
      // This won't compile without `Unused` despite what the gMock docs say.
      .WillRepeatedly([](Unused, Unused) {
        return MakeReader<PartialResultSet>(
            {}, internal::UnavailableError("try-again"));
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto rows = conn->Read(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       "table",
       spanner::KeySet::All(),
       {"UserId", "UserName"}});
  for (auto& row : rows) {
    EXPECT_THAT(row,
                StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
  }
}

/// @test Verify implicit "begin transaction" in Read() works.
TEST(ConnectionImplTest, ReadImplicitBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);
  auto constexpr kText = R"pb(
    metadata: { transaction: { id: "ABCDEF00" } }
  )pb";
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows = conn->Read(
      {txn, "table", spanner::KeySet::All(), {"UserId", "UserName"}});
  EXPECT_TRUE(ContainsNoRows(rows));
  EXPECT_THAT(txn, HasSessionAndTransaction("test-session-name", "ABCDEF00",
                                            false, ""));
}

TEST(ConnectionImplTest, ReadImplicitBeginTransactionOneTransientFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  auto status = internal::UnavailableError("uh-oh");
  auto failing_reader = MakeReader<PartialResultSet>({}, status);
  auto constexpr kText = R"pb(
    metadata: {
      transaction: { id: "ABCDEF00" }
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
  auto ok_reader = MakeReader<PartialResultSet>({kText});

  // n.b. these calls are explicitly sequenced because using the scoped
  // `InSequence` object causes gMock to get confused by the reader calls.
  Sequence s;
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .InSequence(s)
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasBeginTransaction())))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(failing_reader))));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasBeginTransaction())))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(ok_reader))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows = conn->Read(
      {txn, "table", spanner::KeySet::All(), {"UserId", "UserName"}});

  using RowType = std::tuple<std::int64_t, std::string>;
  auto stream = spanner::StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType(12, "Steve")),
                                  IsOkAndHolds(RowType(42, "Ann"))));
  EXPECT_THAT(txn, HasSessionAndTransaction("test-session-name", "ABCDEF00",
                                            false, ""));
}

TEST(ConnectionImplTest, ReadImplicitBeginTransactionOnePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  auto status = internal::PermissionDeniedError("uh-oh");
  auto failing_reader = MakeReader<PartialResultSet>({}, status);
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
  auto ok_reader = MakeReader<PartialResultSet>({kText});

  // n.b. these calls are explicitly sequenced because using the scoped
  // `InSequence` object causes gMock to get confused by the reader calls.
  Sequence s;
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .InSequence(s)
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasBeginTransaction())))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(failing_reader))));
  EXPECT_CALL(*mock, BeginTransaction)
      .InSequence(s)
      .WillOnce(Return(MakeTestTransaction("FEDCBA98")));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasTransactionId("FEDCBA98"))))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(ok_reader))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows = conn->Read(
      {txn, "table", spanner::KeySet::All(), {"UserId", "UserName"}});

  using RowType = std::tuple<std::int64_t, std::string>;
  auto stream = spanner::StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType(12, "Steve")),
                                  IsOkAndHolds(RowType(42, "Ann"))));
  EXPECT_THAT(txn, HasSessionAndTransaction("test-session-name", "FEDCBA98",
                                            false, ""));
}

TEST(ConnectionImplTest, ReadImplicitBeginTransactionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  auto status = internal::PermissionDeniedError("uh-oh");
  auto reader1 = MakeReader<PartialResultSet>({}, status);
  auto reader2 = MakeReader<PartialResultSet>({}, status);

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
  EXPECT_CALL(*mock, BeginTransaction)
      .InSequence(s)
      .WillOnce(Return(MakeTestTransaction("FEDCBA98")));
  EXPECT_CALL(*mock, StreamingRead(_, AllOf(HasSession("test-session-name"),
                                            HasTransactionId("FEDCBA98"))))
      .InSequence(s)
      .WillOnce(Return(ByMove(std::move(reader2))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows = conn->Read(
      {txn, "table", spanner::KeySet::All(), {"UserId", "UserName"}});
  for (auto& row : rows) {
    EXPECT_THAT(row,
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  }
}

TEST(ConnectionImplTest, ExecuteQueryCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in BatchCreateSessions")));
  }
}

TEST(ConnectionImplTest, ExecuteQueryStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>(
          {},
          internal::PermissionDeniedError("uh-oh in GrpcReader::Finish")))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});
  for (auto& row : rows) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GrpcReader::Finish")));
  }
}

TEST(ConnectionImplTest, ExecuteQueryReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
        fields: {
          name: "UserSalary",
          type: { code: NUMERIC }
        }
      }
    }
    values: { string_value: "12" }
    values: { string_value: "Steve" }
    values: { null_value: NULL_VALUE }
    values: { string_value: "42" }
    values: { string_value: "Ann" }
    values: { string_value: "123456.78" }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});

  using RowType =
      std::tuple<std::int64_t, std::string, absl::optional<spanner::Numeric>>;
  auto stream = spanner::StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(
      actual,
      ElementsAre(IsOkAndHolds(RowType(12, "Steve", absl::nullopt)),
                  IsOkAndHolds(RowType(
                      42, "Ann", spanner::MakeNumeric(12345678, -2).value()))));
}

TEST(ConnectionImplTest, ExecuteQueryPgNumericResult) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kText = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "ColumnA",
          type: { code: NUMERIC type_annotation: PG_NUMERIC }
        }
        fields: {
          name: "ColumnB",
          type: { code: NUMERIC type_annotation: PG_NUMERIC }
        }
      }
    }
    values: { string_value: "42" }
    values: { null_value: NULL_VALUE }
    values: { string_value: "NaN" }
    values: { string_value: "0.42" }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});

  using RowType =
      std::tuple<spanner::PgNumeric, absl::optional<spanner::PgNumeric>>;
  auto stream = spanner::StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(
      actual,
      ElementsAre(
          IsOkAndHolds(RowType(spanner::MakePgNumeric(42).value(),  //
                               absl::nullopt)),
          IsOkAndHolds(RowType(spanner::MakePgNumeric("NaN").value(),
                               spanner::MakePgNumeric(42, -2).value()))));
}

TEST(ConnectionImplTest, ExecuteQueryJsonBResult) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kText = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "ColumnA",
          type: { code: JSON type_annotation: PG_JSONB }
        }
        fields: {
          name: "ColumnB",
          type: { code: JSON type_annotation: PG_JSONB }
        }
      }
    }
    values: { string_value: "42" }
    values: { null_value: NULL_VALUE }
    values: { string_value: "[null, null]" }
    values: { string_value: "{\"a\": 1, \"b\": 2}" }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->ExecuteQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});

  using RowType = std::tuple<spanner::JsonB, absl::optional<spanner::JsonB>>;
  auto stream = spanner::StreamOf<RowType>(rows);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(
      actual,
      ElementsAre(
          IsOkAndHolds(RowType(spanner::JsonB("42"), absl::nullopt)),
          IsOkAndHolds(RowType(spanner::JsonB("[null, null]"),
                               spanner::JsonB(R"({"a": 1, "b": 2})")))));
}

TEST(ConnectionImplTest, ExecuteQueryNumericParameter) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto constexpr kResponseNumeric = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "Column",
          type: { code: NUMERIC }
        }
      }
    }
    values: { string_value: "1998" }
  )pb";
  auto constexpr kResponsePgNumeric = R"pb(
    metadata: {
      row_type: {
        fields: {
          name: "Column",
          type: { code: NUMERIC type_annotation: PG_NUMERIC }
        }
      }
    }
    values: { string_value: "1999" }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce([&](std::shared_ptr<grpc::ClientContext> const&,
                    google::spanner::v1::ExecuteSqlRequest const& request) {
        EXPECT_EQ(request.params().fields().at("value").string_value(), "998");
        EXPECT_EQ(request.param_types().at("value").code(),
                  google::spanner::v1::TypeCode::NUMERIC);
        EXPECT_EQ(request.param_types().at("value").type_annotation(),
                  google::spanner::v1::TypeAnnotationCode::
                      TYPE_ANNOTATION_CODE_UNSPECIFIED);
        return MakeReader<PartialResultSet>({kResponseNumeric});
      })
      .WillOnce([&](std::shared_ptr<grpc::ClientContext> const&,
                    google::spanner::v1::ExecuteSqlRequest const& request) {
        EXPECT_EQ(request.params().fields().at("value").string_value(), "999");
        EXPECT_EQ(request.param_types().at("value").code(),
                  google::spanner::v1::TypeCode::NUMERIC);
        EXPECT_EQ(request.param_types().at("value").type_annotation(),
                  google::spanner::v1::TypeAnnotationCode::PG_NUMERIC);
        return MakeReader<PartialResultSet>({kResponsePgNumeric});
      })
      .WillOnce([&](std::shared_ptr<grpc::ClientContext> const&,
                    google::spanner::v1::ExecuteSqlRequest const& request) {
        EXPECT_EQ(request.params().fields().at("value").string_value(), "NaN");
        EXPECT_EQ(request.param_types().at("value").code(),
                  google::spanner::v1::TypeCode::NUMERIC);
        EXPECT_EQ(request.param_types().at("value").type_annotation(),
                  google::spanner::v1::TypeAnnotationCode::PG_NUMERIC);
        return MakeReader<PartialResultSet>({kResponsePgNumeric});
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  for (auto const& value : {spanner::MakeNumeric(998)}) {
    auto rows = conn->ExecuteQuery(
        {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
         spanner::SqlStatement("SELECT Column FROM Table"
                               " WHERE Column > @value",
                               {{"value", spanner::Value(value.value())}})});
    using RowType = std::tuple<spanner::Numeric>;
    auto row = spanner::GetSingularRow(spanner::StreamOf<RowType>(rows));
    EXPECT_EQ(*row, RowType(spanner::MakeNumeric(1998).value()));
  }
  for (auto const& value :
       {spanner::MakePgNumeric(999), spanner::MakePgNumeric("NaN")}) {
    auto rows = conn->ExecuteQuery(
        {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
         spanner::SqlStatement("SELECT Column FROM Table"
                               " WHERE Column > @value",
                               {{"value", spanner::Value(value.value())}})});
    using RowType = std::tuple<spanner::PgNumeric>;
    auto row = spanner::GetSingularRow(spanner::StreamOf<RowType>(rows));
    EXPECT_EQ(*row, RowType(spanner::MakePgNumeric(1999).value()));
  }
}

/// @test Verify implicit "begin transaction" in ExecuteQuery() works.
TEST(ConnectionImplTest, ExecuteQueryImplicitBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);
  auto constexpr kText = R"pb(
    metadata: { transaction: { id: "00FEDCBA" } }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows =
      conn->ExecuteQuery({txn, spanner::SqlStatement("SELECT * FROM Table")});
  EXPECT_TRUE(ContainsNoRows(rows));
  EXPECT_THAT(txn, HasSessionAndTransaction("test-session-name", "00FEDCBA",
                                            false, ""));
}

/**
 * @test Verify that the protos sent by the 9 Connection::* methods affected
 * by QueryOptions contain the expected fields.
 */
TEST(ConnectionImplTest, QueryOptions) {
  struct {
    // Given these QueryOptions ...
    spanner::QueryOptions options;
    // ... expect these protos.
    google::spanner::v1::ExecuteSqlRequest::QueryOptions qo_proto;
    google::spanner::v1::RequestOptions ro_proto;
  } test_cases[] = {
      // Default options.
      {spanner::QueryOptions(),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>().Build()},

      // Optimizer version alone.
      {spanner::QueryOptions().set_optimizer_version(""),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Apply(&google::spanner::v1::ExecuteSqlRequest::QueryOptions::
                      set_optimizer_version,
                  "")
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>().Build()},
      {spanner::QueryOptions().set_optimizer_version("some-version"),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Apply(&google::spanner::v1::ExecuteSqlRequest::QueryOptions::
                      set_optimizer_version,
                  "some-version")
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>().Build()},

      // Optimizer stats package alone.
      {spanner::QueryOptions().set_optimizer_statistics_package(""),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Apply(&google::spanner::v1::ExecuteSqlRequest::QueryOptions::
                      set_optimizer_statistics_package,
                  "")
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>().Build()},
      {spanner::QueryOptions().set_optimizer_statistics_package("some-stats"),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Apply(&google::spanner::v1::ExecuteSqlRequest::QueryOptions::
                      set_optimizer_statistics_package,
                  "some-stats")
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>().Build()},

      // Request priority alone.
      {spanner::QueryOptions().set_request_priority(
           spanner::RequestPriority::kLow),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>()
           .Apply(&google::spanner::v1::RequestOptions::set_priority,
                  google::spanner::v1::RequestOptions::PRIORITY_LOW)
           .Build()},
      {spanner::QueryOptions().set_request_priority(
           spanner::RequestPriority::kHigh),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>()
           .Apply(&google::spanner::v1::RequestOptions::set_priority,
                  google::spanner::v1::RequestOptions::PRIORITY_HIGH)
           .Build()},

      // Request tag alone.
      {spanner::QueryOptions().set_request_tag("tag"),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>()
           .Apply(&google::spanner::v1::RequestOptions::set_request_tag, "tag")
           .Build()},

      // All options together.
      {spanner::QueryOptions()
           .set_optimizer_version("some-version")
           .set_optimizer_statistics_package("some-stats")
           .set_request_priority(spanner::RequestPriority::kLow)
           .set_request_tag("tag"),
       ProtoBuilder<google::spanner::v1::ExecuteSqlRequest::QueryOptions>()
           .Apply(&google::spanner::v1::ExecuteSqlRequest::QueryOptions::
                      set_optimizer_version,
                  "some-version")
           .Apply(&google::spanner::v1::ExecuteSqlRequest::QueryOptions::
                      set_optimizer_statistics_package,
                  "some-stats")
           .Build(),
       ProtoBuilder<google::spanner::v1::RequestOptions>()
           .Apply(&google::spanner::v1::RequestOptions::set_priority,
                  google::spanner::v1::RequestOptions::PRIORITY_LOW)
           .Apply(&google::spanner::v1::RequestOptions::set_request_tag, "tag")
           .Build()},
  };

  for (auto const& tc : test_cases) {
    auto db = spanner::Database("placeholder_project", "placeholder_instance",
                                "placeholder_database_id");
    auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
    auto txn = spanner::MakeReadWriteTransaction(
        spanner::Transaction::ReadWriteOptions().WithTag("tag"));

    auto sql_params = spanner::Connection::SqlParams{
        txn, spanner::SqlStatement{}, tc.options};
    auto execute_partitioned_dml_params =
        spanner::Connection::ExecutePartitionedDmlParams{
            spanner::SqlStatement{}, tc.options};
    auto execute_batch_dml_params =
        spanner::Connection::ExecuteBatchDmlParams{txn, {}, Options{}};
    if (tc.options.request_priority().has_value()) {
      execute_batch_dml_params.options.set<spanner::RequestPriorityOption>(
          *tc.options.request_priority());
    }
    if (tc.options.request_tag().has_value()) {
      execute_batch_dml_params.options.set<spanner::RequestTagOption>(
          *tc.options.request_tag());
    }
    spanner::ReadOptions read_options;
    read_options.request_priority = tc.options.request_priority();
    read_options.request_tag = tc.options.request_tag();
    auto read_params = spanner::Connection::ReadParams{
        txn, "table", spanner::KeySet::All(), {}, read_options, absl::nullopt};
    auto commit_params = spanner::Connection::CommitParams{
        txn, spanner::Mutations{},
        spanner::CommitOptions{}
            .set_request_priority(tc.options.request_priority())
            .set_transaction_tag("tag")};

    // We wrap MockStreamingReadRpc in NiceMock, because we don't really
    // care how it's called in this test (aside from needing to return a
    // transaction in the first call), and we want to minimize gMock's
    // "uninteresting mock function call" warnings.
    auto stream =
        std::make_unique<NiceMock<MockStreamingReadRpc<PartialResultSet>>>();
    auto constexpr kResponseText = R"pb(
      metadata: { transaction: { id: "2468ACE" } }
    )pb";
    PartialResultSet response;
    ASSERT_TRUE(TextFormat::ParseFromString(kResponseText, &response));
    EXPECT_CALL(*stream, Read).WillOnce(Return(response));
    {
      InSequence seq;

      auto execute_sql_request_matcher = AllOf(
          Property(&google::spanner::v1::ExecuteSqlRequest::query_options,
                   IsProtoEqual(tc.qo_proto)),
          Property(
              &google::spanner::v1::ExecuteSqlRequest::request_options,
              IsProtoEqual(
                  ProtoBuilder<google::spanner::v1::RequestOptions>(tc.ro_proto)
                      .Apply(&google::spanner::v1::RequestOptions::
                                 set_transaction_tag,
                             "tag")
                      .Build())));
      auto begin_transaction_request_matcher = Property(
          &google::spanner::v1::BeginTransactionRequest::request_options,
          IsProtoEqual(
              ProtoBuilder<google::spanner::v1::RequestOptions>(tc.ro_proto)
                  .Apply(&google::spanner::v1::RequestOptions::clear_priority)
                  .Build()));
      auto untagged_execute_sql_request_matcher = AllOf(
          Property(&google::spanner::v1::ExecuteSqlRequest::query_options,
                   IsProtoEqual(tc.qo_proto)),
          Property(&google::spanner::v1::ExecuteSqlRequest::request_options,
                   IsProtoEqual(tc.ro_proto)));
      auto execute_batch_dml_request_matcher = Property(
          &google::spanner::v1::ExecuteBatchDmlRequest::request_options,
          IsProtoEqual(
              ProtoBuilder<google::spanner::v1::RequestOptions>(tc.ro_proto)
                  .Apply(
                      &google::spanner::v1::RequestOptions::set_transaction_tag,
                      "tag")
                  .Build()));
      auto read_request_matcher = Property(
          &google::spanner::v1::ReadRequest::request_options,
          IsProtoEqual(
              ProtoBuilder<google::spanner::v1::RequestOptions>(tc.ro_proto)
                  .Apply(
                      &google::spanner::v1::RequestOptions::set_transaction_tag,
                      "tag")
                  .Build()));
      auto commit_request_matcher = Property(
          &google::spanner::v1::CommitRequest::request_options,
          IsProtoEqual(
              ProtoBuilder<google::spanner::v1::RequestOptions>(tc.ro_proto)
                  .Apply(
                      &google::spanner::v1::RequestOptions::clear_request_tag)
                  .Apply(
                      &google::spanner::v1::RequestOptions::set_transaction_tag,
                      "tag")
                  .Build()));

      // ExecuteQuery().
      EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
          .WillOnce(Return(MakeSessionsResponse({"session-name"})))
          .RetiresOnSaturation();
      EXPECT_CALL(*mock, ExecuteStreamingSql(_, execute_sql_request_matcher))
          .WillOnce(Return(ByMove(std::move(stream))))
          .RetiresOnSaturation();

      // ProfileQuery().
      EXPECT_CALL(*mock, ExecuteStreamingSql(_, execute_sql_request_matcher))
          .WillOnce(Return(
              ByMove(std::make_unique<
                     NiceMock<MockStreamingReadRpc<PartialResultSet>>>())))
          .RetiresOnSaturation();

      // ExecutePartitionedDml().
      EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
          .WillOnce(Return(MakeSessionsResponse({"session-name"})))
          .RetiresOnSaturation();
      EXPECT_CALL(*mock, BeginTransaction(_, begin_transaction_request_matcher))
          .WillOnce(Return(MakeTestTransaction("2468ACE")))
          .RetiresOnSaturation();
      EXPECT_CALL(*mock,
                  ExecuteStreamingSql(_, untagged_execute_sql_request_matcher))
          .WillOnce(Return(
              ByMove(std::make_unique<
                     NiceMock<MockStreamingReadRpc<PartialResultSet>>>())))
          .RetiresOnSaturation();

      // ExecuteDml(), ProfileDml(), AnalyzeSql().
      EXPECT_CALL(*mock, ExecuteSql(_, execute_sql_request_matcher))
          .Times(3)
          .RetiresOnSaturation();

      // ExecuteBatchDml().
      EXPECT_CALL(*mock, ExecuteBatchDml(_, execute_batch_dml_request_matcher))
          .Times(1)
          .RetiresOnSaturation();

      // Read().
      EXPECT_CALL(*mock, StreamingRead(_, read_request_matcher))
          .WillOnce(Return(
              ByMove(std::make_unique<
                     NiceMock<MockStreamingReadRpc<PartialResultSet>>>())))
          .RetiresOnSaturation();

      // Commit().
      EXPECT_CALL(*mock, Commit(_, commit_request_matcher))
          .Times(1)
          .RetiresOnSaturation();
    }

    auto conn = MakeConnectionImpl(db, mock);
    internal::OptionsSpan span(MakeLimitedTimeOptions());
    (void)conn->ExecuteQuery(sql_params);
    (void)conn->ProfileQuery(sql_params);
    (void)conn->ExecutePartitionedDml(execute_partitioned_dml_params);
    (void)conn->ExecuteDml(sql_params);
    (void)conn->ProfileDml(sql_params);
    (void)conn->AnalyzeSql(sql_params);
    (void)conn->ExecuteBatchDml(execute_batch_dml_params);
    (void)conn->Read(read_params);
    (void)conn->Commit(commit_params);
  }
}

TEST(ConnectionImplTest, ExecuteDmlCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ExecuteDml({txn, spanner::SqlStatement("DELETE * FROM Table")});

  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in BatchCreateSessions")));
}

TEST(ConnectionImplTest, ExecuteDmlDeleteSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  auto constexpr kText = R"pb(
    metadata: { transaction: { id: "1234567890" } }
    stats: { row_count_exact: 42 }
  )pb";
  google::spanner::v1::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(response));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ExecuteDml({txn, spanner::SqlStatement("DELETE * FROM Table")});

  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->RowsModified(), 42);
}

TEST(ConnectionImplTest, ExecuteDmlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ExecuteDml({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteDml")));
}

TEST(ConnectionImplTest, ExecuteDmlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kUnavailable, "try-again in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ExecuteDml({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteDml")));
}

// Tests that a Transaction that fails to begin does not successfully commit.
// That would violate atomicity since the first DML statement did not execute.
TEST(ConnectionImplTest, ExecuteDmlTransactionAtomicity) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(op_status));
    EXPECT_CALL(*mock, BeginTransaction).WillOnce(Return(begin_status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  // The first operation fails with the status of the operation's RPC
  // (`ExecuteSql` in this case).
  EXPECT_THAT(conn->ExecuteDml({txn, spanner::SqlStatement("SOME STATEMENT")}),
              StatusIs(op_status.code(), HasSubstr(op_status.message())));
  // Subsequent operations fail with the status of `BeginTransaction`.
  EXPECT_THAT(
      conn->ExecuteDml({txn, spanner::SqlStatement("ANOTHER STATEMENT")}),
      StatusIs(begin_status.code(), HasSubstr(begin_status.message())));
  EXPECT_THAT(conn->Commit({txn, {}}),
              StatusIs(begin_status.code(), HasSubstr(begin_status.message())));
}

TEST(ConnectionImplTest, ExecuteDmlTransactionMissing) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));

  // Return an otherwise valid response that does not contain a transaction.
  google::spanner::v1::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString("metadata: {}", &response));
  EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(response));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  EXPECT_THAT(
      conn->ExecuteDml({txn, spanner::SqlStatement("SELECT 1")}),
      StatusIs(StatusCode::kInternal,
               HasSubstr(
                   "Begin transaction requested but no transaction returned")));
}

TEST(ConnectionImplTest, ProfileQuerySuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ProfileQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});

  using RowType = std::tuple<std::int64_t, std::string>;
  auto stream = spanner::StreamOf<RowType>(result);
  auto actual = std::vector<StatusOr<RowType>>{stream.begin(), stream.end()};
  EXPECT_THAT(actual, ElementsAre(IsOkAndHolds(RowType(12, "Steve")),
                                  IsOkAndHolds(RowType(42, "Ann"))));

  auto constexpr kTextExpectedPlan = R"pb(
    plan_nodes: { index: 42 }
  )pb";
  google::spanner::v1::QueryPlan expected_plan;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextExpectedPlan, &expected_plan));

  auto plan = result.ExecutionPlan();
  ASSERT_TRUE(plan);
  EXPECT_THAT(*plan, IsProtoEqual(expected_plan));

  auto execution_stats = result.ExecutionStats();
  ASSERT_TRUE(execution_stats);
  EXPECT_THAT(*execution_stats,
              UnorderedElementsAre(Pair("elapsed_time", "42 secs")));
}

TEST(ConnectionImplTest, ProfileQueryCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ProfileQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});
  for (auto& row : result) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in BatchCreateSessions")));
  }
}

TEST(ConnectionImplTest, ProfileQueryStreamingReadFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto finish_status =
      internal::PermissionDeniedError("uh-oh in GrpcReader::Finish");
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(
          Return(ByMove(MakeReader<PartialResultSet>({}, finish_status))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ProfileQuery(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});
  for (auto& row : result) {
    EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("uh-oh in GrpcReader::Finish")));
  }
}

TEST(ConnectionImplTest, ProfileDmlCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ProfileDml({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in BatchCreateSessions")));
}

TEST(ConnectionImplTest, ProfileDmlDeleteSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
  google::spanner::v1::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(response));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ProfileDml({txn, spanner::SqlStatement("DELETE * FROM Table")});

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

  auto execution_stats = result->ExecutionStats();
  ASSERT_TRUE(execution_stats);
  EXPECT_THAT(*execution_stats,
              UnorderedElementsAre(Pair("elapsed_time", "42 secs")));
}

TEST(ConnectionImplTest, ProfileDmlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ProfileDml({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteDml")));
}

TEST(ConnectionImplTest, ProfileDmlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kUnavailable, "try-again in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->ProfileDml({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteDml")));
}

TEST(ConnectionImplTest, AnalyzeSqlSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();

  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  auto constexpr kText = R"pb(
    metadata: {}
    stats: { query_plan { plan_nodes: { index: 42 } } }
  )pb";
  google::spanner::v1::ResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(ByMove(std::move(response))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->AnalyzeSql(
      {MakeSingleUseTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table")});

  auto constexpr kTextExpectedPlan = R"pb(
    plan_nodes: { index: 42 }
  )pb";
  google::spanner::v1::QueryPlan expected_plan;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextExpectedPlan, &expected_plan));

  EXPECT_THAT(result, IsOkAndHolds(IsProtoEqual(expected_plan)));
}

TEST(ConnectionImplTest, AnalyzeSqlCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->AnalyzeSql({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in BatchCreateSessions")));
}

TEST(ConnectionImplTest, AnalyzeSqlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql).WillOnce(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->AnalyzeSql({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteDml")));
}

TEST(ConnectionImplTest, AnalyzeSqlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    Status status(StatusCode::kUnavailable, "try-again in ExecuteDml");
    EXPECT_CALL(*mock, ExecuteSql)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteSql)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  spanner::Transaction txn =
      MakeReadWriteTransaction(spanner::Transaction::ReadWriteOptions());
  auto result =
      conn->AnalyzeSql({txn, spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteDml")));
}

TEST(ConnectionImplTest, ExecuteBatchDmlSuccess) {
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
  google::spanner::v1::ExecuteBatchDmlResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(
      *mock,
      ExecuteBatchDml(
          _, HasPriority(google::spanner::v1::RequestOptions::PRIORITY_MEDIUM)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(response));

  auto request = {
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
  };

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner::MakeReadWriteTransaction(
      spanner::Transaction::ReadWriteOptions().WithTag("tag"));
  auto result =
      conn->ExecuteBatchDml({txn, request,
                             Options{}.set<spanner::RequestPriorityOption>(
                                 spanner::RequestPriority::kMedium)});
  ASSERT_STATUS_OK(result);
  EXPECT_STATUS_OK(result->status);
  EXPECT_EQ(result->stats.size(), request.size());
  ASSERT_EQ(result->stats.size(), 3);
  EXPECT_EQ(result->stats[0].row_count, 0);
  EXPECT_EQ(result->stats[1].row_count, 1);
  EXPECT_EQ(result->stats[2].row_count, 2);
  EXPECT_THAT(
      txn, HasSessionAndTransaction("session-name", "1234567890", true, "tag"));
}

TEST(ConnectionImplTest, ExecuteBatchDmlPartialFailure) {
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
  google::spanner::v1::ExecuteBatchDmlResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
  EXPECT_CALL(*mock, ExecuteBatchDml).WillOnce(Return(response));

  auto request = {
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
  };

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner::MakeReadWriteTransaction(
      spanner::Transaction::ReadWriteOptions().WithTag("tag"));
  auto result = conn->ExecuteBatchDml({txn, request});
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->status, StatusIs(StatusCode::kUnknown, "oops"));
  EXPECT_NE(result->stats.size(), request.size());
  ASSERT_EQ(result->stats.size(), 2);
  EXPECT_EQ(result->stats[0].row_count, 42);
  EXPECT_EQ(result->stats[1].row_count, 43);
  EXPECT_THAT(
      txn, HasSessionAndTransaction("session-name", "1234567890", true, "tag"));
}

TEST(ConnectionImplTest, ExecuteBatchDmlPermanentFailure) {
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));

    Status status(StatusCode::kPermissionDenied, "uh-oh in ExecuteBatchDml");
    EXPECT_CALL(*mock, ExecuteBatchDml).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteBatchDml).WillOnce(Return(status));
  }

  auto request = {
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
  };

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecuteBatchDml")));
}

TEST(ConnectionImplTest, ExecuteBatchDmlTooManyTransientFailures) {
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));

    Status status(StatusCode::kUnavailable, "try-again in ExecuteBatchDml");
    EXPECT_CALL(*mock, ExecuteBatchDml)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, ExecuteBatchDml)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  auto request = {
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
      spanner::SqlStatement("UPDATE ..."),
  };

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in ExecuteBatchDml")));
}

TEST(ConnectionImplTest, ExecuteBatchDmlNoResultSets) {
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions)
        .WillOnce(Return(MakeSessionsResponse({"session-name"})));
    // The `ExecuteBatchDml` call can succeed, but with no `ResultSet`s and an
    // error status in the response.
    auto constexpr kText = R"pb(
      status: { code: 6 message: "failed to insert ..." }
    )pb";
    google::spanner::v1::ExecuteBatchDmlResponse response;
    ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));
    EXPECT_CALL(*mock, ExecuteBatchDml(_, AllOf(HasSession("session-name"),
                                                HasBeginTransaction())))
        .WillOnce(Return(response));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction("BD000001")));
    EXPECT_CALL(*mock, ExecuteBatchDml(_, AllOf(HasSession("session-name"),
                                                HasTransactionId("BD000001"))))
        .WillOnce(Return(response));
  }

  auto request = {spanner::SqlStatement("UPDATE ...")};

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  auto result = conn->ExecuteBatchDml({txn, request});
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->status, StatusIs(StatusCode::kAlreadyExists,
                                       HasSubstr("failed to insert ...")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlDeleteSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeTestTransaction()));
  auto constexpr kTextResponse = R"pb(
    metadata: {}
    stats: { row_count_lower_bound: 42 }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql(
                         _, AllOf(HasRequestTag("tag"), HasTransactionTag(""))))
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>(
          {},
          internal::UnavailableError("try-again in ExecutePartitionedDml")))))
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kTextResponse}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table"),
       spanner::QueryOptions().set_request_tag("tag")});
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->row_count_lower_bound, 42);
}

TEST(ConnectionImplTest, ExecutePartitionedDmlCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in BatchCreateSessions")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlDeletePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeTestTransaction()));

  // A `kInternal` status can be treated as transient based on the message.
  // This tests that other `kInternal` errors are treated as permanent.
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>(
          {}, internal::InternalError("permanent failure")))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInternal, HasSubstr("permanent failure")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlDeleteTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeTestTransaction()));
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .Times(AtLeast(2))
      // This won't compile without `Unused` despite what the gMock docs say.
      .WillRepeatedly([](Unused, Unused) {
        return MakeReader<PartialResultSet>(
            {},
            internal::UnavailableError("try-again in ExecutePartitionedDml"));
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kUnavailable,
                       HasSubstr("try-again in ExecutePartitionedDml")));
}

TEST(ConnectionImplTest, ExecutePartitionedDmlRetryableInternalErrors) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(MakeTestTransaction("2345678901")));

  // `kInternal` is usually a permanent failure, but if the message indicates
  // the gRPC connection has been closed (which these do), they are treated as
  // transient failures.
  auto constexpr kTextResponse = R"pb(
    metadata: {}
    stats: { row_count_lower_bound: 99999 }
  )pb";
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>(
          {}, internal::InternalError(
                  "Received unexpected EOS on DATA frame from server")))))
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>(
          {}, internal::InternalError(
                  "HTTP/2 error code: INTERNAL_ERROR\nReceived Rst Stream")))))
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kTextResponse}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table")});
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->row_count_lower_bound, 99999);
}

TEST(ConnectionImplTest,
     ExecutePartitionedDmlDeleteBeginTransactionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied,
                              "uh-oh in ExecutePartitionedDml")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in ExecutePartitionedDml")));
}

TEST(ConnectionImplTest,
     ExecutePartitionedDmlDeleteBeginTransactionTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable,
                                    "try-again in ExecutePartitionedDml")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto result = conn->ExecutePartitionedDml(
      {spanner::SqlStatement("DELETE * FROM Table")});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kUnavailable,
                       HasSubstr("try-again in ExecutePartitionedDml")));
}

TEST(ConnectionImplTest, CommitCreateSessionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in BatchCreateSessions")));
}

TEST(ConnectionImplTest, CommitCreateSessionTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable,
                                    "try-again in BatchCreateSessions")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kUnavailable,
                               HasSubstr("try-again in BatchCreateSessions")));
}

TEST(ConnectionImplTest, CommitCreateSessionRetry) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(
          Status(StatusCode::kUnavailable, "try-again in BatchCreateSessions")))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  google::spanner::v1::Transaction txn = MakeTestTransaction();
  EXPECT_CALL(*mock, BeginTransaction).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Commit")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in Commit")));
}

TEST(ConnectionImplTest, CommitBeginTransactionRetry) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  google::spanner::v1::Transaction txn = MakeTestTransaction();
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(txn));
  auto const commit_timestamp =
      spanner::MakeTimestamp(std::chrono::system_clock::from_time_t(123))
          .value();
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(Return(MakeCommitResponse(commit_timestamp)));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction()});
  ASSERT_STATUS_OK(commit);
  EXPECT_EQ(commit_timestamp, commit->commit_timestamp);
}

TEST(ConnectionImplTest, CommitBeginTransactionSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  auto commit = conn->Commit({txn});
  EXPECT_THAT(commit, Not(IsOk()));
  auto const& status = commit.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, CommitBeginTransactionPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(
          Status(StatusCode::kInvalidArgument, "BeginTransaction failed")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
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
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  google::spanner::v1::Transaction txn = MakeTestTransaction();
  EXPECT_CALL(*mock, BeginTransaction).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Commit")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in Commit")));
}

TEST(ConnectionImplTest, CommitCommitTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  google::spanner::v1::Transaction txn = MakeTestTransaction();
  EXPECT_CALL(*mock, BeginTransaction).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId(txn.id()))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Commit")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction()});
  EXPECT_THAT(commit, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("uh-oh in Commit")));
}

TEST(ConnectionImplTest, CommitCommitInvalidatedTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);
  EXPECT_CALL(*mock, Commit).Times(0);

  // Committing an invalidated transaction is a unilateral error.
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionInvalid(txn,
                        Status(StatusCode::kAlreadyExists, "constraint error"));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto commit = conn->Commit({txn});
  EXPECT_THAT(commit, StatusIs(StatusCode::kAlreadyExists,
                               HasSubstr("constraint error")));
}

TEST(ConnectionImplTest, CommitCommitIdempotentTransientSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto const commit_timestamp =
      spanner::MakeTimestamp(std::chrono::system_clock::from_time_t(123))
          .value();
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasNakedTransactionId("test-txn-id"))))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(MakeCommitResponse(commit_timestamp)));

  // Set the id because that makes the commit idempotent.
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto commit = conn->Commit({txn});
  ASSERT_STATUS_OK(commit);
  EXPECT_EQ(commit_timestamp, commit->commit_timestamp);
}

TEST(ConnectionImplTest, CommitSuccessWithTransactionId) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(
      *mock,
      Commit(_, AllOf(HasSession("test-session-name"),
                      HasNakedTransactionId("test-txn-id"),
                      HasPriority(
                          google::spanner::v1::RequestOptions::PRIORITY_HIGH))))
      .WillOnce(Return(MakeCommitResponse(
          spanner::MakeTimestamp(std::chrono::system_clock::from_time_t(123))
              .value())));

  // Set the id because that makes the commit idempotent.
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto commit = conn->Commit({txn,
                              {},
                              spanner::CommitOptions{}.set_request_priority(
                                  spanner::RequestPriority::kHigh)});
  EXPECT_STATUS_OK(commit);
}

TEST(ConnectionImplTest, CommitSuccessWithStats) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  google::spanner::v1::Transaction txn = MakeTestTransaction();
  EXPECT_CALL(*mock, BeginTransaction).WillOnce(Return(txn));
  EXPECT_CALL(*mock, Commit(_, AllOf(HasSession("test-session-name"),
                                     HasReturnStats(true))))
      .WillOnce(Return(MakeCommitResponse(
          spanner::MakeTimestamp(std::chrono::system_clock::from_time_t(123))
              .value(),
          spanner::CommitStats{42})));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto commit = conn->Commit({spanner::MakeReadWriteTransaction(),
                              {},
                              spanner::CommitOptions{}.set_return_stats(true)});
  ASSERT_STATUS_OK(commit);
  ASSERT_TRUE(commit->commit_stats.has_value());
  EXPECT_EQ(42, commit->commit_stats->mutation_count);
}

TEST(ConnectionImplTest, CommitAtLeastOnce) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);  // The whole point!
  auto const commit_timestamp =
      spanner::MakeTimestamp(std::chrono::system_clock::from_time_t(123))
          .value();
  EXPECT_CALL(*mock, Commit)
      .WillOnce([commit_timestamp](
                    grpc::ClientContext&,
                    google::spanner::v1::CommitRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        EXPECT_TRUE(request.has_single_use_transaction());
        EXPECT_EQ(0, request.mutations_size());
        EXPECT_FALSE(request.return_commit_stats());
        EXPECT_EQ(google::spanner::v1::RequestOptions::PRIORITY_UNSPECIFIED,
                  request.request_options().priority());
        EXPECT_THAT(request.request_options().request_tag(), IsEmpty());
        EXPECT_THAT(request.request_options().transaction_tag(), IsEmpty());
        return MakeCommitResponse(commit_timestamp);
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto commit =
      conn->Commit({spanner_internal::MakeSingleUseCommitTransaction({}),
                    spanner::Mutations{}, spanner::CommitOptions{}});
  ASSERT_STATUS_OK(commit);
  EXPECT_EQ(commit_timestamp, commit->commit_timestamp);
}

TEST(ConnectionImplTest, RollbackCreateSessionFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kPermissionDenied,
                                    "uh-oh in BatchCreateSessions")));
  EXPECT_CALL(*mock, Rollback).Times(0);

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kPermissionDenied,
                                 HasSubstr("uh-oh in BatchCreateSessions")));
}

TEST(ConnectionImplTest, RollbackBeginTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  std::string const transaction_id = "RollbackBeginTransaction";
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(MakeTestTransaction(transaction_id)));
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .WillOnce(Return(Status()));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  auto rollback = conn->Rollback({txn});
  EXPECT_STATUS_OK(rollback);
}

TEST(ConnectionImplTest, RollbackSingleUseTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Rollback).Times(0);

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner_internal::MakeSingleUseTransaction(
      spanner::Transaction::SingleUseOptions{
          spanner::Transaction::ReadOnlyOptions{}});
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("Cannot rollback")));
}

TEST(ConnectionImplTest, RollbackPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  std::string const transaction_id = "test-txn-id";
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "uh-oh in Rollback")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, transaction_id);
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kPermissionDenied,
                                 HasSubstr("uh-oh in Rollback")));
}

TEST(ConnectionImplTest, RollbackTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  std::string const transaction_id = "test-txn-id";
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kUnavailable, "try-again in Rollback")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, transaction_id);
  auto rollback = conn->Rollback({txn});
  EXPECT_THAT(rollback, StatusIs(StatusCode::kUnavailable,
                                 HasSubstr("try-again in Rollback")));
}

TEST(ConnectionImplTest, RollbackSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const session_name = "test-session-name";
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({session_name})));
  std::string const transaction_id = "test-txn-id";
  EXPECT_CALL(*mock, Rollback(_, AllOf(HasSession(session_name),
                                       HasNakedTransactionId(transaction_id))))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(Status()));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, transaction_id);
  auto rollback = conn->Rollback({txn});
  EXPECT_STATUS_OK(rollback);
}

TEST(ConnectionImplTest, RollbackInvalidatedTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Rollback).Times(0);

  // Rolling back an invalidated transaction is a unilateral success.
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionInvalid(txn,
                        Status(StatusCode::kAlreadyExists, "constraint error"));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rollback_status = conn->Rollback({txn});
  EXPECT_THAT(rollback_status, StatusIs(StatusCode::kAlreadyExists,
                                        HasSubstr("constraint error")));
}

TEST(ConnectionImplTest, ReadPartition) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce([](std::shared_ptr<grpc::ClientContext> const&,
                   google::spanner::v1::ReadRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        EXPECT_EQ("Table", request.table());
        EXPECT_EQ("DEADBEEF", request.partition_token());
        EXPECT_TRUE(request.data_boost_enabled());
        return MakeReader<PartialResultSet>(
            {R"pb(metadata: { transaction: { id: " ABCDEF00 " } })pb"});
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->Read({spanner::MakeReadOnlyTransaction(),
                          "Table",
                          spanner::KeySet::All(),
                          {"Column"},
                          {},
                          "DEADBEEF",
                          true});
  EXPECT_TRUE(ContainsNoRows(rows));
}

TEST(ConnectionImplTest, PartitionReadSuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
    index: "index"
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

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  spanner::ReadOptions read_options;
  read_options.index_name = "index";
  read_options.limit = 21;
  read_options.request_priority = spanner::RequestPriority::kLow;
  bool data_boost = true;
  StatusOr<std::vector<spanner::ReadPartition>> result =
      conn->PartitionRead({{txn,
                            "table",
                            spanner::KeySet::All(),
                            {"UserId", "UserName"},
                            read_options},
                           {absl::nullopt, absl::nullopt, data_boost}});
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(txn, HasSessionAndTransaction("test-session-name", "CAFEDEAD",
                                            false, ""));

  std::vector<spanner::ReadPartition> expected_read_partitions = {
      spanner_internal::MakeReadPartition(
          "CAFEDEAD", false, "", "test-session-name", "BADDECAF", "table",
          spanner::KeySet::All(), {"UserId", "UserName"}, data_boost,
          read_options),
      spanner_internal::MakeReadPartition(
          "CAFEDEAD", false, "", "test-session-name", "DEADBEEF", "table",
          spanner::KeySet::All(), {"UserId", "UserName"}, data_boost,
          read_options)};

  EXPECT_THAT(*result, UnorderedPointwise(Eq(), expected_read_partitions));
}

TEST(ConnectionImplTest, PartitionReadPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    Status status(StatusCode::kPermissionDenied, "uh-oh");
    EXPECT_CALL(*mock, PartitionRead).WillOnce(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionRead).WillOnce(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  StatusOr<std::vector<spanner::ReadPartition>> result = conn->PartitionRead(
      {{MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions()),
        "table",
        spanner::KeySet::All(),
        {"UserId", "UserName"}}});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(ConnectionImplTest, PartitionReadTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    Status status(StatusCode::kUnavailable, "try-again");
    EXPECT_CALL(*mock, PartitionRead)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionRead)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  StatusOr<std::vector<spanner::ReadPartition>> result = conn->PartitionRead(
      {{MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions()),
        "table",
        spanner::KeySet::All(),
        {"UserId", "UserName"}}});
  EXPECT_THAT(result,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(ConnectionImplTest, QueryPartition) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");

  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce([](std::shared_ptr<grpc::ClientContext> const&,
                   google::spanner::v1::ExecuteSqlRequest const& request) {
        EXPECT_EQ("test-session-name", request.session());
        EXPECT_EQ("SELECT * FROM Table", request.sql());
        EXPECT_EQ("DEADBEEF", request.partition_token());
        EXPECT_TRUE(request.data_boost_enabled());
        return MakeReader<PartialResultSet>(
            {R"pb(metadata: { transaction: { id: " ABCDEF00 " } })pb"});
      });

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  auto rows = conn->ExecuteQuery({spanner::MakeReadOnlyTransaction(),
                                  spanner::SqlStatement("SELECT * FROM Table"),
                                  {},
                                  "DEADBEEF",
                                  true});
  EXPECT_TRUE(ContainsNoRows(rows));
}

TEST(ConnectionImplTest, PartitionQuerySuccess) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
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
    sql: "SELECT * FROM Table"
    params: {}
    partition_options: {}
  )pb";
  google::spanner::v1::PartitionQueryRequest partition_request;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kTextPartitionRequest, &partition_request));
  EXPECT_CALL(*mock, PartitionQuery(_, IsProtoEqual(partition_request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(partition_response));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::SqlStatement sql_statement("SELECT * FROM Table");
  bool data_boost = true;
  StatusOr<std::vector<spanner::QueryPartition>> result = conn->PartitionQuery(
      {MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions()),
       sql_statement,
       {absl::nullopt, absl::nullopt, data_boost}});
  ASSERT_STATUS_OK(result);

  std::vector<spanner::QueryPartition> expected_query_partitions = {
      spanner_internal::MakeQueryPartition("CAFEDEAD", false, "",
                                           "test-session-name", "BADDECAF",
                                           data_boost, sql_statement),
      spanner_internal::MakeQueryPartition("CAFEDEAD", false, "",
                                           "test-session-name", "DEADBEEF",
                                           data_boost, sql_statement)};

  EXPECT_THAT(*result, UnorderedPointwise(Eq(), expected_query_partitions));
}

TEST(ConnectionImplTest, PartitionQueryPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  Status failed_status = Status(StatusCode::kPermissionDenied, "End of line.");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    EXPECT_CALL(*mock, PartitionQuery).WillOnce(Return(failed_status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionQuery).WillOnce(Return(failed_status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  StatusOr<std::vector<spanner::QueryPartition>> result = conn->PartitionQuery(
      {MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table"),
       spanner::PartitionOptions()});
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr(failed_status.message())));
}

TEST(ConnectionImplTest, PartitionQueryTooManyTransientFailures) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  Status failed_status =
      Status(StatusCode::kUnavailable, "try-again in PartitionQuery");
  {
    InSequence seq;
    EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
        .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
    EXPECT_CALL(*mock, PartitionQuery)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(failed_status));
    EXPECT_CALL(*mock, BeginTransaction)
        .WillOnce(Return(MakeTestTransaction()));
    EXPECT_CALL(*mock, PartitionQuery)
        .Times(AtLeast(2))
        .WillRepeatedly(Return(failed_status));
  }

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  StatusOr<std::vector<spanner::QueryPartition>> result = conn->PartitionQuery(
      {MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions()),
       spanner::SqlStatement("SELECT * FROM Table"),
       spanner::PartitionOptions()});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable,
                               HasSubstr(failed_status.message())));
}

TEST(ConnectionImplTest, MultipleThreads) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  std::string const session_prefix = "test-session-prefix-";
  std::string const role = "TestRole";
  std::atomic<int> session_counter(0);
  EXPECT_CALL(*mock, BatchCreateSessions(
                         _, AllOf(HasDatabase(db), HasCreatorRole(role))))
      .WillRepeatedly(
          [&session_prefix, &session_counter](
              grpc::ClientContext&,
              google::spanner::v1::BatchCreateSessionsRequest const& request) {
            google::spanner::v1::BatchCreateSessionsResponse response;
            for (int i = 0; i < request.session_count(); ++i) {
              response.add_session()->set_name(
                  session_prefix + std::to_string(++session_counter));
            }
            return response;
          });
  EXPECT_CALL(*mock, Rollback)
      .WillRepeatedly([session_prefix](
                          grpc::ClientContext&,
                          google::spanner::v1::RollbackRequest const& request) {
        EXPECT_THAT(request.session(), StartsWith(session_prefix));
        return Status();
      });

  int const per_thread_iterations = 1000;
  auto const thread_count = []() -> unsigned {
    if (std::thread::hardware_concurrency() == 0) {
      return 16;
    }
    return std::thread::hardware_concurrency();
  }();

  auto runner = [](int thread_id, int iterations, spanner::Connection* conn) {
    for (int i = 0; i != iterations; ++i) {
      internal::OptionsSpan span(MakeLimitedTimeOptions());
      auto txn = spanner::MakeReadWriteTransaction();
      SetTransactionId(
          txn, "txn-" + std::to_string(thread_id) + ":" + std::to_string(i));
      auto rollback = conn->Rollback({txn});
      EXPECT_STATUS_OK(rollback);
    }
  };

  auto conn = MakeConnectionImpl(
      db, mock, Options{}.set<spanner::SessionCreatorRoleOption>(role));
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
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"session-1"})))
      .WillOnce(Return(MakeSessionsResponse({"session-2"})));

  constexpr int kNumResponses = 4;
  std::array<std::unique_ptr<MockStreamingReadRpc<PartialResultSet>>,
             kNumResponses>
      readers;
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
    PartialResultSet response;
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
    readers[i] = MakeReader<PartialResultSet>({std::move(response)});
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
  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());

  spanner::Transaction txn1 =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows = conn->Read({txn1, "table", spanner::KeySet::All(), {"Number"}});
  EXPECT_THAT(txn1,
              HasSessionAndTransaction("session-1", "ABCDEF01", false, ""));
  for (auto& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    ASSERT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 0);
  }

  spanner::Transaction txn2 =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  rows = conn->Read({txn2, "table", spanner::KeySet::All(), {"Number"}});
  EXPECT_THAT(txn2,
              HasSessionAndTransaction("session-2", "ABCDEF02", false, ""));
  for (auto& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    ASSERT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 1);
  }

  rows = conn->Read({txn1, "table", spanner::KeySet::All(), {"Number"}});
  EXPECT_THAT(txn1,
              HasSessionAndTransaction("session-1", "ABCDEF01", false, ""));
  for (auto& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    ASSERT_STATUS_OK(row);
    EXPECT_EQ(std::get<0>(*row), 2);
  }

  rows = conn->Read({txn2, "table", spanner::KeySet::All(), {"Number"}});
  EXPECT_THAT(txn2,
              HasSessionAndTransaction("session-2", "ABCDEF02", false, ""));
  for (auto& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    ASSERT_STATUS_OK(row);
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
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, BeginTransaction).Times(0);

  auto constexpr kText = R"pb(
    metadata: { transaction: { id: "ABCDEF00" } }
  )pb";
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(Return(ByMove(MakeReader<PartialResultSet>({kText}))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());
  spanner::Transaction txn =
      MakeReadOnlyTransaction(spanner::Transaction::ReadOnlyOptions());
  auto rows = conn->Read(
      {txn, "table", spanner::KeySet::All(), {"UserId", "UserName"}});
  EXPECT_TRUE(ContainsNoRows(rows));
  EXPECT_THAT(txn, HasSessionAndTransaction("test-session-name", "ABCDEF00",
                                            false, ""));

  // `conn` is the only reference to the `ConnectionImpl`, so dropping it will
  // cause the `ConnectionImpl` object to be deleted, while `txn` and its
  // associated `Session` continues to live on.
  conn.reset();
}

TEST(ConnectionImplTest, ReadSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto finish_status = SessionNotFoundError("test-session-name");
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(
          Return(ByMove(MakeReader<PartialResultSet>({}, finish_status))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto params = spanner::Connection::ReadParams{txn};
  auto response = GetSingularRow(conn->Read(std::move(params)));
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, PartitionReadSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, PartitionRead)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto params = spanner::Connection::ReadParams{txn};
  auto response = conn->PartitionRead({params});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecuteQuerySessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto finish_status = SessionNotFoundError("test-session-name");
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(
          Return(ByMove(MakeReader<PartialResultSet>({}, finish_status))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = GetSingularRow(conn->ExecuteQuery({txn}));
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ProfileQuerySessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  auto finish_status = SessionNotFoundError("test-session-name");
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce(
          Return(ByMove(MakeReader<PartialResultSet>({}, finish_status))));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = GetSingularRow(conn->ProfileQuery({txn}));
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecuteDmlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->ExecuteDml({txn});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ProfileDmlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->ProfileDml({txn});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, AnalyzeSqlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->AnalyzeSql({txn});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, PartitionQuerySessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, PartitionQuery)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->PartitionQuery({txn});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecuteBatchDmlSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, ExecuteBatchDml)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->ExecuteBatchDml({txn});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, ExecutePartitionedDmlSessionNotFound) {
  // NOTE: There's no test here because this method does not accept a
  // spanner::Transaction and so there's no way to extract the Session to check
  // if it's bad. We could modify the API to inject/extract this, but this is a
  // user-facing API that we don't want to mess up.
}

TEST(ConnectionImplTest, CommitSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Commit)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto response = conn->Commit({txn});
  EXPECT_THAT(response, Not(IsOk()));
  auto const& status = response.status();
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, RollbackSessionNotFound) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("project", "instance", "database");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));
  EXPECT_CALL(*mock, Rollback)
      .WillOnce(Return(SessionNotFoundError("test-session-name")));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedRetryOptions());
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionId(txn, "test-txn-id");
  auto const& status = conn->Rollback({txn});
  EXPECT_TRUE(IsSessionNotFound(status)) << status;
  EXPECT_THAT(txn, HasBadSession());
}

TEST(ConnectionImplTest, OperationsFailOnInvalidatedTransaction) {
  auto mock = std::make_shared<spanner_testing::MockSpannerStub>();
  auto db = spanner::Database("placeholder_project", "placeholder_instance",
                              "placeholder_database_id");
  EXPECT_CALL(*mock, BatchCreateSessions(_, HasDatabase(db)))
      .WillOnce(Return(MakeSessionsResponse({"test-session-name"})));

  auto conn = MakeConnectionImpl(db, mock);
  internal::OptionsSpan span(MakeLimitedTimeOptions());

  // Committing an invalidated transaction is a unilateral error.
  auto txn = spanner::MakeReadWriteTransaction();
  SetTransactionInvalid(
      txn, Status(StatusCode::kInvalidArgument, "BeginTransaction failed"));
  // All operations on an invalid transaction should return the error that
  // invalidated it, without actually making a RPC.

  EXPECT_THAT(conn->Read({txn, "table", spanner::KeySet::All(), {"Column"}})
                  .begin()
                  ->status(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(
      conn->PartitionRead({{txn, "table", spanner::KeySet::All(), {"Column"}}}),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->ExecuteQuery({txn, spanner::SqlStatement("SELECT 1")})
                  .begin()
                  ->status(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(
      conn->ExecuteDml({txn, spanner::SqlStatement("DELETE * FROM Table")}),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(conn->ProfileQuery({txn, spanner::SqlStatement("SELECT 1")})
                  .begin()
                  ->status(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(
      conn->ProfileDml({txn, spanner::SqlStatement("DELETE * FROM Table")}),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  EXPECT_THAT(
      conn->AnalyzeSql({txn, spanner::SqlStatement("SELECT * FROM Table")}),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("BeginTransaction failed")));

  // ExecutePartitionedDml creates its own transaction so it's not tested here.

  EXPECT_THAT(
      conn->PartitionQuery({txn, spanner::SqlStatement("SELECT * FROM Table"),
                            spanner::PartitionOptions()}),
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
