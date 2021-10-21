// Copyright 2020 Google LLC
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

#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/internal/async_longrunning_op.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::MockAsyncResponseReader;

using MockAsyncLongrunningOpReader =
    MockAsyncResponseReader<google::longrunning::Operation>;

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

class AsyncPollOpTest : public bigtable::testing::TableTestFixture {
 protected:
  AsyncPollOpTest()
      : TableTestFixture(
            CompletionQueue(std::make_shared<FakeCompletionQueueImpl>())),
        polling_policy_(
            bigtable::DefaultPollingPolicy(internal::kBigtableLimits)),
        metadata_update_policy_("test_operation_id", MetadataParamTypes::NAME),
        client_(new testing::MockAdminClient(
            ClientOptions().DisableBackgroundThreads(cq_))) {}

  std::shared_ptr<PollingPolicy const> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<testing::MockAdminClient> client_;
};

TEST_F(AsyncPollOpTest, AsyncPollOpTestSuccess) {
  auto reader = absl::make_unique<MockAsyncLongrunningOpReader>();

  EXPECT_CALL(*reader, Finish)
      .WillOnce(
          [](google::longrunning::Operation* response, grpc::Status* status,
             void*) { OperationFinishedSuccessfully(*response, *status); });
  EXPECT_CALL(*client_, AsyncGetOperation)
      .WillOnce(
          [&reader](grpc::ClientContext*,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue*) {
            EXPECT_EQ("test_operation_id", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(reader.get());
          });

  google::longrunning::Operation op_arg;
  op_arg.set_name("test_operation_id");

  auto poll_op_future = internal::StartAsyncPollOp(
      __func__, polling_policy_->clone(), metadata_update_policy_, cq_,
      internal::AsyncLongrunningOperation<
          AdminClient, google::bigtable::v2::SampleRowKeysResponse>(
          client_, std::move(op_arg)));

  EXPECT_EQ(std::future_status::timeout, poll_op_future.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());

  cq_impl_->SimulateCompletion(true);

  auto res = poll_op_future.get().value();

  ASSERT_STATUS_OK(res);
  EXPECT_EQ("test_response", res->row_key());
}

TEST_F(AsyncPollOpTest, AsyncPollOpTestPermanentFailure) {
  auto reader = absl::make_unique<MockAsyncLongrunningOpReader>();
  EXPECT_CALL(*reader, Finish)
      .WillOnce([](google::longrunning::Operation*, grpc::Status* status,
                   void*) {
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "oh no");
      });
  EXPECT_CALL(*client_, AsyncGetOperation)
      .WillOnce(
          [&reader](grpc::ClientContext*,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue*) {
            EXPECT_EQ("test_operation_id", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(reader.get());
          });

  google::longrunning::Operation op_arg;
  op_arg.set_name("test_operation_id");

  auto poll_op_future = internal::StartAsyncPollOp(
      __func__, polling_policy_->clone(), metadata_update_policy_, cq_,
      internal::AsyncLongrunningOperation<
          AdminClient, google::bigtable::v2::SampleRowKeysResponse>(
          client_, std::move(op_arg)));

  EXPECT_EQ(std::future_status::timeout, poll_op_future.wait_for(1_ms));
  EXPECT_EQ(1, cq_impl_->size());

  cq_impl_->SimulateCompletion(true);

  auto res = poll_op_future.get();
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
}

TEST_F(AsyncPollOpTest, AsyncPollOpTestPolicyExhausted) {
  auto reader = absl::make_unique<MockAsyncLongrunningOpReader>();
  LimitedErrorCountRetryPolicy retry(/*maximum_failures=*/3);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1));
  GenericPollingPolicy<LimitedErrorCountRetryPolicy> polling(retry, backoff);

  EXPECT_CALL(*reader, Finish)
      .WillRepeatedly(
          [](google::longrunning::Operation*, grpc::Status* status, void*) {
            *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "oh no");
          });

  EXPECT_CALL(*client_, AsyncGetOperation)
      .WillRepeatedly(
          [&reader](grpc::ClientContext*,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue*) {
            EXPECT_EQ("test_operation_id", request.name());
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                google::longrunning::Operation>>(reader.get());
          });

  google::longrunning::Operation op_arg;
  op_arg.set_name("test_operation_id");

  auto poll_op_future = internal::StartAsyncPollOp(
      __func__, polling.clone(), metadata_update_policy_, cq_,
      internal::AsyncLongrunningOperation<
          AdminClient, google::bigtable::v2::SampleRowKeysResponse>(
          client_, std::move(op_arg)));

  EXPECT_EQ(std::future_status::timeout, poll_op_future.wait_for(5_ms));
  EXPECT_EQ(1, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);
  cq_impl_->SimulateCompletion(true);
  cq_impl_->SimulateCompletion(true);

  auto res = poll_op_future.get();

  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kUnknown, res.status().code());
}
}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
