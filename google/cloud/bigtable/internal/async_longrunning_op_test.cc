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
  if (!has_error) {
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
    EXPECT_EQ(status.ok(), !has_error);
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

INSTANTIATE_TEST_SUITE_P(
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

INSTANTIATE_TEST_SUITE_P(
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

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
