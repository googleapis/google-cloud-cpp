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
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
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
using ::google::cloud::bigtable::testing::MockReadRowsStream;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::ElementsAre;
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

bigtable::RowSet TestRowSet() { return bigtable::RowSet("r1", "r2"); }

Matcher<v2::ReadRowsRequest const&> HasTestRowSet() {
  return Property(&v2::ReadRowsRequest::rows,
                  Property(&v2::RowSet::row_keys, ElementsAre("r1", "r2")));
}

bigtable::Filter TestFilter() { return bigtable::Filter::Latest(5); }

Matcher<v2::ReadRowsRequest const&> HasTestFilter() {
  return Property(
      &v2::ReadRowsRequest::filter,
      Property(&v2::RowFilter::cells_per_column_limit_filter, Eq(5)));
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

  auto conn = TestConnection(std::move(mock));
  auto status = conn->Apply(kAppProfile, kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST(DataConnectionTest, ApplyRetryIdempotentOnly) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  auto conn = TestConnection(std::move(mock));
  auto status =
      conn->Apply(kAppProfile, kTableName, NonIdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

// The DefaultRowReader is tested extensively in `default_row_reader_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST(DataConnectionTest, ReadRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(kAppProfile, request.app_profile_id());
        EXPECT_THAT(kTableName, request.table_name());
        EXPECT_THAT(42, request.rows_limit());
        EXPECT_THAT(request, HasTestRowSet());
        EXPECT_THAT(request, HasTestFilter());

        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  auto reader =
      conn->ReadRows(kAppProfile, kTableName, TestRowSet(), 42, TestFilter());
  EXPECT_EQ(reader.begin(), reader.end());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
