// Copyright 2018 Google LLC.
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

#include "google/cloud/bigtable/internal/async_check_consistency.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/internal/table_admin.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
namespace {

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::admin::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;
using MockAsyncCheckConsistencyReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        btproto::CheckConsistencyResponse>;
using MockAsyncGenerateConsistencyToken =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        btproto::GenerateConsistencyTokenResponse>;

class NoexAsyncCheckConsistencyTest : public ::testing::Test {};

/// @test Verify that TableAdmin::CheckConsistency() works in a simplest case.
TEST_F(NoexAsyncCheckConsistencyTest, Simple) {
  auto client = std::make_shared<testing::MockAdminClient>();
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto check_consistency_reader =
      google::cloud::internal::make_unique<MockAsyncCheckConsistencyReader>();
  EXPECT_CALL(*check_consistency_reader, Finish(_, _, _))
      .WillOnce(Invoke([](btproto::CheckConsistencyResponse* response,
                          grpc::Status* status, void*) {
        response->set_consistent(true);
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client, AsyncCheckConsistency(_, _, _))
      .WillOnce(Invoke([&check_consistency_reader](
                           grpc::ClientContext*,
                           btproto::CheckConsistencyRequest const& request,
                           grpc::CompletionQueue*) {
        EXPECT_EQ("qwerty", request.consistency_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckConsistencyResponse>>(check_consistency_reader.get());
      }));

  bool user_op_called = false;
  auto user_callback = [&user_op_called](CompletionQueue& cq, bool response,
                                         grpc::Status const& status) {
    EXPECT_TRUE(response);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("mocked-status", status.error_message());
    user_op_called = true;
  };
  using OpType = internal::AsyncPollCheckConsistency<decltype(user_callback)>;
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(
      "instance_id", MetadataParamTypes::NAME, "table_id");
  auto op = std::make_shared<OpType>(
      __func__, polling_policy->clone(), metadata_update_policy, client,
      ConsistencyToken("qwerty"), "table_name", std::move(user_callback));
  op->Start(cq);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());
  cq_impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

class NoexAsyncCheckConsistencyRetryTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<bool> {};
// Verify that one retry works. No reason for many tests - AsyncPollOp is
// thoroughly tested.
TEST_P(NoexAsyncCheckConsistencyRetryTest, OneRetry) {
  bool const fail_first_rpc = GetParam();
  auto client = std::make_shared<testing::MockAdminClient>();
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto check_consistency_reader =
      google::cloud::internal::make_unique<MockAsyncCheckConsistencyReader>();
  EXPECT_CALL(*check_consistency_reader, Finish(_, _, _))
      .WillOnce(Invoke([fail_first_rpc](
                           btproto::CheckConsistencyResponse* response,
                           grpc::Status* status, void*) {
        response->set_consistent(false);
        *status = grpc::Status(fail_first_rpc ? grpc::StatusCode::UNAVAILABLE
                                              : grpc::StatusCode::OK,
                               "mocked-status");
      }))
      .WillOnce(Invoke([](btproto::CheckConsistencyResponse* response,
                          grpc::Status* status, void*) {
        response->set_consistent(true);
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client, AsyncCheckConsistency(_, _, _))
      .WillOnce(Invoke([&check_consistency_reader](
                           grpc::ClientContext*,
                           btproto::CheckConsistencyRequest const& request,
                           grpc::CompletionQueue*) {
        EXPECT_EQ("qwerty", request.consistency_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckConsistencyResponse>>(check_consistency_reader.get());
      }))
      .WillOnce(Invoke([&check_consistency_reader](
                           grpc::ClientContext*,
                           btproto::CheckConsistencyRequest const& request,
                           grpc::CompletionQueue*) {
        EXPECT_EQ("qwerty", request.consistency_token());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckConsistencyResponse>>(check_consistency_reader.get());
      }));

  // Make the asynchronous request.
  bool user_op_called = false;
  auto user_callback = [&user_op_called](CompletionQueue& cq, bool response,
                                         grpc::Status const& status) {
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("mocked-status", status.error_message());
    EXPECT_TRUE(response);
    user_op_called = true;
  };
  using OpType = internal::AsyncPollCheckConsistency<decltype(user_callback)>;
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(
      "instance_id", MetadataParamTypes::NAME, "table_id");
  auto op = std::make_shared<OpType>(
      __func__, polling_policy->clone(), metadata_update_policy, client,
      ConsistencyToken("qwerty"), "table_name", std::move(user_callback));
  op->Start(cq);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  // request
  cq_impl->SimulateCompletion(cq, true);
  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  // timer

  cq_impl->SimulateCompletion(cq, true);
  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  //  request

  cq_impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

INSTANTIATE_TEST_CASE_P(OneRetry, NoexAsyncCheckConsistencyRetryTest,
                        ::testing::Values(false, true));

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
