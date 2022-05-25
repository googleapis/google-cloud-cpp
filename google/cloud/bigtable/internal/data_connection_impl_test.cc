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
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
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

bigtable::SingleRowMutation IdempotentMutation() {
  return bigtable::SingleRowMutation(
      "row", {bigtable::SetCell("fam", "col", ms(0), "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation() {
  return bigtable::SingleRowMutation("row",
                                     {bigtable::SetCell("fam", "col", "val")});
}

class DataConnectionTest : public ::testing::Test {
 protected:
  std::shared_ptr<DataConnectionImpl> TestConnection() {
    auto options =
        Options{}
            .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
            .set<DataRetryPolicyOption>(retry_.clone())
            .set<DataBackoffPolicyOption>(backoff_.clone());
    auto background = internal::MakeBackgroundThreadsFactory(options)();
    return std::make_shared<DataConnectionImpl>(std::move(background),
                                                mock_stub_, std::move(options));
  }

  DataLimitedErrorCountRetryPolicy retry_ =
      DataLimitedErrorCountRetryPolicy(kNumRetries);
  ExponentialBackoffPolicy backoff_ =
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0);
  std::shared_ptr<MockBigtableStub> mock_stub_ =
      std::make_shared<MockBigtableStub>();
};

TEST_F(DataConnectionTest, Options) {
  auto connection = TestConnection();
  auto options = connection->options();
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
TEST_F(DataConnectionTest, CallOptionsOverrideConnectionOptions) {
  EXPECT_CALL(*mock_stub_, MutateRow).WillOnce(Return(PermanentError()));

  auto call_r = std::make_shared<MockDataRetryPolicy>();
  auto call_b = std::make_shared<MockBackoffPolicy>();
  auto call_i = std::make_shared<MockIdempotentMutationPolicy>();
  auto call_opts = Options{}
                       .set<DataRetryPolicyOption>(call_r)
                       .set<DataBackoffPolicyOption>(call_b)
                       .set<IdempotentMutationPolicyOption>(call_i);

  EXPECT_CALL(*call_r, clone).WillOnce(Return(ByMove(retry_.clone())));
  EXPECT_CALL(*call_b, clone).WillOnce(Return(ByMove(backoff_.clone())));
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

  auto connection = std::make_shared<DataConnectionImpl>(nullptr, mock_stub_,
                                                         std::move(conn_opts));
  internal::OptionsSpan span(call_opts);
  (void)connection->Apply(kAppProfile, kTableName, IdempotentMutation());
}

/**
 * Verify that connection options (provided to the DataConnection constructor)
 * take affect. We will only test this for Apply() and assume it holds true for
 * all RPCs.
 *
 * This is testing the private `retry_policy()`, `backoff_policy()`,
 * `idempotency_policy()` member functions.
 */
TEST_F(DataConnectionTest, UseConnectionOptionsIfNoCallOptions) {
  EXPECT_CALL(*mock_stub_, MutateRow).WillOnce(Return(PermanentError()));

  auto conn_r = std::make_shared<MockDataRetryPolicy>();
  auto conn_b = std::make_shared<MockBackoffPolicy>();
  auto conn_i = std::make_shared<MockIdempotentMutationPolicy>();
  auto conn_opts = Options{}
                       .set<DataRetryPolicyOption>(conn_r)
                       .set<DataBackoffPolicyOption>(conn_b)
                       .set<IdempotentMutationPolicyOption>(conn_i);

  EXPECT_CALL(*conn_r, clone).WillOnce(Return(ByMove(retry_.clone())));
  EXPECT_CALL(*conn_b, clone).WillOnce(Return(ByMove(backoff_.clone())));
  EXPECT_CALL(*conn_i, clone)
      .WillOnce(Return(ByMove(bigtable::DefaultIdempotentMutationPolicy())));

  auto connection = std::make_shared<DataConnectionImpl>(nullptr, mock_stub_,
                                                         std::move(conn_opts));
  (void)connection->Apply(kAppProfile, kTableName, IdempotentMutation());
}

TEST_F(DataConnectionTest, ApplySuccess) {
  EXPECT_CALL(*mock_stub_, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return v2::MutateRowResponse{};
      });

  auto connection = TestConnection();
  auto status =
      connection->Apply(kAppProfile, kTableName, IdempotentMutation());
  ASSERT_STATUS_OK(status);
}

TEST_F(DataConnectionTest, ApplyPermanentFailure) {
  EXPECT_CALL(*mock_stub_, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return PermanentError();
      });

  auto connection = TestConnection();
  auto status =
      connection->Apply(kAppProfile, kTableName, IdempotentMutation());
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, ApplyRetryThenSuccess) {
  EXPECT_CALL(*mock_stub_, MutateRow)
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

  auto connection = TestConnection();
  auto status =
      connection->Apply(kAppProfile, kTableName, IdempotentMutation());
  ASSERT_STATUS_OK(status);
}

TEST_F(DataConnectionTest, ApplyRetryExhausted) {
  EXPECT_CALL(*mock_stub_, MutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [](grpc::ClientContext&, v2::MutateRowRequest const& request) {
            EXPECT_EQ(kAppProfile, request.app_profile_id());
            EXPECT_EQ(kTableName, request.table_name());
            EXPECT_EQ("row", request.row_key());
            return TransientError();
          });

  auto connection = TestConnection();
  auto status =
      connection->Apply(kAppProfile, kTableName, IdempotentMutation());
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, ApplyRetryIdempotentOnly) {
  EXPECT_CALL(*mock_stub_, MutateRow)
      .WillOnce([](grpc::ClientContext&, v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  auto connection = TestConnection();
  auto status =
      connection->Apply(kAppProfile, kTableName, NonIdempotentMutation());
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
