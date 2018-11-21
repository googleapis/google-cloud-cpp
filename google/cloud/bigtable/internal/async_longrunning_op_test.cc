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

#include "google/cloud/bigtable/internal/async_longrunning_op.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/internal/table_admin.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
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
using MockAsyncLongrunningOpReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        google::longrunning::Operation>;

class NoexAsyncLongrunningOpTest : public ::testing::Test {};

/// @test Verify that TableAdmin::LongrunningOp() works in a simplest case.
TEST_F(NoexAsyncLongrunningOpTest, Simple) {
  auto client = std::make_shared<testing::MockAdminClient>();
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto longrunning_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  EXPECT_CALL(*longrunning_reader, Finish(_, _, _))
      .WillOnce(Invoke([](google::longrunning::Operation* response,
                          grpc::Status* status, void*) {
        // This might be confusing, but we're using a GetOperationRequest as
        // response. This could have been any other message for the purpose of
        // this test, but this is simple, so it's handy.
        google::longrunning::GetOperationRequest response_content;
        response_content.set_name("asdfgh");
        auto any =
            google::cloud::internal::make_unique<google::protobuf::Any>();
        any->PackFrom(response_content);
        response->set_allocated_response(any.release());
        response->set_name("qwerty");
        response->set_done(true);
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client, AsyncGetOperation(_, _, _))
      .WillOnce(
          Invoke([&longrunning_reader](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     grpc::CompletionQueue*) {
            EXPECT_EQ("qwerty", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(longrunning_reader.get());
          }));

  bool user_op_called = false;
  auto user_callback = [&user_op_called](
                           CompletionQueue& cq,
                           google::longrunning::GetOperationRequest& response,
                           grpc::Status const& status) {
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("asdfgh", response.name());
    EXPECT_EQ("mocked-status", status.error_message());
    user_op_called = true;
  };
  using OpType = internal::AsyncPollLongrunningOp<
      decltype(user_callback), testing::MockAdminClient,
      google::longrunning::GetOperationRequest>;
  google::longrunning::Operation op_arg;
  op_arg.set_name("qwerty");
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(
      "instance_id", MetadataParamTypes::NAME, "table_id");
  auto op = std::make_shared<OpType>(
      __func__, polling_policy->clone(), metadata_update_policy, client,
      std::move(op_arg), std::move(user_callback));
  op->Start(cq);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());
  cq_impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

class NoexAsyncLongrunningImmediatelyFinished
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<bool> {};

// Verify that handling already finished operations works.
TEST_P(NoexAsyncLongrunningImmediatelyFinished, ImmeditalyFinished) {
  bool const has_error = GetParam();
  auto client = std::make_shared<testing::MockAdminClient>();
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  google::longrunning::Operation longrunning_op;
  longrunning_op.set_name("qwerty");
  longrunning_op.set_done(true);
  if (not has_error) {
    // This might be confusing, but we're using a GetOperationRequest as
    // response. This could have been any other message for the purpose of
    // this test, but this is simple, so it's handy.
    google::longrunning::GetOperationRequest response_content;
    response_content.set_name("asdfgh");
    auto any = google::cloud::internal::make_unique<google::protobuf::Any>();
    any->PackFrom(response_content);
    longrunning_op.set_allocated_response(any.release());
  } else {
    auto error = google::cloud::internal::make_unique<google::rpc::Status>();
    error->set_code(grpc::StatusCode::FAILED_PRECONDITION);
    error->set_message("something is broken");
    longrunning_op.set_allocated_error(error.release());
  }

  auto longrunning_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();

  // Make the asynchronous request.
  bool user_op_called = false;
  auto user_callback = [&user_op_called, has_error](
                           CompletionQueue& cq,
                           google::longrunning::GetOperationRequest& response,
                           grpc::Status const& status) {
    EXPECT_EQ(status.ok(), not has_error);
    if (has_error) {
      EXPECT_EQ(grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
    } else {
      EXPECT_EQ("asdfgh", response.name());
    }
    user_op_called = true;
  };
  using OpType = internal::AsyncPollLongrunningOp<
      decltype(user_callback), testing::MockAdminClient,
      google::longrunning::GetOperationRequest>;
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(
      "instance_id", MetadataParamTypes::NAME, "table_id");
  auto op = std::make_shared<OpType>(
      __func__, polling_policy->clone(), metadata_update_policy, client,
      std::move(longrunning_op), std::move(user_callback));
  op->Start(cq);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  // timer
  cq_impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

INSTANTIATE_TEST_CASE_P(
    ImmediateFinished, NoexAsyncLongrunningImmediatelyFinished,
    ::testing::Values(
        // The long running operation was successful.
        true,
        // The long running operation finished with an error.
        false));

class OpRetryConfig {
 public:
  grpc::StatusCode first_rpc_error_code;
  grpc::StatusCode whole_op_error_code;
};

class NoexAsyncLongrunningOpRetryTest
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<OpRetryConfig> {};

// Verify that one retry works. No reason for many tests - AsyncPollOp is
// thoroughly tested.
TEST_P(NoexAsyncLongrunningOpRetryTest, OneRetry) {
  auto const config = GetParam();
  auto client = std::make_shared<testing::MockAdminClient>();
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto longrunning_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  EXPECT_CALL(*longrunning_reader, Finish(_, _, _))
      .WillOnce(Invoke([config](google::longrunning::Operation* response,
                                grpc::Status* status, void*) {
        response->set_name("qwerty");
        response->set_done(false);
        *status = grpc::Status(config.first_rpc_error_code, "mocked-status");
      }))
      .WillOnce(Invoke([&config](google::longrunning::Operation* response,
                                 grpc::Status* status, void*) {
        response->set_name("qwerty");
        response->set_done(true);
        if (config.whole_op_error_code == grpc::StatusCode::OK) {
          // This might be confusing, but we're using a GetOperationRequest as
          // response. This could have been any other message for the purpose of
          // this test, but this is simple, so it's handy.
          google::longrunning::GetOperationRequest response_content;
          response_content.set_name("asdfgh");
          auto any =
              google::cloud::internal::make_unique<google::protobuf::Any>();
          any->PackFrom(response_content);
          response->set_allocated_response(any.release());
        } else {
          auto error =
              google::cloud::internal::make_unique<google::rpc::Status>();
          error->set_code(config.whole_op_error_code);
          error->set_message("something is broken");
          response->set_allocated_error(error.release());
        }
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client, AsyncGetOperation(_, _, _))
      .WillOnce(
          Invoke([&longrunning_reader](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     grpc::CompletionQueue*) {
            EXPECT_EQ("qwerty", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(longrunning_reader.get());
          }))
      .WillOnce(
          Invoke([&longrunning_reader](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     grpc::CompletionQueue*) {
            EXPECT_EQ("qwerty", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(longrunning_reader.get());
          }));

  // Make the asynchronous request.
  bool user_op_called = false;
  auto user_callback = [&user_op_called, &config](
                           CompletionQueue& cq,
                           google::longrunning::GetOperationRequest& response,
                           grpc::Status const& status) {
    EXPECT_EQ(config.whole_op_error_code, status.error_code());
    if (status.ok()) {
      EXPECT_EQ("asdfgh", response.name());
      EXPECT_EQ("mocked-status", status.error_message());
    }
    user_op_called = true;
  };
  using OpType = internal::AsyncPollLongrunningOp<
      decltype(user_callback), testing::MockAdminClient,
      google::longrunning::GetOperationRequest>;
  google::longrunning::Operation op_arg;
  op_arg.set_name("qwerty");
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(
      "instance_id", MetadataParamTypes::NAME, "table_id");
  auto op = std::make_shared<OpType>(
      __func__, polling_policy->clone(), metadata_update_policy, client,
      std::move(op_arg), std::move(user_callback));
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

INSTANTIATE_TEST_CASE_P(
    OneRetry, NoexAsyncLongrunningOpRetryTest,
    ::testing::Values(
        // First RPC returns an OK status but indicates
        // the long running op has not finished yet.
        OpRetryConfig{// The error code returned by the first attempt.
                      grpc::StatusCode::OK,
                      // The error code returned by the whole long running
                      // operation on the
                      // second attempt.
                      grpc::StatusCode::OK},
        // First RPC fails, but second succeeds.
        OpRetryConfig{// The error code returned by the first attempt.
                      grpc::StatusCode::UNAVAILABLE,
                      // The error code returned by the whole long running
                      // operation on the
                      // second attempt.
                      grpc::StatusCode::OK},
        // First RPC fails, second succeds, but indicates
        // that the operation failed permanently.
        OpRetryConfig{// The error code returned by the first attempt.
                      grpc::StatusCode::OK,
                      // The error code returned by the whole long running
                      // operation on the
                      // second attempt.
                      grpc::StatusCode::FAILED_PRECONDITION}));

// AsyncRetryUnaryRpcAndPollRes tests are on CreateCluterImpl to not overdo on
// mocks.

class EndToEndConfig {
 public:
  grpc::StatusCode create_clutester_error_code;
  bool expect_polling;
  grpc::StatusCode polling_error_code;
  bool polling_finished;
  grpc::StatusCode expected;
};

class AsyncRetryUnaryRpcAndPollResEndToEnd
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<EndToEndConfig> {};

TEST_P(AsyncRetryUnaryRpcAndPollResEndToEnd, EndToEnd) {
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
  auto rpc_retry_policy =
      bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits);
  auto rpc_backoff_policy =
      bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kProjectId,
                                              MetadataParamTypes::PARENT);

  auto client = std::make_shared<testing::MockInstanceAdminClient>();
  EXPECT_CALL(*client, project()).WillRepeatedly(ReturnRef(kProjectId));

  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto create_cluster_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  EXPECT_CALL(*create_cluster_reader, Finish(_, _, _))
      .WillOnce(Invoke([config](longrunning::Operation* response,
                                grpc::Status* status, void*) {
        response->set_name("create_cluster_op_1");
        *status =
            grpc::Status(config.create_clutester_error_code, "mocked-status");
      }));
  auto get_operation_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  if (config.expect_polling) {
    EXPECT_CALL(*get_operation_reader, Finish(_, _, _))
        .WillOnce(Invoke([config](longrunning::Operation* response,
                                  grpc::Status* status, void*) {
          if (not config.polling_finished) {
            *status = grpc::Status(config.polling_error_code, "mocked-status");
            return;
          }
          response->set_done(true);
          if (config.polling_error_code != grpc::StatusCode::OK) {
            auto error =
                google::cloud::internal::make_unique<google::rpc::Status>();
            error->set_code(config.polling_error_code);
            error->set_message("something is broken");
            response->set_allocated_error(error.release());
          } else {
            btproto::Cluster response_content;
            response_content.set_name("my_newly_created_cluster");
            auto any =
                google::cloud::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(response_content);
            response->set_allocated_response(any.release());
          }
        }));
  }

  EXPECT_CALL(*client, AsyncCreateCluster(_, _, _))
      .WillOnce(Invoke([&create_cluster_reader](
                           grpc::ClientContext*,
                           btproto::CreateClusterRequest const& request,
                           grpc::CompletionQueue*) {
        EXPECT_EQ("my_newly_created_cluster", request.cluster_id());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<longrunning::Operation>>(
            create_cluster_reader.get());
      }));
  if (config.expect_polling) {
    EXPECT_CALL(*client, AsyncGetOperation(_, _, _))
        .WillOnce(Invoke([&get_operation_reader](
                             grpc::ClientContext*,
                             longrunning::GetOperationRequest const& request,
                             grpc::CompletionQueue*) {
          EXPECT_EQ("create_cluster_op_1", request.name());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<
              grpc::ClientAsyncResponseReaderInterface<longrunning::Operation>>(
              get_operation_reader.get());
        }));
  }

  bool user_op_called = false;
  auto user_callback = [&user_op_called, &config](CompletionQueue& cq,
                                                  btproto::Cluster& cluster,
                                                  grpc::Status const& status) {
    user_op_called = true;
    EXPECT_EQ(config.expected, status.error_code());
    if (status.ok()) {
      EXPECT_EQ("my_newly_created_cluster", cluster.name());
    }
  };

  static_assert(
      internal::ExtractMemberFunctionType<decltype(
          &testing::MockInstanceAdminClient::AsyncCreateCluster)>::value,
      "Cannot extract member function type");
  using MemberFunction = typename internal::ExtractMemberFunctionType<decltype(
      &testing::MockInstanceAdminClient::AsyncCreateCluster)>::MemberFunction;

  using Retry =
      internal::AsyncRetryAndPollUnaryRpc<testing::MockInstanceAdminClient,
                                          btproto::Cluster, MemberFunction,
                                          internal::ConstantIdempotencyPolicy>;

  btproto::CreateClusterRequest request;
  request.set_cluster_id("my_newly_created_cluster");
  auto op = std::make_shared<Retry>(
      __func__, std::move(polling_policy), std::move(rpc_retry_policy),
      std::move(rpc_backoff_policy), internal::ConstantIdempotencyPolicy(false),
      metadata_update_policy, client,
      &testing::MockInstanceAdminClient::AsyncCreateCluster,
      std::move(request));
  op->Start(cq, user_callback);

  EXPECT_FALSE(user_op_called);

  EXPECT_EQ(1U, cq_impl->size());  // AsyncCreateCluster
  cq_impl->SimulateCompletion(cq, true);

  if (config.expect_polling) {
    EXPECT_FALSE(user_op_called);
    EXPECT_EQ(1U, cq_impl->size());  // AsyncGetOperation
    cq_impl->SimulateCompletion(cq, true);
  }

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

INSTANTIATE_TEST_CASE_P(
    EndToEnd, AsyncRetryUnaryRpcAndPollResEndToEnd,
    ::testing::Values(
        // Everything succeeds immediately.
        EndToEndConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::OK,
            // Expect that AsyncRetryUnaryRpcAndPollRes calls GetOperation.
            true,
            // Error code which GetOperation returns.
            grpc::StatusCode::OK,
            // GetOperation reports that it's finished.
            true,
            // Expected result.
            grpc::StatusCode::OK},
        // Sending CreateCluster fails, the error should be propagated.
        EndToEndConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::PERMISSION_DENIED,
            // Expect that AsyncRetryUnaryRpcAndPollRes won't call GetOperation.
            false,
            // Error code which GetOperation returns - ignored in this test.
            grpc::StatusCode::UNKNOWN,
            // GetOperation reports that it's finished - ignored in this test.
            true,
            // Expected result.
            grpc::StatusCode::PERMISSION_DENIED},
        // GetOperation times out w.r.t PollingPolicy, UNKNOWN is returned.
        EndToEndConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::OK,
            // Expect that AsyncRetryUnaryRpcAndPollRes calls GetOperation.
            true,
            // Error code which GetOperation returns.
            grpc::StatusCode::OK,
            // GetOperation reports that it's not yet finished.
            false,
            // Expected result.
            grpc::StatusCode::UNKNOWN},
        // GetOperation fails. UNKNOWN is returned.
        EndToEndConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::OK,
            // Expect that AsyncRetryUnaryRpcAndPollRes calls GetOperation.
            true,
            // Error code which GetOperation returns.
            grpc::StatusCode::UNAVAILABLE,
            // GetOperation reports that it's not yet finished.
            false,
            // Expected result.
            grpc::StatusCode::UNAVAILABLE},
        // GetOperation succeeds but reports an error - it is passed on.
        EndToEndConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::OK,
            // Expect that AsyncRetryUnaryRpcAndPollRes calls GetOperation.
            true,
            // Error code which GetOperation returns.
            grpc::StatusCode::UNAVAILABLE,
            // GetOperation reports that it's finished.
            true,
            // Expected result.
            grpc::StatusCode::UNAVAILABLE}));

class CancelConfig {
 public:
  // Error code returned from AsyncCreateCluster
  grpc::StatusCode create_clutester_error_code;
  // Whether to call cancel during AsyncCreateCluster
  bool cancel_create_cluster;
  // Whether to expect a call to GetOperation
  bool expect_polling;
  // Error code returned from GetOperation
  grpc::StatusCode polling_error_code;
  // Whether to call cancel during GetOperation
  bool cancel_polling;
  grpc::StatusCode expected;
};

class AsyncRetryUnaryRpcAndPollResCancel
    : public bigtable::testing::internal::TableTestFixture,
      public WithParamInterface<CancelConfig> {};

TEST_P(AsyncRetryUnaryRpcAndPollResCancel, Cancellations) {
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
  auto rpc_retry_policy =
      bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits);
  auto rpc_backoff_policy =
      bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits);
  MetadataUpdatePolicy metadata_update_policy(kProjectId,
                                              MetadataParamTypes::PARENT);

  auto client = std::make_shared<testing::MockInstanceAdminClient>();
  EXPECT_CALL(*client, project()).WillRepeatedly(ReturnRef(kProjectId));
  auto cq_impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto create_cluster_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  EXPECT_CALL(*create_cluster_reader, Finish(_, _, _))
      .WillOnce(Invoke([config](longrunning::Operation* response,
                                grpc::Status* status, void*) {
        response->set_name("create_cluster_op_1");
        *status =
            grpc::Status(config.create_clutester_error_code, "mocked-status");
      }));
  auto get_operation_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  if (config.expect_polling) {
    EXPECT_CALL(*get_operation_reader, Finish(_, _, _))
        .WillOnce(Invoke([config](longrunning::Operation* response,
                                  grpc::Status* status, void*) {
          response->set_done(true);
          if (config.polling_error_code != grpc::StatusCode::OK) {
            *status = grpc::Status(config.polling_error_code, "mocked-status");
          } else {
            btproto::Cluster response_content;
            response_content.set_name("my_newly_created_cluster");
            auto any =
                google::cloud::internal::make_unique<google::protobuf::Any>();
            any->PackFrom(response_content);
            response->set_allocated_response(any.release());
          }
        }));
  }

  EXPECT_CALL(*client, AsyncCreateCluster(_, _, _))
      .WillOnce(Invoke([&create_cluster_reader](
                           grpc::ClientContext*,
                           btproto::CreateClusterRequest const& request,
                           grpc::CompletionQueue*) {
        EXPECT_EQ("my_newly_created_cluster", request.cluster_id());
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<
            grpc::ClientAsyncResponseReaderInterface<longrunning::Operation>>(
            create_cluster_reader.get());
      }));
  if (config.expect_polling) {
    EXPECT_CALL(*client, AsyncGetOperation(_, _, _))
        .WillOnce(Invoke([&get_operation_reader](
                             grpc::ClientContext*,
                             longrunning::GetOperationRequest const& request,
                             grpc::CompletionQueue*) {
          EXPECT_EQ("create_cluster_op_1", request.name());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<
              grpc::ClientAsyncResponseReaderInterface<longrunning::Operation>>(
              get_operation_reader.get());
        }));
  }

  // Make the asynchronous request.
  bool user_op_called = false;
  auto user_callback = [&user_op_called, &config](CompletionQueue& cq,
                                                  btproto::Cluster& cluster,
                                                  grpc::Status const& status) {
    user_op_called = true;
    EXPECT_EQ(config.expected, status.error_code());
    if (status.ok()) {
      EXPECT_EQ("my_newly_created_cluster", cluster.name());
    }
  };

  static_assert(
      internal::ExtractMemberFunctionType<decltype(
          &testing::MockInstanceAdminClient::AsyncCreateCluster)>::value,
      "Cannot extract member function type");
  using MemberFunction = typename internal::ExtractMemberFunctionType<decltype(
      &testing::MockInstanceAdminClient::AsyncCreateCluster)>::MemberFunction;

  using Retry =
      internal::AsyncRetryAndPollUnaryRpc<testing::MockInstanceAdminClient,
                                          btproto::Cluster, MemberFunction,
                                          internal::ConstantIdempotencyPolicy>;

  btproto::CreateClusterRequest request;
  request.set_cluster_id("my_newly_created_cluster");
  auto op = std::make_shared<Retry>(
      __func__, std::move(polling_policy), std::move(rpc_retry_policy),
      std::move(rpc_backoff_policy), internal::ConstantIdempotencyPolicy(false),
      metadata_update_policy, client,
      &testing::MockInstanceAdminClient::AsyncCreateCluster,
      std::move(request));
  op->Start(cq, user_callback);

  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, cq_impl->size());  // AsyncCreateCluster
  if (config.cancel_create_cluster) {
    op->Cancel();
  }

  cq_impl->SimulateCompletion(cq, true);

  if (config.expect_polling) {
    EXPECT_FALSE(user_op_called);
    EXPECT_EQ(1U, cq_impl->size());  // AsyncGetOperation
    if (config.cancel_polling) {
      op->Cancel();
    }
    cq_impl->SimulateCompletion(cq, true);
  }

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(cq_impl->empty());
}

INSTANTIATE_TEST_CASE_P(
    CancelTest, AsyncRetryUnaryRpcAndPollResCancel,
    ::testing::Values(
        // Cancel during AsyncCreateCluster yields the request returning
        // CANCELLED.
        CancelConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::CANCELLED,
            // Cancel while in AsyncCreateCluster.
            true,
            // Expect that AsyncAwaitConsistency doesn't call GetOperation.
            false,
            // Error code which GetOperation returns - irrelevant.
            grpc::StatusCode::UNKNOWN,
            // Don't cancel while in GetOperation
            false,
            // Expected result.
            grpc::StatusCode::CANCELLED},
        // Cancel during AsyncCreateCluster yields the request returning OK.
        CancelConfig{
            // Error code which AsyncCreateCluster returns.
            grpc::StatusCode::OK,
            // Cancel while in AsyncCreateCluster.
            true,
            // Expect that AsyncAwaitConsistency doesn't call GetOperation.
            false,
            // Error code which GetOperation returns - irrelevant.
            grpc::StatusCode::UNKNOWN,
            // Don't cancel while in GetOperation
            false,
            // Expected result.
            grpc::StatusCode::CANCELLED},
        // Cancel during GetOperation yields the request returning CANCELLED.
        CancelConfig{// Error code which AsyncCreateCluster returns.
                     grpc::StatusCode::OK,
                     // Don't cancel AsyncCreateCluster.
                     false,
                     // Expect that AsyncAwaitConsistency calls GetOperation.
                     true,
                     // Error code which GetOperation returns
                     grpc::StatusCode::CANCELLED,
                     // Don't cancel while in GetOperation
                     true,
                     // Expected result.
                     grpc::StatusCode::CANCELLED},
        // Cancel during GetOperation yields the request returning OK.
        CancelConfig{// Error code which AsyncCreateCluster returns.
                     grpc::StatusCode::OK,
                     // Don't cancel AsyncCreateCluster.
                     false,
                     // Expect that AsyncAwaitConsistency calls GetOperation.
                     true,
                     // Error code which GetOperation returns
                     grpc::StatusCode::OK,
                     // Don't cancel while in GetOperation
                     true,
                     // Expected result.
                     grpc::StatusCode::OK}));

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
