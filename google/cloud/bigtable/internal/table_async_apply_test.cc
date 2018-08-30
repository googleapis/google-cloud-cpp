// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
namespace {
class NoexTableAsyncApplyTest
    : public bigtable::testing::internal::TableTestFixture {};

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using namespace google::cloud::testing_util::chrono_literals;
namespace btproto = google::bigtable::v2;

using testing::MockAsyncApplyReader;

/// @test Verify that noex::Table::AsyncApply() works in a simple case.
TEST_F(NoexTableAsyncApplyTest, SuccessAfterOneRetry) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Simulate a transient failure first.
  auto r1 = google::cloud::internal::make_unique<MockAsyncApplyReader>();
  bool r1_called = false;
  EXPECT_CALL(*r1, Finish(_, _, _))
      .WillOnce(Invoke([&r1_called](btproto::MutateRowResponse*,
                                    grpc::Status* status, void*) {
        r1_called = true;
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }));

  // Then simulate a successful response.
  auto r2 = google::cloud::internal::make_unique<MockAsyncApplyReader>();
  bool r2_called = false;
  EXPECT_CALL(*r2, Finish(_, _, _))
      .WillOnce(Invoke([&r2_called](btproto::MutateRowResponse* r,
                                    grpc::Status* status, void* tag) {
        r2_called = true;
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  // Because there is a transient failure, we expect two calls.
  EXPECT_CALL(*client_, AsyncMutateRow(_, _, _))
      .WillOnce(
          Invoke([&r1](grpc::ClientContext*, btproto::MutateRowRequest const&,
                       grpc::CompletionQueue*) {
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                btproto::MutateRowResponse>>(r1.get());
          }))
      .WillOnce(
          Invoke([&r2](grpc::ClientContext*, btproto::MutateRowRequest const&,
                       grpc::CompletionQueue*) {
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                btproto::MutateRowResponse>>(r2.get());
          }));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  table_.AsyncApply(
      bigtable::SingleRowMutation(
          "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}),
      cq,
      [&op_called, &capture_status](CompletionQueue& cq,
                                    google::bigtable::v2::MutateRowResponse& r,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      });

  // At this point r1 is fired, but neither r2, nor the final callback are
  // called, verify that and simulate the first request completing.
  EXPECT_TRUE(r1_called);
  EXPECT_FALSE(r2_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);

  // That should have created a timer, but not fired r2, verify that and
  // simulate the timer completion.
  EXPECT_TRUE(r1_called);
  EXPECT_FALSE(r2_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);

  // Once the timer completes, r2 should be fired, but op, verify this and
  // simulate r2 completing.
  EXPECT_TRUE(r1_called);
  EXPECT_TRUE(r2_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);

  // At this point all requests and the final callback should be completed.
  EXPECT_TRUE(r1_called);
  EXPECT_TRUE(r2_called);
  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_TRUE(capture_status.ok());
  EXPECT_EQ("mocked-status", capture_status.error_message());
}

/// @test Verify that noes::Table::AsyncApply() fails on a permanent error.
TEST_F(NoexTableAsyncApplyTest, PermanentFailure) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Simulate an immediate permanent failure.
  auto r1 = google::cloud::internal::make_unique<MockAsyncApplyReader>();
  bool r1_called = false;
  EXPECT_CALL(*r1, Finish(_, _, _))
      .WillOnce(Invoke([&r1_called](btproto::MutateRowResponse*,
                                    grpc::Status* status, void*) {
        r1_called = true;
        *status = grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh");
      }));

  // Because there is a permanent failure, we expect a single call.
  EXPECT_CALL(*client_, AsyncMutateRow(_, _, _))
      .WillOnce(
          Invoke([&r1](grpc::ClientContext*, btproto::MutateRowRequest const&,
                       grpc::CompletionQueue*) {
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                btproto::MutateRowResponse>>(r1.get());
          }));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  table_.AsyncApply(
      bigtable::SingleRowMutation(
          "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}),
      cq,
      [&op_called, &capture_status](CompletionQueue& cq,
                                    google::bigtable::v2::MutateRowResponse& r,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      });

  // At this point r1 is fired, but the final callback is not, verify that and
  // simulate the first request completing.
  EXPECT_TRUE(r1_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);

  // At this point the final callback should be completed.
  EXPECT_TRUE(r1_called);
  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::FAILED_PRECONDITION, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("AsyncApply"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr(table_.table_name()));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("permanent error"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("uh-oh"));
}

std::vector<std::shared_ptr<MockAsyncApplyReader>>
SetupMockForMultipleTransients(
    std::shared_ptr<testing::MockDataClient> client) {
  // We prepare the client to return multiple readers, all of them returning
  // transient failures.
  std::vector<std::shared_ptr<MockAsyncApplyReader>> readers;
  EXPECT_CALL(*client, AsyncMutateRow(_, _, _))
      .WillRepeatedly(Invoke([&readers](grpc::ClientContext*,
                                        btproto::MutateRowRequest const&,
                                        grpc::CompletionQueue*) {
        auto r = std::make_shared<MockAsyncApplyReader>();
        readers.push_back(r);
        EXPECT_CALL(*r, Finish(_, _, _))
            .WillOnce(Invoke(
                [](btproto::MutateRowResponse*, grpc::Status* status, void*) {
                  *status =
                      grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
                }));
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::MutateRowResponse>>(r.get());
      }));
  return readers;
}

/// @test Verify that noex::Table::AsyncApply() stops retrying on too many
/// transient failures.
TEST_F(NoexTableAsyncApplyTest, TooManyTransientFailures) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  auto readers = SetupMockForMultipleTransients(client_);

  // Create a table that accepts at most 3 failures.
  constexpr int kMaxTransients = 3;
  bigtable::noex::Table tested(
      client_, "test-table",
      bigtable::LimitedErrorCountRetryPolicy(kMaxTransients),
      bigtable::ExponentialBackoffPolicy(10_ms, 100_ms));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  tested.AsyncApply(
      bigtable::SingleRowMutation(
          "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}),
      cq,
      [&op_called, &capture_status](CompletionQueue& cq,
                                    google::bigtable::v2::MutateRowResponse& r,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      });

  // We expect call -> timer -> call -> timer -> call -> timer -> call[failed],
  // so simulate the cycle 6 times:
  for (int i = 0; i != 2 * kMaxTransients; ++i) {
    EXPECT_FALSE(op_called);
    EXPECT_EQ(1U, impl->size());
    impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);
  }

  // Okay, simulate one more iteration, that should exhaust the policy:
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::UNAVAILABLE, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("AsyncApply"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr(tested.table_name()));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("transient error"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("try-again"));

  impl->Shutdown();
}

/// @test Verify that noex::Table::AsyncApply() fails on transient errors
/// for non-idempotent calls.
TEST_F(NoexTableAsyncApplyTest, TransientFailureNonIdempotent) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Simulate an immediate permanent failure.
  auto r1 = google::cloud::internal::make_unique<MockAsyncApplyReader>();
  bool r1_called = false;
  EXPECT_CALL(*r1, Finish(_, _, _))
      .WillOnce(Invoke([&r1_called](btproto::MutateRowResponse*,
                                    grpc::Status* status, void*) {
        r1_called = true;
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
      }));

  // Because there is a permanent failure, we expect a single call.
  EXPECT_CALL(*client_, AsyncMutateRow(_, _, _))
      .WillOnce(
          Invoke([&r1](grpc::ClientContext*, btproto::MutateRowRequest const&,
                       grpc::CompletionQueue*) {
            // This is safe, see comments in MockAsyncResponseReader.
            return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                btproto::MutateRowResponse>>(r1.get());
          }));

  // Make the asynchronous request, use the server-side timestamp to test with
  // non-idempotent mutations.
  bool op_called = false;
  grpc::Status capture_status;
  table_.AsyncApply(
      bigtable::SingleRowMutation("bar",
                                  {bigtable::SetCell("fam", "col", "val")}),
      cq,
      [&op_called, &capture_status](CompletionQueue& cq,
                                    google::bigtable::v2::MutateRowResponse& r,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      });

  // At this point r1 is fired, but the final callback is not, verify that and
  // simulate the first request completing.
  EXPECT_TRUE(r1_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);

  // At this point the final callback should be completed.
  EXPECT_TRUE(r1_called);
  EXPECT_TRUE(op_called);
  EXPECT_EQ(0U, impl->size());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::UNAVAILABLE, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("AsyncApply"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr(table_.table_name()));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("non-idempotent"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("try-again"));
}

/// @test Verify that noex::Table::AsyncApply() stops retrying if one attempt
/// is canceled.
TEST_F(NoexTableAsyncApplyTest, StopRetryOnOperationCancel) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  auto readers = SetupMockForMultipleTransients(client_);

  // Create a table that accepts at most 3 failures.
  constexpr int kMaxTransients = 3;
  bigtable::noex::Table tested(
      client_, "test-table",
      bigtable::LimitedErrorCountRetryPolicy(kMaxTransients),
      bigtable::ExponentialBackoffPolicy(10_ms, 100_ms));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  tested.AsyncApply(
      bigtable::SingleRowMutation(
          "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}),
      cq,
      [&op_called, &capture_status](CompletionQueue& cq,
                                    google::bigtable::v2::MutateRowResponse& r,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      });

  // Cancel the pending operation, this should immediately fail.
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::CANCELLED);

  // At this point the operation should immediately fail.
  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::CANCELLED, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("AsyncApply"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr(tested.table_name()));
  EXPECT_THAT(capture_status.error_message(),
              HasSubstr("pending operation cancelled"));
}

/// @test Verify that noex::Table::AsyncApply() stops retrying if a timer
/// between attempts is cancelled.
TEST_F(NoexTableAsyncApplyTest, StopRetryOnTimerCancel) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  auto readers = SetupMockForMultipleTransients(client_);

  // Create a table that accepts at most 3 failures.
  constexpr int kMaxTransients = 3;
  bigtable::noex::Table tested(
      client_, "test-table",
      bigtable::LimitedErrorCountRetryPolicy(kMaxTransients),
      bigtable::ExponentialBackoffPolicy(10_ms, 100_ms));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  tested.AsyncApply(
      bigtable::SingleRowMutation(
          "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}),
      cq,
      [&op_called, &capture_status](CompletionQueue& cq,
                                    google::bigtable::v2::MutateRowResponse& r,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      });

  // Simulate a failure in the pending operation, that should create a timer.
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::COMPLETED);

  // Cancel the pending timer.
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, AsyncOperation::CANCELLED);

  // At this point the operation should immediately fail.
  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::CANCELLED, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("AsyncApply"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr(tested.table_name()));
  EXPECT_THAT(capture_status.error_message(),
              HasSubstr("pending timer cancelled"));
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
