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

#include "google/cloud/bigtable/internal/async_retry_unary_rpc_and_poll.h"
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

// AsyncRetryUnaryRpcAndPollRes tests use CreateCluter to not create new mocks.

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
        *status = config.create_clutester_error_code != grpc::StatusCode::OK
                      ? grpc::Status(config.create_clutester_error_code,
                                     "mocked-status")
                      : grpc::Status::OK;
      }));
  auto get_operation_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  if (config.expect_polling) {
    EXPECT_CALL(*get_operation_reader, Finish(_, _, _))
        .WillOnce(Invoke([config](longrunning::Operation* response,
                                  grpc::Status* status, void*) {
          if (!config.polling_finished) {
            *status =
                config.polling_error_code != grpc::StatusCode::OK
                    ? grpc::Status(config.polling_error_code, "mocked-status")
                    : grpc::Status::OK;
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

  using Retry = internal::AsyncRetryAndPollUnaryRpc<
      testing::MockInstanceAdminClient, btproto::Cluster, MemberFunction,
      internal::ConstantIdempotencyPolicy, decltype(user_callback)>;

  btproto::CreateClusterRequest request;
  request.set_cluster_id("my_newly_created_cluster");
  auto op = std::make_shared<Retry>(
      __func__, std::move(polling_policy), std::move(rpc_retry_policy),
      std::move(rpc_backoff_policy), internal::ConstantIdempotencyPolicy(false),
      metadata_update_policy, client,
      &testing::MockInstanceAdminClient::AsyncCreateCluster, std::move(request),
      std::move(user_callback));
  op->Start(cq);

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

INSTANTIATE_TEST_SUITE_P(
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
        *status = config.create_clutester_error_code != grpc::StatusCode::OK
                      ? grpc::Status(config.create_clutester_error_code,
                                     "mocked-status")
                      : grpc::Status::OK;
      }));
  auto get_operation_reader =
      google::cloud::internal::make_unique<MockAsyncLongrunningOpReader>();
  if (config.expect_polling) {
    EXPECT_CALL(*get_operation_reader, Finish(_, _, _))
        .WillOnce(Invoke([config](longrunning::Operation* response,
                                  grpc::Status* status, void*) {
          response->set_done(true);
          if (config.polling_error_code != grpc::StatusCode::OK) {
            *status =
                config.polling_error_code != grpc::StatusCode::OK
                    ? grpc::Status(config.polling_error_code, "mocked-status")
                    : grpc::Status::OK;
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

  using Retry = internal::AsyncRetryAndPollUnaryRpc<
      testing::MockInstanceAdminClient, btproto::Cluster, MemberFunction,
      internal::ConstantIdempotencyPolicy, decltype(user_callback)>;

  btproto::CreateClusterRequest request;
  request.set_cluster_id("my_newly_created_cluster");
  auto op = std::make_shared<Retry>(
      __func__, std::move(polling_policy), std::move(rpc_retry_policy),
      std::move(rpc_backoff_policy), internal::ConstantIdempotencyPolicy(false),
      metadata_update_policy, client,
      &testing::MockInstanceAdminClient::AsyncCreateCluster, std::move(request),
      std::move(user_callback));
  op->Start(cq);

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

INSTANTIATE_TEST_SUITE_P(
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
