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
  impl->SimulateCompletion(cq, true);

  // That should have created a timer, but not fired r2, verify that and
  // simulate the timer completion.
  EXPECT_FALSE(r2_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  // Once the timer completes, r2 should be fired, but op, verify this and
  // simulate r2 completing.
  EXPECT_TRUE(r2_called);
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  // At this point all requests and the final callback should be completed.
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
  impl->SimulateCompletion(cq, true);

  // At this point the final callback should be completed.
  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::FAILED_PRECONDITION, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("AsyncApply"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr(table_.table_name()));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("permanent error"));
  EXPECT_THAT(capture_status.error_message(), HasSubstr("uh-oh"));
}

void SetupMockForMultipleTransients(
    std::shared_ptr<testing::MockDataClient> client,
    std::vector<std::shared_ptr<MockAsyncApplyReader>>& readers) {
  // We prepare the client to return multiple readers, all of them returning
  // transient failures.
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
}

void SetupMockForMultipleCancellations(
    std::shared_ptr<testing::MockDataClient> client,
    std::vector<std::shared_ptr<MockAsyncApplyReader>>& readers) {
  // We prepare the client to return multiple readers, all of them returning
  // transient failures.
  EXPECT_CALL(*client, AsyncMutateRow(_, _, _))
      .WillRepeatedly(Invoke([&readers](grpc::ClientContext*,
                                        btproto::MutateRowRequest const&,
                                        grpc::CompletionQueue*) {
        auto r = std::make_shared<MockAsyncApplyReader>();
        readers.push_back(r);
        EXPECT_CALL(*r, Finish(_, _, _))
            .WillOnce(Invoke([](btproto::MutateRowResponse*,
                                grpc::Status* status, void*) {
              *status = grpc::Status(grpc::StatusCode::CANCELLED, "cancelled");
            }));
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::MutateRowResponse>>(r.get());
      }));
}

/// @test Verify that noex::Table::AsyncApply() stops retrying on too many
/// transient failures.
TEST_F(NoexTableAsyncApplyTest, TooManyTransientFailures) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  std::vector<std::shared_ptr<MockAsyncApplyReader>> readers;
  SetupMockForMultipleTransients(client_, readers);

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
    impl->SimulateCompletion(cq, true);
  }

  // Okay, simulate one more iteration, that should exhaust the policy:
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);
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
  impl->SimulateCompletion(cq, true);

  // At this point the final callback should be completed.
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

  std::vector<std::shared_ptr<MockAsyncApplyReader>> readers;
  SetupMockForMultipleCancellations(client_, readers);

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
  impl->SimulateCompletion(cq, true);

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

/// @test Verify that noex::Table::AsyncApply() doesn't retry if Finish()
/// returns false (even though it would have been a bug in gRPC).
TEST_F(NoexTableAsyncApplyTest, BuggyGrpcReturningFalseOnFinish) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  std::vector<std::shared_ptr<MockAsyncApplyReader>> readers;
  SetupMockForMultipleTransients(client_, readers);

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

  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  // let finish return false
  impl->SimulateCompletion(cq, false);

  // At this point the operation should immediately fail (UNKNOWN is not a
  // transient failure).
  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_EQ(grpc::StatusCode::UNKNOWN, capture_status.error_code());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("Finish()"));
}

/// @test Verify that noex::Table::AsyncApply() stops retrying if a timer
/// between attempts is cancelled.
TEST_F(NoexTableAsyncApplyTest, StopRetryOnTimerCancel) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  std::vector<std::shared_ptr<MockAsyncApplyReader>> readers;
  SetupMockForMultipleTransients(client_, readers);

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
  impl->SimulateCompletion(cq, true);

  // Cancel the pending timer.
  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, false);

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

struct Counter : public grpc::ClientAsyncResponseReaderInterface<
                     btproto::MutateRowResponse> {
  Counter() { ++constructor_count; }
  ~Counter() override { ++destructor_count; }
  void StartCall() override {}
  void ReadInitialMetadata(void*) override {}
  void Finish(btproto::MutateRowResponse*, grpc::Status*, void*) override {}

  static int constructor_count;
  static int destructor_count;
};

int Counter::constructor_count = 0;
int Counter::destructor_count = 0;

/// @test Verify that gRPC async reply handles are not deleted.
TEST_F(NoexTableAsyncApplyTest, AsyncReaderNotDeleted) {
  // If gRPC ever disables the optimization whereby std::unique_ptr<> does not
  // delete objects of type grpc::ClientAsyncResponseReaderInterface<T> the
  // tests will fail in the ASAN builds, but with very obscure errors. We expect
  // this test to clarify the problem for our future selves.
  auto counter = new Counter;
  {
    std::unique_ptr<
        grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>
        tmp(counter);
    EXPECT_EQ(1, Counter::constructor_count);
  }
  ASSERT_EQ(0, Counter::destructor_count) << R"""(
When this test was written gRPC specialized the deleter for
grpc::ClientAsyncResponseReaderInterface<T>. Consequently the tests that mock
these objects need to manually cleanup any instances of objects of that type. It
seems that gRPC no longer specializes the std::unique_ptr deleter. If that is
the case all the tests that perform manual cleanups may crash, or fail in the
ASAN builds. This test is here to (we hope) ease troubleshooting of this future
problem.
)""";

  // Manually cleanup because gRPC disables the automatic cleanup in
  // std::unique_ptr.
  delete counter;
  EXPECT_EQ(1, Counter::destructor_count);
  EXPECT_EQ(1, Counter::constructor_count);
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
