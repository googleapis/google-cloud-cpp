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
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

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
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

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
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

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
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

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

class NoexTableStreamingAsyncBulkApplyTest
    : public bigtable::testing::internal::TableTestFixture {};

// @test Verify that noex::Table::StreamingAsyncBulkApply() works.
TEST_F(NoexTableStreamingAsyncBulkApplyTest, SimpleTest) {
  // Will succeed in the first batch in the first attempt
  SingleRowMutation succeed_first_batch("r1",
                                        {SetCell("fam", "col", 0_ms, "qux")});
  // Will succeed in the second batch in the first attempt
  SingleRowMutation succeed_second_batch("r2",
                                         {SetCell("fam", "col", 0_ms, "qux")});
  // Will transiently fail in the first attempt and succeed in the second
  SingleRowMutation transient_failure("r3",
                                      {SetCell("fam", "col", 0_ms, "qux")});
  // Will permanently fail in the first attempt
  SingleRowMutation permanent_failure("r5",
                                      {SetCell("fam", "col", 0_ms, "qux")});
  // Will never be confirmed
  SingleRowMutation never_confirmed("r6", {SetCell("fam", "col", 0_ms, "qux")});

  std::vector<SingleRowMutation> mutations{
      succeed_first_batch, succeed_second_batch, transient_failure,
      permanent_failure, never_confirmed};
  bt::BulkMutation mut(mutations.begin(), mutations.end());

  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  MockClientAsyncReaderInterface<btproto::MutateRowsResponse>* reader2 =
      new MockClientAsyncReaderInterface<btproto::MutateRowsResponse>;
  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        {
          // succeed_first_batch
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          // transient_failure
          auto& e = *r->add_entries();
          e.set_index(2);
          e.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        }
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        {
          // succeed_second_batch
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          // permanent_failure
          auto& e = *r->add_entries();
          e.set_index(3);
          e.mutable_status()->set_code(grpc::StatusCode::PERMISSION_DENIED);
        }
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {}));
  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "mocked-status");
      }));

  EXPECT_CALL(*reader2, Read(_, _))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {
        {
          // transient_failure now succeds
          auto& e = *r->add_entries();
          e.set_index(0);  // used to be index 2, but in the second attempt is 0
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
      }))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r, void*) {}));
  EXPECT_CALL(*reader2, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        // this should make the retry loop stop
        *status =
            grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncMutateRows(_, _, _, _))
      .WillOnce(Invoke([&reader1, mutations](
                           grpc::ClientContext*,
                           btproto::MutateRowsRequest const& req,
                           grpc::CompletionQueue*, void*) {
        int i = 0;
        std::vector<SingleRowMutation> mutations_copy(mutations);
        for (auto& m : mutations_copy) {
          google::bigtable::v2::MutateRowsRequest::Entry expected;
          m.MoveTo(&expected);
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, req.entries(i++))) << delta;
        }
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader1);
      }))
      .WillOnce(Invoke([&reader2, transient_failure, never_confirmed](
                           grpc::ClientContext*,
                           btproto::MutateRowsRequest const& req,
                           grpc::CompletionQueue*, void*) {
        EXPECT_EQ(2, req.entries_size());
        {
          google::bigtable::v2::MutateRowsRequest::Entry expected;
          SingleRowMutation transient_failure_copy(transient_failure);
          transient_failure_copy.MoveTo(&expected);
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, req.entries(0))) << delta;
        }
        {
          google::bigtable::v2::MutateRowsRequest::Entry expected;
          SingleRowMutation never_confirmed_copy(never_confirmed);
          never_confirmed_copy.MoveTo(&expected);
          std::string delta;
          google::protobuf::util::MessageDifferencer differencer;
          differencer.ReportDifferencesToString(&delta);
          EXPECT_TRUE(differencer.Compare(expected, req.entries(1))) << delta;
        }
        return std::unique_ptr<
            MockClientAsyncReaderInterface<btproto::MutateRowsResponse>>(
            reader2);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto cq_impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(cq_impl);

  std::vector<int> succeeded_mutations;
  std::vector<FailedMutation> failed_mutations_intermediate;
  std::vector<FailedMutation> failed_mutations_final;
  bool attempt_finished = false;
  bool whole_op_finished = false;
  table_.StreamingAsyncBulkApply(
      cq,
      [&succeeded_mutations](CompletionQueue& cq, std::vector<int> succeeded) {
        succeeded_mutations.swap(succeeded);
      },
      [&failed_mutations_intermediate](CompletionQueue& cq,
                                       std::vector<FailedMutation> failed) {
        failed_mutations_intermediate.swap(failed);
      },
      [&attempt_finished](CompletionQueue& cq, grpc::Status&) {
        attempt_finished = true;
      },
      [&whole_op_finished, &failed_mutations_final](
          CompletionQueue& cq, std::vector<FailedMutation>& failed,
          grpc::Status& status) {
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(grpc::StatusCode::PERMISSION_DENIED, status.error_code());
        whole_op_finished = true;
        failed_mutations_final.swap(failed);
      },
      std::move(mut));

  cq_impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  cq_impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read

  ASSERT_EQ(1, succeeded_mutations.size());
  // succeed_first_batch
  ASSERT_EQ(0, succeeded_mutations[0]);
  succeeded_mutations.clear();

  ASSERT_TRUE(failed_mutations_intermediate.empty());
  ASSERT_FALSE(whole_op_finished);
  ASSERT_TRUE(failed_mutations_final.empty());
  ASSERT_FALSE(attempt_finished);

  cq_impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 2 read

  // succeed_second_batch
  ASSERT_EQ(1, succeeded_mutations.size());
  EXPECT_EQ(1, succeeded_mutations[0]);
  succeeded_mutations.clear();

  // permanent_failure
  ASSERT_EQ(1, failed_mutations_intermediate.size());
  ASSERT_EQ(3, failed_mutations_intermediate[0].original_index());
  failed_mutations_intermediate.clear();

  ASSERT_FALSE(whole_op_finished);
  ASSERT_TRUE(failed_mutations_final.empty());
  ASSERT_FALSE(attempt_finished);

  cq_impl->SimulateCompletion(cq, false);
  // state == FINISHING

  cq_impl->SimulateCompletion(cq, true);
  // in timer

  ASSERT_FALSE(whole_op_finished);
  ASSERT_TRUE(failed_mutations_final.empty());
  ASSERT_TRUE(attempt_finished);
  attempt_finished = false;

  cq_impl->SimulateCompletion(cq, true);
  // timer finished
  cq_impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  cq_impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read

  ASSERT_EQ(1, succeeded_mutations.size());
  // transient_failure
  ASSERT_EQ(2, succeeded_mutations[0]);
  succeeded_mutations.clear();
  ASSERT_FALSE(whole_op_finished);
  ASSERT_FALSE(attempt_finished);
  ASSERT_TRUE(failed_mutations_intermediate.empty());
  ASSERT_TRUE(failed_mutations_final.empty());

  cq_impl->SimulateCompletion(cq, false);
  // state == FINISHING
  cq_impl->SimulateCompletion(cq, true);
  ASSERT_EQ(0, cq_impl->size());

  ASSERT_TRUE(whole_op_finished);
  ASSERT_TRUE(attempt_finished);
  ASSERT_TRUE(failed_mutations_intermediate.empty());
  ASSERT_EQ(1, failed_mutations_final.size());
  // never_confirmed
  ASSERT_EQ(4, failed_mutations_final[0].original_index());
}

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
