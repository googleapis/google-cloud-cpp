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
                        ::testing::Values(
                            // First RPC returns an OK status but indicates that
                            // replication has not yet caught up.
                            false,
                            // First RPC fails.
                            true));

class EndToEndConfig {
 public:
  grpc::StatusCode generate_token_error_code;
  bool expect_check_consistency_call;
  grpc::StatusCode check_consistency_error_code;
  bool check_consistency_finished;
  grpc::StatusCode expected;
};

class NoexAsyncCheckConsistencyEndToEnd
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<EndToEndConfig> {};

TEST_P(NoexAsyncCheckConsistencyEndToEnd, EndToEnd) {
  using namespace ::testing;

  auto config = GetParam();

  std::string const kProjectId = "the-project";
  std::string const kInstanceId = "the-instance";
  std::string const kClusterId = "the-cluster";
  bigtable::TableId const kTableId("the-table");

  internal::RPCPolicyParameters const noRetries = {
      std::chrono::hours(0),
      std::chrono::hours(0),
      std::chrono::hours(0),
  };
  auto polling_policy = bigtable::DefaultPollingPolicy(noRetries);

  auto client = std::make_shared<testing::MockAdminClient>();
  EXPECT_CALL(*client, project()).WillRepeatedly(ReturnRef(kProjectId));
  bigtable::noex::TableAdmin tested(client, kInstanceId, *polling_policy);
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto generate_token_reader =
      google::cloud::internal::make_unique<MockAsyncGenerateConsistencyToken>();
  EXPECT_CALL(*generate_token_reader, Finish(_, _, _))
      .WillOnce(
          Invoke([config](btproto::GenerateConsistencyTokenResponse* response,
                          grpc::Status* status, void*) {
            response->set_consistency_token("qwerty");
            *status =
                grpc::Status(config.generate_token_error_code, "mocked-status");
          }));
  auto check_consistency_reader =
      google::cloud::internal::make_unique<MockAsyncCheckConsistencyReader>();
  if (config.expect_check_consistency_call) {
    EXPECT_CALL(*check_consistency_reader, Finish(_, _, _))
        .WillOnce(Invoke([config](btproto::CheckConsistencyResponse* response,
                                  grpc::Status* status, void*) {
          response->set_consistent(config.check_consistency_finished);
          *status = grpc::Status(config.check_consistency_error_code,
                                 "mocked-status");
        }));
  }

  EXPECT_CALL(*client, AsyncGenerateConsistencyToken(_, _, _))
      .WillOnce(
          Invoke([&generate_token_reader](
                     grpc::ClientContext*,
                     btproto::GenerateConsistencyTokenRequest const& request,
                     grpc::CompletionQueue*) {
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                btproto::GenerateConsistencyTokenResponse>>(
                generate_token_reader.get());
          }));
  if (config.expect_check_consistency_call) {
    EXPECT_CALL(*client, AsyncCheckConsistency(_, _, _))
        .WillOnce(Invoke([&check_consistency_reader](
                             grpc::ClientContext*,
                             btproto::CheckConsistencyRequest const& request,
                             grpc::CompletionQueue*) {
          EXPECT_EQ("qwerty", request.consistency_token());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
              btproto::CheckConsistencyResponse>>(
              check_consistency_reader.get());
        }));
  }

  // Make the asynchronous request.
  bool user_op_called = false;
  auto user_callback = [&user_op_called, &config](CompletionQueue& cq,
                                                  grpc::Status const& status) {
    user_op_called = true;
    EXPECT_EQ(config.expected, status.error_code());
  };
  tested.AsyncAwaitConsistency(kTableId, cq, user_callback);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  // AsyncGenerateConsistencyToken
  cq_impl->SimulateCompletion(cq, true);

  if (config.expect_check_consistency_call) {
    EXPECT_FALSE(user_op_called);
    EXPECT_EQ(1U, cq_impl->size());  // AsyncCheckConsistency
    cq_impl->SimulateCompletion(cq, true);
  }

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

INSTANTIATE_TEST_CASE_P(
    EndToEnd, NoexAsyncCheckConsistencyEndToEnd,
    ::testing::Values(
        // Everything succeeds immediately.
        EndToEndConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::OK,
            // Expect that AsyncAwaitConsistency calls CheckConsistency.
            true,
            // Error code which CheckConsistency returns.
            grpc::StatusCode::OK,
            // CheckConsisteny reports that it's finished.
            true,
            // Expected result.
            grpc::StatusCode::OK},
        //
        // Generating token fails, the error should be propagated.
        EndToEndConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::PERMISSION_DENIED,
            // Expect that AsyncAwaitConsistency won't call CheckConsistency.
            false,
            // Error code which CheckConsistency returns.
            grpc::StatusCode::UNKNOWN,
            // CheckConsisteny reports that it's finished.
            true,
            // Expected result.
            grpc::StatusCode::PERMISSION_DENIED},
        // CheckConsistency times out w.r.t PollingPolicy, UNKNOWN is returned.
        EndToEndConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::OK,
            // Expect that AsyncAwaitConsistency calls CheckConsistency.
            true,
            // Error code which CheckConsistency returns.
            grpc::StatusCode::OK,
            // CheckConsisteny reports that it's not yet finished.
            false,
            // Expected result.
            grpc::StatusCode::UNKNOWN},
        // CheckConsistency fails. UNKNOWN is returned.
        EndToEndConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::OK,
            // Expect that AsyncAwaitConsistency calls CheckConsistency.
            true,
            // Error code which CheckConsistency returns.
            grpc::StatusCode::UNAVAILABLE,
            // CheckConsisteny reports that it's not yet finished.
            false,
            // Expected result.
            grpc::StatusCode::UNAVAILABLE},
        // CheckConsistency succeeds but reports an error - it is passed on.
        EndToEndConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::OK,
            // Expect that AsyncAwaitConsistency calls CheckConsistency.
            true,
            // Error code which CheckConsistency returns.
            grpc::StatusCode::UNAVAILABLE,
            // CheckConsisteny reports that it's finished.
            true,
            // Expected result.
            grpc::StatusCode::UNAVAILABLE}));

class CancelConfig {
 public:
  // Error code returned from GenerateConsistencyToken
  grpc::StatusCode generate_token_error_code;
  // Whether to call cancel during GenerateConsistencyToken
  bool cancel_generate_token;
  // Whether to expect a call to CheckConsistency
  bool expect_check_consistency_call;
  // Error code returned from CheckConsistency
  grpc::StatusCode check_consistency_error_code;
  // Whether to call cancel during CheckConsistency
  bool cancel_check_consistency;
  grpc::StatusCode expected;
};

class NoexAsyncCheckConsistencyCancel
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<CancelConfig> {};

TEST_P(NoexAsyncCheckConsistencyCancel, Cancellations) {
  using namespace ::testing;

  auto config = GetParam();

  std::string const kProjectId = "the-project";
  std::string const kInstanceId = "the-instance";
  std::string const kClusterId = "the-cluster";
  bigtable::TableId const kTableId("the-table");

  internal::RPCPolicyParameters const noRetries = {
      std::chrono::hours(0),
      std::chrono::hours(0),
      std::chrono::hours(0),
  };
  auto polling_policy = bigtable::DefaultPollingPolicy(noRetries);

  auto client = std::make_shared<testing::MockAdminClient>();
  EXPECT_CALL(*client, project()).WillRepeatedly(ReturnRef(kProjectId));
  bigtable::noex::TableAdmin tested(client, kInstanceId, *polling_policy);
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto generate_token_reader =
      google::cloud::internal::make_unique<MockAsyncGenerateConsistencyToken>();
  EXPECT_CALL(*generate_token_reader, Finish(_, _, _))
      .WillOnce(
          Invoke([config](btproto::GenerateConsistencyTokenResponse* response,
                          grpc::Status* status, void*) {
            response->set_consistency_token("qwerty");
            *status =
                grpc::Status(config.generate_token_error_code, "mocked-status");
          }));
  auto check_consistency_reader =
      google::cloud::internal::make_unique<MockAsyncCheckConsistencyReader>();
  if (config.expect_check_consistency_call) {
    EXPECT_CALL(*check_consistency_reader, Finish(_, _, _))
        .WillOnce(Invoke([config](btproto::CheckConsistencyResponse* response,
                                  grpc::Status* status, void*) {
          response->set_consistent(true);
          *status = grpc::Status(config.check_consistency_error_code,
                                 "mocked-status");
        }));
  }

  EXPECT_CALL(*client, AsyncGenerateConsistencyToken(_, _, _))
      .WillOnce(
          Invoke([&generate_token_reader](
                     grpc::ClientContext*,
                     btproto::GenerateConsistencyTokenRequest const& request,
                     grpc::CompletionQueue*) {
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                btproto::GenerateConsistencyTokenResponse>>(
                generate_token_reader.get());
          }));
  if (config.expect_check_consistency_call) {
    EXPECT_CALL(*client, AsyncCheckConsistency(_, _, _))
        .WillOnce(Invoke([&check_consistency_reader](
                             grpc::ClientContext*,
                             btproto::CheckConsistencyRequest const& request,
                             grpc::CompletionQueue*) {
          EXPECT_EQ("qwerty", request.consistency_token());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
              btproto::CheckConsistencyResponse>>(
              check_consistency_reader.get());
        }));
  }

  // Make the asynchronous request.
  bool user_op_called = false;
  auto user_callback = [&user_op_called, &config](CompletionQueue& cq,
                                                  grpc::Status const& status) {
    user_op_called = true;
    EXPECT_EQ(config.expected, status.error_code());
  };
  auto op = tested.AsyncAwaitConsistency(kTableId, cq, user_callback);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  // AsyncGenerateConsistencyToken
  if (config.cancel_generate_token) {
    op->Cancel();
  }

  cq_impl->SimulateCompletion(cq, true);

  if (config.expect_check_consistency_call) {
    EXPECT_FALSE(user_op_called);
    EXPECT_EQ(1U, cq_impl->size());  // AsyncCheckConsistency
    if (config.cancel_check_consistency) {
      op->Cancel();
    }
    cq_impl->SimulateCompletion(cq, true);
  }

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

INSTANTIATE_TEST_CASE_P(
    CancelTest, NoexAsyncCheckConsistencyCancel,
    ::testing::Values(
        // Cancel during GenerateConsistencyTokenResponse
        // yields the request returning CANCELLED.
        CancelConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::CANCELLED,
            // Cancel while in GenerateToken.
            true,
            // Expect that AsyncAwaitConsistency won't call CheckConsistency.
            false,
            // Error code which CheckConsistency returns - irrelevant.
            grpc::StatusCode::UNKNOWN,
            // Don't cancel while in CheckConsistency
            false,
            // Expected result.
            grpc::StatusCode::CANCELLED},
        // Cancel during GenerateConsistencyTokenResponse
        // yields the request returning OK.
        CancelConfig{
            // Error code which GenerateToken returns.
            grpc::StatusCode::OK,
            // Cancel while in GenerateToken.
            true,
            // Expect that AsyncAwaitConsistency won't call CheckConsistency.
            false,
            // Error code which CheckConsistency returns - irrelevant.
            grpc::StatusCode::UNKNOWN,
            // Don't cancel while in CheckConsistency
            false,
            // Expected result.
            grpc::StatusCode::CANCELLED},
        // Cancel during CheckConsistency
        // yields the request returning CANCELLED.
        CancelConfig{// Error code which GenerateToken returns.
                     grpc::StatusCode::OK,
                     // Don't cancel GenerateToken.
                     false,
                     // Expect that AsyncAwaitConsistency call CheckConsistency.
                     true,
                     // Error code which CheckConsistency returns
                     grpc::StatusCode::CANCELLED,
                     // Don't cancel while in CheckConsistency
                     true,
                     // Expected result.
                     grpc::StatusCode::CANCELLED},
        // Cancel during CheckConsistency
        // yields the request returning OK.
        CancelConfig{// Error code which GenerateToken returns.
                     grpc::StatusCode::OK,
                     // Don't cancel GenerateToken.
                     false,
                     // Expect that AsyncAwaitConsistency call CheckConsistency.
                     true,
                     // Error code which CheckConsistency returns
                     grpc::StatusCode::OK,
                     // Don't cancel while in CheckConsistency
                     true,
                     // Expected result.
                     grpc::StatusCode::OK}));

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
