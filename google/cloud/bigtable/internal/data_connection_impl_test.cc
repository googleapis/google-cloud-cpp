// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockDataRetryPolicy;
using ::google::cloud::bigtable::testing::MockIdempotentMutationPolicy;
using ::google::cloud::bigtable::testing::MockMutateRowsStream;
using ::google::cloud::bigtable::testing::MockReadRowsStream;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::An;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::Return;
using ms = std::chrono::milliseconds;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try again");
}

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "fail");
}

bigtable::SingleRowMutation IdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(
      row_key, {bigtable::SetCell("fam", "col", ms(0), "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(row_key,
                                     {bigtable::SetCell("fam", "col", "val")});
}

Matcher<v2::MutateRowsRequest::Entry> Entry(std::string const& row_key) {
  return Property(&v2::MutateRowsRequest::Entry::row_key, row_key);
}

void CheckFailedMutations(
    std::vector<bigtable::FailedMutation> const& actual,
    std::vector<bigtable::FailedMutation> const& expected) {
  struct Unroll {
    explicit Unroll(std::vector<bigtable::FailedMutation> const& failed) {
      for (auto const& f : failed) {
        statuses.push_back(f.status().code());
        indices.push_back(f.original_index());
      }
    }
    std::vector<StatusCode> statuses;
    std::vector<int> indices;
  };

  auto a = Unroll(actual);
  auto e = Unroll(expected);
  EXPECT_THAT(a.statuses, ElementsAreArray(e.statuses));
  EXPECT_THAT(a.indices, ElementsAreArray(e.indices));
}

// Individual entry pairs are: {index, StatusCode}
v2::MutateRowsResponse MakeBulkApplyResponse(
    std::vector<std::pair<int, grpc::StatusCode>> const& entries) {
  v2::MutateRowsResponse resp;
  for (auto entry : entries) {
    auto& e = *resp.add_entries();
    e.set_index(entry.first);
    e.mutable_status()->set_code(entry.second);
  }
  return resp;
}

bigtable::RowSet TestRowSet() { return bigtable::RowSet("r1", "r2"); }

Matcher<v2::ReadRowsRequest const&> HasTestRowSet() {
  return Property(&v2::ReadRowsRequest::rows,
                  Property(&v2::RowSet::row_keys, ElementsAre("r1", "r2")));
}

bigtable::Filter TestFilter() { return bigtable::Filter::Latest(5); }

Matcher<v2::RowFilter const&> IsTestFilter() {
  return Property(&v2::RowFilter::cells_per_column_limit_filter, Eq(5));
}

Matcher<v2::Mutation const&> MatchMutation(bigtable::Mutation const& m) {
  // For simplicity, we only use SetCell mutations in these tests.
  auto const& s = m.op.set_cell();
  return Property(
      &v2::Mutation::set_cell,
      AllOf(Property(&v2::Mutation::SetCell::family_name, s.family_name()),
            Property(&v2::Mutation::SetCell::column_qualifier,
                     s.column_qualifier()),
            Property(&v2::Mutation::SetCell::timestamp_micros,
                     s.timestamp_micros()),
            Property(&v2::Mutation::SetCell::value, s.value())));
}

Matcher<bigtable::Cell const&> MatchCell(bigtable::Cell const& c) {
  return AllOf(
      Property(&bigtable::Cell::row_key, c.row_key()),
      Property(&bigtable::Cell::family_name, c.family_name()),
      Property(&bigtable::Cell::column_qualifier, c.column_qualifier()),
      Property(&bigtable::Cell::timestamp, c.timestamp()),
      Property(&bigtable::Cell::value, c.value()),
      Property(&bigtable::Cell::labels, ElementsAreArray(c.labels())));
}

DataLimitedErrorCountRetryPolicy TestRetryPolicy() {
  return DataLimitedErrorCountRetryPolicy(kNumRetries);
}

ExponentialBackoffPolicy TestBackoffPolicy() {
  return ExponentialBackoffPolicy(ms(0), ms(0), 2.0);
}

std::shared_ptr<DataConnectionImpl> TestConnection(
    std::shared_ptr<BigtableStub> mock) {
  auto options =
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<DataRetryPolicyOption>(TestRetryPolicy().clone())
          .set<DataBackoffPolicyOption>(TestBackoffPolicy().clone());
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<DataConnectionImpl>(
      std::move(background), std::move(mock), std::move(options));
}

TEST(TransformReadModifyWriteRowResponse, Basic) {
  v2::ReadModifyWriteRowResponse response;
  auto constexpr kText = R"pb(
    row {
      key: "row"
      families {
        name: "cf1"
        columns {
          qualifier: "cq1"
          cells { value: "100" }
          cells { value: "200" }
        }
      }
      families {
        name: "cf2"
        columns {
          qualifier: "cq2"
          cells { value: "with-timestamp" timestamp_micros: 10 }
        }
        columns {
          qualifier: "cq3"
          cells { value: "with-labels" labels: "l1" labels: "l2" }
        }
      }
    })pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  auto row = TransformReadModifyWriteRowResponse(std::move(response));
  EXPECT_EQ("row", row.row_key());

  auto c1 = bigtable::Cell("row", "cf1", "cq1", 0, "100");
  auto c2 = bigtable::Cell("row", "cf1", "cq1", 0, "200");
  auto c3 = bigtable::Cell("row", "cf2", "cq2", 10, "with-timestamp");
  auto c4 = bigtable::Cell("row", "cf2", "cq3", 0, "with-labels", {"l1", "l2"});
  EXPECT_THAT(row.cells(), ElementsAre(MatchCell(c1), MatchCell(c2),
                                       MatchCell(c3), MatchCell(c4)));
}

TEST(DataConnectionTest, Options) {
  auto mock = std::make_shared<MockBigtableStub>();
  auto conn = TestConnection(std::move(mock));
  auto options = conn->options();
  EXPECT_TRUE(options.has<GrpcCredentialOption>());
  ASSERT_TRUE(options.has<EndpointOption>());
}

/**
 * Verify that call options (provided via an OptionsSpan) take precedence over
 * connection options (provided to the DataConnection constructor). We will only
 * test this for Apply() and assume it holds true for all RPCs.
 *
 * This is testing the private `retry_policy()`, `backoff_policy()`,
 * `idempotency_policy()` member functions.
 */
TEST(DataConnectionTest, CallOptionsOverrideConnectionOptions) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow).WillOnce(Return(PermanentError()));

  auto call_r = std::make_shared<MockDataRetryPolicy>();
  auto call_b = std::make_shared<MockBackoffPolicy>();
  auto call_i = std::make_shared<MockIdempotentMutationPolicy>();
  auto call_opts = Options{}
                       .set<DataRetryPolicyOption>(call_r)
                       .set<DataBackoffPolicyOption>(call_b)
                       .set<IdempotentMutationPolicyOption>(call_i);

  EXPECT_CALL(*call_r, clone)
      .WillOnce(Return(ByMove(TestRetryPolicy().clone())));
  EXPECT_CALL(*call_b, clone)
      .WillOnce(Return(ByMove(TestBackoffPolicy().clone())));
  EXPECT_CALL(*call_i, clone)
      .WillOnce(Return(ByMove(bigtable::DefaultIdempotentMutationPolicy())));

  auto conn_r = std::make_shared<MockDataRetryPolicy>();
  auto conn_b = std::make_shared<MockBackoffPolicy>();
  auto conn_i = std::make_shared<MockIdempotentMutationPolicy>();
  auto conn_opts = Options{}
                       .set<DataRetryPolicyOption>(conn_r)
                       .set<DataBackoffPolicyOption>(conn_b)
                       .set<IdempotentMutationPolicyOption>(conn_i);

  EXPECT_CALL(*conn_r, clone).Times(0);
  EXPECT_CALL(*conn_b, clone).Times(0);
  EXPECT_CALL(*conn_i, clone).Times(0);

  auto conn =
      std::make_shared<DataConnectionImpl>(nullptr, mock, std::move(conn_opts));
  internal::OptionsSpan span(call_opts);
  (void)conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
}

/**
 * Verify that connection options (provided to the DataConnection constructor)
 * take affect. We will only test this for Apply() and assume it holds true for
 * all RPCs.
 *
 * This is testing the private `retry_policy()`, `backoff_policy()`,
 * `idempotency_policy()` member functions.
 */
TEST(DataConnectionTest, UseConnectionOptionsIfNoCallOptions) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow).WillOnce(Return(PermanentError()));

  auto conn_r = std::make_shared<MockDataRetryPolicy>();
  auto conn_b = std::make_shared<MockBackoffPolicy>();
  auto conn_i = std::make_shared<MockIdempotentMutationPolicy>();
  auto conn_opts = Options{}
                       .set<DataRetryPolicyOption>(conn_r)
                       .set<DataBackoffPolicyOption>(conn_b)
                       .set<IdempotentMutationPolicyOption>(conn_i);

  EXPECT_CALL(*conn_r, clone)
      .WillOnce(Return(ByMove(TestRetryPolicy().clone())));
  EXPECT_CALL(*conn_b, clone)
      .WillOnce(Return(ByMove(TestBackoffPolicy().clone())));
  EXPECT_CALL(*conn_i, clone)
      .WillOnce(Return(ByMove(bigtable::DefaultIdempotentMutationPolicy())));

  auto conn =
      std::make_shared<DataConnectionImpl>(nullptr, mock, std::move(conn_opts));
  (void)conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
}

TEST(DataConnectionTest, ApplySuccess) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return v2::MutateRowResponse{};
      });

  auto conn = TestConnection(std::move(mock));
  auto status = conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
  ASSERT_STATUS_OK(status);
}

TEST(DataConnectionTest, ApplyPermanentFailure) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return PermanentError();
      });

  auto conn = TestConnection(std::move(mock));
  auto status = conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, ApplyRetryThenSuccess) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      })
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return v2::MutateRowResponse{};
      });

  auto conn = TestConnection(std::move(mock));
  auto status = conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
  ASSERT_STATUS_OK(status);
}

TEST(DataConnectionTest, ApplyRetryExhausted) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [](grpc::ClientContext&, v2::MutateRowRequest const& request) {
            EXPECT_EQ(kAppProfile, request.app_profile_id());
            EXPECT_EQ(kTableName, request.table_name());
            EXPECT_EQ("row", request.row_key());
            return TransientError();
          });

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto conn = TestConnection(std::move(mock));
  auto status = conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, ApplyRetryIdempotency) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  auto mock_i = absl::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone, is_idempotent(An<v2::Mutation const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto conn = TestConnection(std::move(mock));
  auto status =
      conn->Apply(kAppProfile, kTableName, NonIdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, AsyncApplySuccess) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future(make_status_or(v2::MutateRowResponse{}));
      });

  auto conn = TestConnection(std::move(mock));
  auto status =
      conn->AsyncApply(kAppProfile, kTableName, IdempotentMutation("row"));
  ASSERT_STATUS_OK(status.get());
}

TEST(DataConnectionTest, AsyncApplyPermanentFailure) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            PermanentError());
      });

  auto conn = TestConnection(std::move(mock));
  auto status =
      conn->AsyncApply(kAppProfile, kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, AsyncApplyRetryExhausted) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            TransientError());
      });

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto conn = TestConnection(std::move(mock));
  auto status =
      conn->AsyncApply(kAppProfile, kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, AsyncApplyRetryIdempotency) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            TransientError());
      });

  auto mock_i = absl::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone, is_idempotent(An<v2::Mutation const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto conn = TestConnection(std::move(mock));
  auto status =
      conn->AsyncApply(kAppProfile, kTableName, NonIdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, BulkApplyEmpty) {
  auto mock = std::make_shared<MockBigtableStub>();
  auto conn = TestConnection(std::move(mock));
  auto actual =
      conn->BulkApply(kAppProfile, kTableName, bigtable::BulkMutation());
  CheckFailedMutations(actual, {});
}

TEST(DataConnectionTest, BulkApplySuccess) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(Entry("r0"), Entry("r1")));
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeBulkApplyResponse(
                {{0, grpc::StatusCode::OK}, {1, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto actual = conn->BulkApply(kAppProfile, kTableName, std::move(mut));
  CheckFailedMutations(actual, {});
}

TEST(DataConnectionTest, BulkApplyRetryMutationPolicy) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 2},
                                                    {TransientError(), 3}};
  bigtable::BulkMutation mut(
      IdempotentMutation("success"),
      IdempotentMutation("retries-transient-error"),
      IdempotentMutation("fails-with-permanent-error"),
      NonIdempotentMutation("fails-with-transient-error"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(
                MakeBulkApplyResponse({{0, grpc::StatusCode::OK},
                                       {1, grpc::StatusCode::UNAVAILABLE},
                                       {2, grpc::StatusCode::PERMISSION_DENIED},
                                       {3, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(Entry("retries-transient-error")));
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto actual = conn->BulkApply(kAppProfile, kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST(DataConnectionTest, BulkApplyIncompleteStreamRetried) {
  bigtable::BulkMutation mut(IdempotentMutation("returned"),
                             IdempotentMutation("forgotten"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(Entry("forgotten")));
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto actual = conn->BulkApply(kAppProfile, kTableName, std::move(mut));
  CheckFailedMutations(actual, {});
}

TEST(DataConnectionTest, BulkApplyStreamRetryExhausted) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [](std::unique_ptr<grpc::ClientContext>,
             google::bigtable::v2::MutateRowsRequest const& request) {
            EXPECT_EQ(kAppProfile, request.app_profile_id());
            EXPECT_EQ(kTableName, request.table_name());
            auto stream = absl::make_unique<MockMutateRowsStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(TransientError()));
            return stream;
          });

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto conn = TestConnection(std::move(mock));
  auto actual = conn->BulkApply(kAppProfile, kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST(DataConnectionTest, BulkApplyStreamPermanentError) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto actual = conn->BulkApply(kAppProfile, kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST(DataConnectionTest, BulkApplyNoSleepIfNoPendingMutations) {
  bigtable::BulkMutation mut(IdempotentMutation("succeeds"),
                             IdempotentMutation("fails-immediately"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeBulkApplyResponse(
                {{0, grpc::StatusCode::OK},
                 {1, grpc::StatusCode::PERMISSION_DENIED}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).Times(0);

  internal::OptionsSpan span(
      Options{}.set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto conn = TestConnection(std::move(mock));
  (void)conn->BulkApply(kAppProfile, kTableName, std::move(mut));
}

// The DefaultRowReader is tested extensively in `default_row_reader_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST(DataConnectionTest, ReadRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(42, request.rows_limit());
        EXPECT_THAT(request, HasTestRowSet());
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto reader =
      conn->ReadRows(kAppProfile, kTableName, TestRowSet(), 42, TestFilter());
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST(DataConnectionTest, ReadRowEmpty) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto resp = conn->ReadRow(kAppProfile, kTableName, "row", TestFilter());
  ASSERT_STATUS_OK(resp);
  EXPECT_FALSE(resp->first);
}

TEST(DataConnectionTest, ReadRowSuccess) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce([]() {
              v2::ReadRowsResponse r;
              auto& chunk = *r.add_chunks();
              *chunk.mutable_row_key() = "row";
              chunk.mutable_family_name()->set_value("cf");
              chunk.mutable_qualifier()->set_value("cq");
              chunk.set_commit_row(true);
              return r;
            })
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto resp = conn->ReadRow(kAppProfile, kTableName, "row", TestFilter());
  ASSERT_STATUS_OK(resp);
  EXPECT_TRUE(resp->first);
  EXPECT_EQ("row", resp->second.row_key());
}

TEST(DataConnectionTest, ReadRowFailure) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto resp = conn->ReadRow(kAppProfile, kTableName, "row", TestFilter());
  EXPECT_THAT(resp, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, CheckAndMutateRowSuccess) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([&](grpc::ClientContext&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(true);
        return resp;
      })
      .WillOnce([&](grpc::ClientContext&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(false);
        return resp;
      });

  auto conn = TestConnection(std::move(mock));
  auto predicate = conn->CheckAndMutateRow(kAppProfile, kTableName, "row",
                                           TestFilter(), {t1, t2}, {f1, f2});
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateMatched);

  predicate = conn->CheckAndMutateRow(kAppProfile, kTableName, "row",
                                      TestFilter(), {t1, t2}, {f1, f2});
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateNotMatched);
}

TEST(DataConnectionTest, CheckAndMutateRowIdempotency) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([&](grpc::ClientContext&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return TransientError();
      });

  auto mock_i = absl::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone,
                is_idempotent(An<v2::CheckAndMutateRowRequest const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto conn = TestConnection(std::move(mock));
  auto status = conn->CheckAndMutateRow(kAppProfile, kTableName, "row",
                                        TestFilter(), {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, CheckAndMutateRowPermanentError) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([&](grpc::ClientContext&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return PermanentError();
      });

  auto conn = TestConnection(std::move(mock));
  auto status = conn->CheckAndMutateRow(kAppProfile, kTableName, "row",
                                        TestFilter(), {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, CheckAndMutateRowRetryExhausted) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([&](grpc::ClientContext&,
                          v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return TransientError();
      });

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<IdempotentMutationPolicyOption>(
              bigtable::AlwaysRetryMutationPolicy().clone()));
  auto conn = TestConnection(std::move(mock));
  auto status = conn->CheckAndMutateRow(kAppProfile, kTableName, "row",
                                        TestFilter(), {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, AsyncCheckAndMutateRowSuccess) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(true);
        return make_ready_future(make_status_or(resp));
      })
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(false);
        return make_ready_future(make_status_or(resp));
      });

  auto conn = TestConnection(std::move(mock));
  auto predicate =
      conn->AsyncCheckAndMutateRow(kAppProfile, kTableName, "row", TestFilter(),
                                   {t1, t2}, {f1, f2})
          .get();
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateMatched);

  predicate = conn->AsyncCheckAndMutateRow(kAppProfile, kTableName, "row",
                                           TestFilter(), {t1, t2}, {f1, f2})
                  .get();
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateNotMatched);
}

TEST(DataConnectionTest, AsyncCheckAndMutateRowIdempotency) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            TransientError());
      });

  auto mock_i = absl::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone,
                is_idempotent(An<v2::CheckAndMutateRowRequest const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto conn = TestConnection(std::move(mock));
  auto status = conn->AsyncCheckAndMutateRow(kAppProfile, kTableName, "row",
                                             TestFilter(), {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, AsyncCheckAndMutateRowPermanentError) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            PermanentError());
      });

  auto conn = TestConnection(std::move(mock));
  auto status = conn->AsyncCheckAndMutateRow(kAppProfile, kTableName, "row",
                                             TestFilter(), {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, AsyncCheckAndMutateRowRetryExhausted) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            TransientError());
      });

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<IdempotentMutationPolicyOption>(
              bigtable::AlwaysRetryMutationPolicy().clone()));
  auto conn = TestConnection(std::move(mock));
  auto status = conn->AsyncCheckAndMutateRow(kAppProfile, kTableName, "row",
                                             TestFilter(), {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

// The `AsyncRowSampler` is tested extensively in `async_row_sampler_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST(DataConnectionTest, AsyncSampleRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([](CompletionQueue const&, std::unique_ptr<grpc::ClientContext>,
                   v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        using ErrorStream =
            internal::AsyncStreamingReadRpcError<v2::SampleRowKeysResponse>;
        return absl::make_unique<ErrorStream>(PermanentError());
      });

  auto conn = TestConnection(std::move(mock));
  auto samples = conn->AsyncSampleRows(kAppProfile, kTableName).get();
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, ReadModifyWriteRowSuccess) {
  v2::ReadModifyWriteRowResponse response;
  auto constexpr kText = R"pb(
    row {
      key: "row"
      families {
        name: "cf"
        columns {
          qualifier: "cq"
          cells { value: "value" }
        }
      }
    })pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([&response](grpc::ClientContext&,
                            v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return response;
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto conn = TestConnection(std::move(mock));
  auto row = conn->ReadModifyWriteRow(req);
  ASSERT_STATUS_OK(row);
  EXPECT_EQ("row", row->row_key());

  auto c = bigtable::Cell("row", "cf", "cq", 0, "value");
  EXPECT_THAT(row->cells(), ElementsAre(MatchCell(c)));
}

TEST(DataConnectionTest, ReadModifyWriteRowPermanentError) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](grpc::ClientContext&,
                   v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return PermanentError();
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto conn = TestConnection(std::move(mock));
  auto row = conn->ReadModifyWriteRow(req);
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, ReadModifyWriteRowTransientErrorNotRetried) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](grpc::ClientContext&,
                   v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(0);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto conn = TestConnection(std::move(mock));
  auto row = conn->ReadModifyWriteRow(req);
  EXPECT_THAT(row, StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, AsyncReadModifyWriteRowSuccess) {
  v2::ReadModifyWriteRowResponse response;
  auto constexpr kText = R"pb(
    row {
      key: "row"
      families {
        name: "cf"
        columns {
          qualifier: "cq"
          cells { value: "value" }
        }
      }
    })pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([&response](google::cloud::CompletionQueue&,
                            std::unique_ptr<grpc::ClientContext>,
                            v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future(make_status_or(response));
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto conn = TestConnection(std::move(mock));
  auto row = conn->AsyncReadModifyWriteRow(req).get();
  ASSERT_STATUS_OK(row);
  EXPECT_EQ("row", row->row_key());

  auto c = bigtable::Cell("row", "cf", "cq", 0, "value");
  EXPECT_THAT(row->cells(), ElementsAre(MatchCell(c)));
}

TEST(DataConnectionTest, AsyncReadModifyWriteRowPermanentError) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::ReadModifyWriteRowResponse>>(
            PermanentError());
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto conn = TestConnection(std::move(mock));
  auto row = conn->AsyncReadModifyWriteRow(req).get();
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DataConnectionTest, AsyncReadModifyWriteRowTransientErrorNotRetried) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::ReadModifyWriteRowResponse>>(
            TransientError());
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(0);
    return clone;
  });

  internal::OptionsSpan span(
      Options{}.set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto conn = TestConnection(std::move(mock));
  auto row = conn->AsyncReadModifyWriteRow(req).get();
  EXPECT_THAT(row, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
