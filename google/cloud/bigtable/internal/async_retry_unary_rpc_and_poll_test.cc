// Copyright 2020 Google LLC.
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
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
namespace {

namespace btproto = ::google::bigtable::admin::v2;
using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;

using MockAsyncLongrunningOpReader =
    ::google::cloud::testing_util::MockAsyncResponseReader<
        google::longrunning::Operation>;

class AsyncStartPollAfterRetryUnaryRpcTest
    : public bigtable::testing::TableTestFixture {
 public:
  AsyncStartPollAfterRetryUnaryRpcTest()
      : TableTestFixture(CompletionQueue(
            std::make_shared<testing_util::FakeCompletionQueueImpl>())),
        k_project_id("the-project"),
        k_instance_id("the-instance"),
        k_cluster_id("the-cluster"),
        k_table_id("the-table"),
        no_retries{
            std::chrono::hours(0),
            std::chrono::hours(0),
            std::chrono::hours(0),
        },
        polling_policy(bigtable::DefaultPollingPolicy(no_retries)),
        rpc_retry_policy(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy(
            bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits)),
        metadata_update_policy(
            "projects/" + k_project_id + "/instances/" + k_instance_id,
            MetadataParamTypes::PARENT),
        client(std::make_shared<testing::MockInstanceAdminClient>(
            ClientOptions().DisableBackgroundThreads(cq_))),
        create_cluster_reader(
            absl::make_unique<MockAsyncLongrunningOpReader>()),
        get_operation_reader(
            absl::make_unique<MockAsyncLongrunningOpReader>()) {
    EXPECT_CALL(*client, project())
        .WillRepeatedly(::testing::ReturnRef(k_project_id));
  }
  void ExpectCreateCluster(grpc::StatusCode mocked_code) {
    EXPECT_CALL(*create_cluster_reader, Finish)
        .WillOnce([mocked_code](longrunning::Operation* response,
                                grpc::Status* status, void*) {
          response->set_name("create_cluster_op_1");
          *status = mocked_code != grpc::StatusCode::OK
                        ? grpc::Status(mocked_code, "mocked-status")
                        : grpc::Status::OK;
        });
    EXPECT_CALL(*client, AsyncCreateCluster)
        .WillOnce([this](grpc::ClientContext* context,
                         btproto::CreateClusterRequest const& request,
                         grpc::CompletionQueue*) {
          EXPECT_STATUS_OK(IsContextMDValid(
              *context,
              "google.bigtable.admin.v2.BigtableInstanceAdmin.CreateCluster",
              google::cloud::internal::ApiClientHeader()));
          EXPECT_EQ("my_newly_created_cluster", request.cluster_id());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<
              grpc::ClientAsyncResponseReaderInterface<longrunning::Operation>>(
              create_cluster_reader.get());
        });
  }

  void ExpectPolling(bool polling_finished,
                     grpc::StatusCode polling_error_code) {
    EXPECT_CALL(*get_operation_reader, Finish)
        .WillOnce([polling_finished, polling_error_code](
                      longrunning::Operation* response, grpc::Status* status,
                      void*) {
          if (!polling_finished) {
            *status = (polling_error_code != grpc::StatusCode::OK)
                          ? grpc::Status(polling_error_code, "mocked-status")
                          : grpc::Status::OK;
            return;
          }
          response->set_done(true);
          if (polling_error_code != grpc::StatusCode::OK) {
            auto error = absl::make_unique<google::rpc::Status>();
            error->set_code(polling_error_code);
            error->set_message("something is broken");
            response->set_allocated_error(error.release());
          } else {
            btproto::Cluster response_content;
            response_content.set_name("my_newly_created_cluster");
            auto any = absl::make_unique<google::protobuf::Any>();
            any->PackFrom(response_content);
            response->set_allocated_response(any.release());
          }
        });
    EXPECT_CALL(*client, AsyncGetOperation)
        .WillOnce([this](grpc::ClientContext* context,
                         longrunning::GetOperationRequest const& request,
                         grpc::CompletionQueue*) {
          EXPECT_STATUS_OK(IsContextMDValid(
              *context, "google.longrunning.Operations.GetOperation",
              google::cloud::internal::ApiClientHeader()));
          EXPECT_EQ("create_cluster_op_1", request.name());
          // This is safe, see comments in MockAsyncResponseReader.
          return std::unique_ptr<
              grpc::ClientAsyncResponseReaderInterface<longrunning::Operation>>(
              get_operation_reader.get());
        });
  }

  future<StatusOr<btproto::Cluster>> SimulateCreateCluster() {
    btproto::CreateClusterRequest request;
    request.set_cluster_id("my_newly_created_cluster");
    auto fut = internal::AsyncStartPollAfterRetryUnaryRpc<btproto::Cluster>(
        __func__, std::move(polling_policy), std::move(rpc_retry_policy),
        std::move(rpc_backoff_policy),
        internal::ConstantIdempotencyPolicy(
            google::cloud::internal::Idempotency::kNonIdempotent),
        std::move(metadata_update_policy), client,
        [this](grpc::ClientContext* context,
               btproto::CreateClusterRequest const& request,
               grpc::CompletionQueue* cq) {
          return client->AsyncCreateCluster(context, request, cq);
        },
        std::move(request), cq_);

    EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
    EXPECT_EQ(1U, cq_impl_->size());  // AsyncCreateCluster
    cq_impl_->SimulateCompletion(true);
    return fut;
  }

  std::string const k_project_id;
  std::string const k_instance_id;
  std::string const k_cluster_id;
  std::string const k_table_id;
  internal::RPCPolicyParameters const no_retries;
  std::unique_ptr<PollingPolicy> polling_policy;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy;
  MetadataUpdatePolicy metadata_update_policy;
  std::shared_ptr<testing::MockInstanceAdminClient> client;
  std::unique_ptr<MockAsyncLongrunningOpReader> create_cluster_reader;
  std::unique_ptr<MockAsyncLongrunningOpReader> get_operation_reader;
};

TEST_F(AsyncStartPollAfterRetryUnaryRpcTest, EverythingSucceeds) {
  ExpectCreateCluster(grpc::StatusCode::OK);
  ExpectPolling(true, grpc::StatusCode::OK);

  auto fut = SimulateCreateCluster();

  EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
  EXPECT_EQ(1U, cq_impl_->size());  // AsyncGetOperation
  cq_impl_->SimulateCompletion(true);

  auto res = fut.get();

  EXPECT_TRUE(cq_impl_->empty());
  ASSERT_STATUS_OK(res);
  EXPECT_EQ("my_newly_created_cluster", res->name());
}

TEST_F(AsyncStartPollAfterRetryUnaryRpcTest, NoPollingWhenCreateClusterFails) {
  ExpectCreateCluster(grpc::StatusCode::PERMISSION_DENIED);

  auto fut = SimulateCreateCluster();

  auto res = fut.get();

  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
}

TEST_F(AsyncStartPollAfterRetryUnaryRpcTest, PollTimesOutReturnsUnknown) {
  ExpectCreateCluster(grpc::StatusCode::OK);
  ExpectPolling(false, grpc::StatusCode::OK);

  auto fut = SimulateCreateCluster();

  EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
  EXPECT_EQ(1U, cq_impl_->size());  // AsyncGetOperation
  cq_impl_->SimulateCompletion(true);

  auto res = fut.get();

  EXPECT_TRUE(cq_impl_->empty());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kUnknown, res.status().code());
}

TEST_F(AsyncStartPollAfterRetryUnaryRpcTest,
       PollExhaustedOnFailuresReturnsLastError) {
  ExpectCreateCluster(grpc::StatusCode::OK);
  ExpectPolling(false, grpc::StatusCode::UNAVAILABLE);

  auto fut = SimulateCreateCluster();

  EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
  EXPECT_EQ(1U, cq_impl_->size());  // AsyncGetOperation
  cq_impl_->SimulateCompletion(true);

  auto res = fut.get();

  EXPECT_TRUE(cq_impl_->empty());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kUnavailable, res.status().code());
}

TEST_F(AsyncStartPollAfterRetryUnaryRpcTest, FinalErrorIsPassedOn) {
  ExpectCreateCluster(grpc::StatusCode::OK);
  ExpectPolling(true, grpc::StatusCode::UNAVAILABLE);

  auto fut = SimulateCreateCluster();

  EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
  EXPECT_EQ(1U, cq_impl_->size());  // AsyncGetOperation
  cq_impl_->SimulateCompletion(true);

  auto res = fut.get();

  EXPECT_TRUE(cq_impl_->empty());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kUnavailable, res.status().code());
}

}  // namespace
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
