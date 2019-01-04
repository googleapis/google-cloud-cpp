// Copyright 2018 Google LLC
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

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::v2;
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;

class NoexTableAsyncBulkApplyTest
    : public bigtable::testing::internal::TableTestFixture {};

/// @test Verify that noex::Table::AsyncBulkApply() works in a simple case.
TEST_F(NoexTableAsyncBulkApplyTest, IdempotencyAndRetries) {
  // This test creates 3 mutations. First one will succeed straight away, second
  // on retry and third never, because it's not idempotent.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}),
      bt::SingleRowMutation("baz", {bt::SetCell("fam", "col", "qux")}));

  using bigtable::testing::MockClientAsyncReaderInterface;

  // reader1 will confirm only the first mutation and return UNAVAILABLE to
  // others.
  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(2);
          e.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        }
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {}));

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  // reader2 will confirm only the first mutation (the only one); the mutation
  // which used to be first should now be confirmed and the mutation which used
  // to be third is not idempotent, so should not be retried.
  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader2 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter2(reader2);
  EXPECT_CALL(*reader2, Read(_, _))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {}));

  EXPECT_CALL(*reader2, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::MutateRowsRequest const& r,
                                          grpc::CompletionQueue*, void*) {
        EXPECT_EQ(3, r.entries_size());
        return std::move(reader_deleter1);
      }))
      .WillOnce(Invoke([&reader_deleter2](grpc::ClientContext*,
                                          btproto::MutateRowsRequest const& r,
                                          grpc::CompletionQueue*, void*) {
        // Second invocation should only contain the second mutation.
        EXPECT_EQ(1, r.entries_size());
        EXPECT_EQ("bar", r.entries(0).row_key());
        return std::move(reader_deleter2);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool mutator_finished = false;
  table_.AsyncBulkApply(cq,
                        [&mutator_finished](CompletionQueue& cq,
                                            std::vector<FailedMutation>& failed,
                                            grpc::Status& status) {
                          EXPECT_EQ(1U, failed.size());
                          EXPECT_EQ("baz", failed[0].mutation().row_key());
                          EXPECT_TRUE(status.ok());
                          mutator_finished = true;
                        },
                        std::move(mut));

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  impl->SimulateCompletion(cq, false);
  // FinishTimer
  impl->SimulateCompletion(cq, true);
  // Second attempt
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(mutator_finished);
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(mutator_finished);
}

/// @test Verify that noex::Table::AsyncBulkApply() works when cancelled
TEST_F(NoexTableAsyncBulkApplyTest, Cancelled) {
  // This test attempts to write one mutation but fails straight away because
  // the user cancels the request.
  bt::BulkMutation mut(
      bt::SingleRowMutation("baz", {bt::SetCell("fam", "col", "qux")}));

  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::CANCELLED, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::MutateRowsRequest const&,
                                          grpc::CompletionQueue*, void*) {
        return std::move(reader_deleter1);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool mutator_finished = false;
  table_.AsyncBulkApply(cq,
                        [&mutator_finished](CompletionQueue& cq,
                                            std::vector<FailedMutation>& failed,
                                            grpc::Status& status) {
                          EXPECT_FALSE(status.ok());
                          EXPECT_EQ(grpc::StatusCode::CANCELLED,
                                    status.error_code());
                          mutator_finished = true;
                        }, std::move(mut));

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(mutator_finished);
  impl->SimulateCompletion(cq, false);
  // callback fired
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(mutator_finished);
}

/// @test Verify that noex::Table::AsyncBulkApply() works when permanent error
//  occurs
TEST_F(NoexTableAsyncBulkApplyTest, PermanentError) {
  // This test attempts to write a single mutation, which fails with
  // PERMISSION_DENIED, which is a permanent error, hence is not retried.
  bt::BulkMutation mut(
      bt::SingleRowMutation("baz", {bt::SetCell("fam", "col", "qux")}));

  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status =
            grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::MutateRowsRequest const&,
                                          grpc::CompletionQueue*, void*) {
        return std::move(reader_deleter1);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool mutator_finished = false;
  table_.AsyncBulkApply(cq,
                        [&mutator_finished](CompletionQueue& cq,
                                            std::vector<FailedMutation>& failed,
                                            grpc::Status& status) {
                          EXPECT_FALSE(status.ok());
                          EXPECT_EQ(grpc::StatusCode::PERMISSION_DENIED,
                                    status.error_code());
                          mutator_finished = true;
                        },
                        std::move(mut));

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(mutator_finished);
  impl->SimulateCompletion(cq, false);
  // callback fired
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(mutator_finished);
}

/// @test Verify that cancellation of noex::Table::AsyncBulkApply() works when
//  when the request is waiting for retry.
TEST_F(NoexTableAsyncBulkApplyTest, CancelledInTimer) {
  // This test attempts to write two mutations. The first mutation succeeds on
  // the first run, but the second fails in a transient way (UNAVAILABLE).
  // When BulkMutator waits for the right moment to retry, the operation gets
  // cancelled.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  using bigtable::testing::MockClientAsyncReaderInterface;

  // reader1 will confirm only the first mutation and return UNAVAILABLE to
  // the other.
  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        }
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {}));

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::MutateRowsRequest const& r,
                                          grpc::CompletionQueue*, void*) {
        EXPECT_EQ(2, r.entries_size());
        return std::move(reader_deleter1);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool mutator_finished = false;
  table_.AsyncBulkApply(cq,
                        [&mutator_finished](CompletionQueue& cq,
                                            std::vector<FailedMutation>& failed,
                                            grpc::Status& status) {
                          EXPECT_EQ(grpc::StatusCode::CANCELLED,
                                    status.error_code());
                          mutator_finished = true;
                        },
                        std::move(mut));

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  impl->SimulateCompletion(cq, false);
  EXPECT_FALSE(mutator_finished);
  // FinishTimer
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(mutator_finished);
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
