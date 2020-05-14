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
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/mock_instance_admin_client.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/mock_completion_queue.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::bigtable::v2::SampleRowKeysResponse;
using ::google::cloud::testing_util::MockCompletionQueue;
using ::testing::_;
using ::testing::Invoke;
using ::testing::WithParamInterface;

using MockAsyncLongrunningOpReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        google::longrunning::Operation>;

void OperationFinishedSuccessfully(google::longrunning::Operation& response,
                                   grpc::Status& status) {
  status = grpc::Status();
  response.set_name("test_operation_id");
  response.set_done(true);
  google::bigtable::v2::SampleRowKeysResponse response_content;
  response_content.set_row_key("test_response");
  auto any = absl::make_unique<google::protobuf::Any>();
  any->PackFrom(response_content);
  response.set_allocated_response(any.release());
}

class AsyncLongrunningOpFutureTest : public bigtable::testing::TableTestFixture,
                                     public WithParamInterface<bool> {};

TEST_P(AsyncLongrunningOpFutureTest, EndToEnd) {
  auto const success = GetParam();
  auto client = std::make_shared<testing::MockAdminClient>();
  auto cq_impl = std::make_shared<MockCompletionQueue>();
  bigtable::CompletionQueue cq(cq_impl);

  auto longrunning_reader = absl::make_unique<MockAsyncLongrunningOpReader>();
  EXPECT_CALL(*longrunning_reader, Finish(_, _, _))
      .WillOnce(Invoke([success](google::longrunning::Operation* response,
                                 grpc::Status* status, void*) {
        if (success) {
          OperationFinishedSuccessfully(*response, *status);
        } else {
          *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "oh no");
        }
      }));

  EXPECT_CALL(*client, AsyncGetOperation(_, _, _))
      .WillOnce(
          Invoke([&longrunning_reader](
                     grpc::ClientContext*,
                     google::longrunning::GetOperationRequest const& request,
                     grpc::CompletionQueue*) {
            EXPECT_EQ("test_operation_id", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(longrunning_reader.get());
          }));

  google::longrunning::Operation op_arg;
  op_arg.set_name("test_operation_id");
  auto polling_policy =
      bigtable::DefaultPollingPolicy(internal::kBigtableLimits);
  auto metadata_update_policy =
      MetadataUpdatePolicy(op_arg.name(), MetadataParamTypes::NAME);

  auto fut = internal::StartAsyncLongrunningOp<
      AdminClient, google::bigtable::v2::SampleRowKeysResponse>(
      __func__, polling_policy->clone(), metadata_update_policy, client, cq,
      std::move(op_arg));

  EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl->size());

  cq_impl->SimulateCompletion(true);

  auto res = fut.get();
  EXPECT_TRUE(cq_impl->empty());
  if (success) {
    ASSERT_STATUS_OK(res);
    EXPECT_EQ("test_response", res->row_key());
  } else {
    ASSERT_FALSE(res);
    EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
  }
}

INSTANTIATE_TEST_SUITE_P(EndToEnd, AsyncLongrunningOpFutureTest,
                         ::testing::Values(
                             // We succeeded to contact the service.
                             true,
                             // We failed to contact the service.
                             false));

class AsyncLongrunningOperationTest : public ::testing::Test {
 public:
  AsyncLongrunningOperationTest()
      : client_(std::make_shared<testing::MockAdminClient>()),
        cq_impl_(std::make_shared<MockCompletionQueue>()),
        cq_(cq_impl_),
        longrunning_reader_(absl::make_unique<MockAsyncLongrunningOpReader>()),
        context_(new grpc::ClientContext) {}

  /**
   * Expect a call to the operation.
   *
   * The mock operation will return:
   * - UNAVAILABLE error if !returned
   * - not finished operation if returned && !*returned
   * - PERMISSION_DENIED if returned && *returned && !**returned
   * - `SampleRowKeysResponse` with row_key equal to **returned otherwise
   */
  StatusOr<optional<StatusOr<SampleRowKeysResponse>>> SimulateCall(
      std::function<void(google::longrunning::Operation&, grpc::Status&)> const&
          response_filler,
      google::longrunning::Operation op) {
    EXPECT_CALL(*longrunning_reader_, Finish(_, _, _))
        .WillOnce(
            Invoke([response_filler](google::longrunning::Operation* response,
                                     grpc::Status* status, void*) {
              response_filler(*response, *status);
            }));

    EXPECT_CALL(*client_, AsyncGetOperation(_, _, _))
        .WillOnce(Invoke(
            [this](grpc::ClientContext*,
                   google::longrunning::GetOperationRequest const& request,
                   grpc::CompletionQueue*) {
              EXPECT_EQ("test_operation_id", request.name());
              // This is safe, see comments in MockAsyncResponseReader.
              return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>(longrunning_reader_.get());
            }));
    internal::AsyncLongrunningOperation<testing::MockAdminClient,
                                        SampleRowKeysResponse>
        operation(client_, std::move(op));
    auto fut = operation(cq_, std::move(context_));

    EXPECT_EQ(std::future_status::timeout, fut.wait_for(1_ms));
    EXPECT_EQ(1, cq_impl_->size());

    cq_impl_->SimulateCompletion(true);
    EXPECT_TRUE(cq_impl_->empty());

    return fut.get();
  }

  std::shared_ptr<testing::MockAdminClient> client_;
  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::unique_ptr<MockAsyncLongrunningOpReader> longrunning_reader_;
  std::unique_ptr<grpc::ClientContext> context_;
};

TEST_F(AsyncLongrunningOperationTest, Success) {
  google::longrunning::Operation op;
  op.set_name("test_operation_id");

  StatusOr<optional<StatusOr<SampleRowKeysResponse>>> res =
      SimulateCall(OperationFinishedSuccessfully, op);

  ASSERT_STATUS_OK(res);
  ASSERT_TRUE(*res);
  ASSERT_STATUS_OK(**res);
  EXPECT_EQ("test_response", (**res)->row_key());
}

TEST_F(AsyncLongrunningOperationTest, Unfinished) {
  google::longrunning::Operation op;
  op.set_name("test_operation_id");

  StatusOr<optional<StatusOr<SampleRowKeysResponse>>> res = SimulateCall(
      [](google::longrunning::Operation& response, grpc::Status& status) {
        status = grpc::Status();
        response.set_name("test_operation_id");
        response.set_done(false);
      },
      op);

  ASSERT_STATUS_OK(res);
  ASSERT_FALSE(*res);
}

TEST_F(AsyncLongrunningOperationTest, FinishedFailure) {
  google::longrunning::Operation op;
  op.set_name("test_operation_id");

  StatusOr<optional<StatusOr<SampleRowKeysResponse>>> res = SimulateCall(
      [](google::longrunning::Operation& response, grpc::Status& status) {
        status = grpc::Status();
        response.set_name("test_operation_id");
        response.set_done(true);
        auto error = absl::make_unique<google::rpc::Status>();
        error->set_code(grpc::StatusCode::PERMISSION_DENIED);
        error->set_message("something is broken");
        response.set_allocated_error(error.release());
      },
      op);

  ASSERT_STATUS_OK(res);
  ASSERT_TRUE(*res);
  ASSERT_FALSE(**res);
  ASSERT_EQ(StatusCode::kPermissionDenied, (*res)->status().code());
}

TEST_F(AsyncLongrunningOperationTest, PollExhausted) {
  google::longrunning::Operation op;
  op.set_name("test_operation_id");

  StatusOr<optional<StatusOr<SampleRowKeysResponse>>> res = SimulateCall(
      [](google::longrunning::Operation&, grpc::Status& status) {
        status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "oh no");
      },
      op);

  ASSERT_FALSE(res);
  ASSERT_EQ(StatusCode::kUnavailable, res.status().code());
}

TEST_F(AsyncLongrunningOperationTest, ImmediateSuccess) {
  google::longrunning::Operation op;
  grpc::Status dummy_status;
  OperationFinishedSuccessfully(op, dummy_status);

  internal::AsyncLongrunningOperation<testing::MockAdminClient,
                                      SampleRowKeysResponse>
      operation(client_, std::move(op));
  auto fut = operation(cq_, std::move(context_));
  EXPECT_TRUE(cq_impl_->empty());
  StatusOr<optional<StatusOr<SampleRowKeysResponse>>> res = fut.get();

  ASSERT_STATUS_OK(res);
  ASSERT_TRUE(*res);
  ASSERT_STATUS_OK(**res);
  EXPECT_EQ("test_response", (**res)->row_key());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
