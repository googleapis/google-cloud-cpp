// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/mutation_batcher.h"
#include "google/cloud/bigtable/mocks/mock_data_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace bt = ::google::cloud::bigtable;

using ::google::cloud::bigtable_mocks::MockDataConnection;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::chrono_literals::operator""_ms;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::WithParamInterface;

std::size_t MutationSize(SingleRowMutation mut) {
  google::bigtable::v2::MutateRowsRequest::Entry entry;
  mut.MoveTo(&entry);
  return entry.ByteSizeLong();
}

Status BadStatus() { return Status(StatusCode::kAborted, "fail"); }

struct Exchange {
  Exchange(BulkMutation mut, std::vector<int> fails) : req(std::move(mut)) {
    res.reserve(fails.size());
    std::transform(fails.begin(), fails.end(), std::back_inserter(res),
                   [](int i) { return FailedMutation(BadStatus(), i); });
  }

  BulkMutation req;
  std::vector<FailedMutation> res;
};

struct MutationState {
  bool admitted{false};
  bool completed{false};
  Status completion_status;
};

class MutationStates {
 public:
  explicit MutationStates(std::vector<std::shared_ptr<MutationState>> states)
      : states_(std::move(states)) {}

  bool AllAdmitted() {
    return std::all_of(states_.begin(), states_.end(),
                       [](std::shared_ptr<MutationState> const& state) {
                         return state->admitted;
                       });
  }

  bool AllCompleted() {
    return std::all_of(states_.begin(), states_.end(),
                       [](std::shared_ptr<MutationState> const& state) {
                         return state->completed;
                       });
  }

  bool NoneAdmitted() {
    return std::none_of(states_.begin(), states_.end(),
                        [](std::shared_ptr<MutationState> const& state) {
                          return state->admitted;
                        });
  }

  bool NoneCompleted() {
    return std::none_of(states_.begin(), states_.end(),
                        [](std::shared_ptr<MutationState> const& state) {
                          return state->completed;
                        });
  }

  std::vector<std::shared_ptr<MutationState>> states_;
};

class MutationBatcherTest : public ::testing::Test {
 protected:
  MutationBatcherTest() {
    EXPECT_CALL(*mock_cq_, RunAsync).WillRepeatedly([this](Operation op) {
      operations_.emplace(std::move(op));
    });
    EXPECT_CALL(*mock_, options).WillRepeatedly(Return(Options{}));

    table_ = std::make_unique<Table>(mock_, TableResource("p", "i", "t"));
  }

  void ExpectInteraction(std::vector<Exchange> const& interactions) {
    ::testing::InSequence seq;
    for (auto interaction : interactions) {
      EXPECT_CALL(*mock_, AsyncBulkApply)
          .WillOnce(
              [interaction](std::string const&, BulkMutation mut) mutable {
                google::bigtable::v2::MutateRowsRequest expected;
                interaction.req.MoveTo(&expected);
                google::bigtable::v2::MutateRowsRequest actual;
                mut.MoveTo(&actual);
                EXPECT_THAT(actual, IsProtoEqual(expected));
                return make_ready_future(interaction.res);
              });
    }
  }

  void FinishSingleItemStream() {
    auto op = std::move(operations_.front());
    operations_.pop();
    op->exec();
  }

  std::shared_ptr<MutationState> Apply(MutationBatcher& batcher,
                                       SingleRowMutation mut) {
    auto res = std::make_shared<MutationState>();
    auto admission_and_completion = batcher.AsyncApply(cq_, std::move(mut));
    admission_and_completion.first.then([res](future<void> f) {
      f.get();
      res->admitted = true;
    });
    admission_and_completion.second.then([res](future<Status> status) {
      res->completed = true;
      res->completion_status = status.get();
    });
    return res;
  }

  template <typename Iterator>
  MutationStates ApplyMany(MutationBatcher& batcher, Iterator begin,
                           Iterator end) {
    std::vector<std::shared_ptr<MutationState>> res;
    for (; begin != end; ++begin) {
      res.emplace_back(Apply(batcher, *begin));
    }
    return MutationStates(std::move(res));
  }

  std::size_t NumOperationsOutstanding() { return operations_.size(); }

  std::shared_ptr<MockCompletionQueueImpl> mock_cq_ =
      std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq_{mock_cq_};

  using Operation = std::unique_ptr<google::cloud::internal::RunAsyncBase>;
  std::queue<Operation> operations_;

  std::shared_ptr<MockDataConnection> mock_ =
      std::make_shared<MockDataConnection>();
  std::unique_ptr<Table> table_;
};

TEST(OptionsTest, Defaults) {
  MutationBatcher::Options opt = MutationBatcher::Options();
  ASSERT_EQ(1000, opt.max_mutations_per_batch);
  ASSERT_EQ(4, opt.max_batches);
}

TEST(OptionsTest, Trivial) {
  MutationBatcher::Options opt = MutationBatcher::Options()
                                     .SetMaxMutationsPerBatch(1)
                                     .SetMaxSizePerBatch(2)
                                     .SetMaxBatches(3)
                                     .SetMaxOutstandingSize(4)
                                     .SetMaxOutstandingMutations(5);
  ASSERT_EQ(1, opt.max_mutations_per_batch);
  ASSERT_EQ(2, opt.max_size_per_batch);
  ASSERT_EQ(3, opt.max_batches);
  ASSERT_EQ(4, opt.max_outstanding_size);
  ASSERT_EQ(5, opt.max_outstanding_mutations);
}

TEST(OptionsTest, StrictLimits) {
  MutationBatcher::Options opt = MutationBatcher::Options()
                                     .SetMaxMutationsPerBatch(200000)
                                     .SetMaxOutstandingMutations(400000);
  // See `kBigtableMutationLimit`
  ASSERT_EQ(100000, opt.max_mutations_per_batch);
  // See `kBigtableOutstandingMutationLimit`
  ASSERT_EQ(300000, opt.max_outstanding_mutations);
}

TEST_F(MutationBatcherTest, TrivialTest) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")})});
  MutationBatcher batcher(*table_);

  ExpectInteraction({Exchange({mutations[0]}, {})});

  auto state = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state->admitted);
  EXPECT_FALSE(state->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state->completed);
  EXPECT_EQ(0, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, BatchIsFlushedImmediately) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo3", {bt::SetCell("fam", "col", 0_ms, "baz")})});
  MutationBatcher batcher(*table_, MutationBatcher::Options()
                                       .SetMaxMutationsPerBatch(10)
                                       .SetMaxSizePerBatch(2000)
                                       .SetMaxBatches(1)
                                       .SetMaxOutstandingSize(4000));

  ExpectInteraction({Exchange({mutations[0]}, {}),
                     Exchange({mutations[1], mutations[2]}, {})});

  auto state0 = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state0->admitted);
  EXPECT_FALSE(state0->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto state1 = ApplyMany(batcher, mutations.begin() + 1, mutations.end());
  EXPECT_TRUE(state1.AllAdmitted());
  EXPECT_TRUE(state1.NoneCompleted());
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state0->completed);
  EXPECT_TRUE(state1.NoneCompleted());
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state1.AllCompleted());
  EXPECT_EQ(0, NumOperationsOutstanding());
}

class MutationBatcherBoolParamTest : public MutationBatcherTest,
                                     public WithParamInterface<bool> {};

TEST_P(MutationBatcherBoolParamTest, PerBatchLimitsAreObeyed) {
  // The first SingleRowMutation will trigger a flush. Before it completes,
  // we'll try to schedule the next 3 (total 5 mutation in them), but only 2
  // will be admitted because there is a limit of 2 mutations.
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz"),
                                  bt::SetCell("fam", "col2", 0_ms, "baz"),
                                  bt::SetCell("fam", "col3", 0_ms, "baz")}),
       SingleRowMutation("foo3", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo4", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  bool hit_batch_size_limit = GetParam();

  MutationBatcher batcher(
      *table_, MutationBatcher::Options()
                   .SetMaxMutationsPerBatch(hit_batch_size_limit ? 1000 : 4)
                   .SetMaxSizePerBatch(hit_batch_size_limit
                                           ? (MutationSize(mutations[1]) +
                                              MutationSize(mutations[2]) +
                                              MutationSize(mutations[3]) - 1)
                                           : 2000)
                   .SetMaxBatches(1)
                   .SetMaxOutstandingSize(4000));

  ExpectInteraction(
      {Exchange({mutations[0]}, {}),
       // The only slot is now taken by the batch holding SingleRowMutation 0.
       Exchange({mutations[1], mutations[2]}, {}),
       // SingleRowMutations 1 and 2 fill up the batch, so mutation 3 won't fit.
       Exchange({mutations[3]}, {})}
      // Therefore, mutation 3 is executed in its own batch.
  );

  auto state0 = Apply(batcher, mutations[0]);

  EXPECT_TRUE(state0->admitted);
  EXPECT_FALSE(state0->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto immediately_admitted =
      ApplyMany(batcher, mutations.begin() + 1, mutations.begin() + 3);
  auto initially_not_admitted = Apply(batcher, mutations[3]);

  EXPECT_TRUE(immediately_admitted.AllAdmitted());
  EXPECT_TRUE(immediately_admitted.NoneCompleted());
  EXPECT_FALSE(initially_not_admitted->admitted);
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state0->completed);
  EXPECT_TRUE(immediately_admitted.NoneCompleted());
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_TRUE(initially_not_admitted->admitted);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(immediately_admitted.AllCompleted());
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(initially_not_admitted->completed);
  EXPECT_EQ(0, NumOperationsOutstanding());
}

INSTANTIATE_TEST_SUITE_P(SizeOrNumMutationsLimit, MutationBatcherBoolParamTest,
                         ::testing::Values(
                             // Test the #mutations limit
                             false,
                             // Test the size of mutations limit
                             true));

TEST_F(MutationBatcherTest, RequestsWithManyMutationsAreRejected) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col1", 0_ms, "baz"),
                                 bt::SetCell("fam", "col2", 0_ms, "baz"),
                                 bt::SetCell("fam", "col3", 0_ms, "baz")})});

  MutationBatcher batcher(
      *table_, MutationBatcher::Options().SetMaxMutationsPerBatch(2));

  auto state = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state->admitted);
  EXPECT_TRUE(state->completed);
  EXPECT_THAT(state->completion_status, Not(IsOk()));
  EXPECT_EQ(0, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, OutstandingMutationsAreCapped) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col1", 0_ms, "baz")}),
       SingleRowMutation("foo", {bt::SetCell("fam", "col1", 0_ms, "baz"),
                                 bt::SetCell("fam", "col2", 0_ms, "baz"),
                                 bt::SetCell("fam", "col3", 0_ms, "baz")})});

  // The second mutation will go through alone. But it will not go through if
  // the first mutation is outstanding due to the outstanding mutations limit.
  MutationBatcher batcher(
      *table_, MutationBatcher::Options().SetMaxOutstandingMutations(3));

  ExpectInteraction(
      {Exchange({mutations[0]}, {}), Exchange({mutations[1]}, {})});

  auto initially_admitted = Apply(batcher, mutations[0]);
  EXPECT_TRUE(initially_admitted->admitted);
  EXPECT_FALSE(initially_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto initially_not_admitted = Apply(batcher, mutations[1]);
  EXPECT_FALSE(initially_not_admitted->admitted);
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(initially_admitted->completed);
  EXPECT_STATUS_OK(initially_admitted->completion_status);
  EXPECT_TRUE(initially_not_admitted->admitted);
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(initially_not_admitted->completed);
  EXPECT_STATUS_OK(initially_not_admitted->completion_status);
  EXPECT_EQ(0, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, OutstandingMutationSizeIsCapped) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col1", 0_ms, "baz")}),
       SingleRowMutation("foo", {bt::SetCell("fam", "col1", 0_ms, "baz"),
                                 bt::SetCell("fam", "col2", 0_ms, "baz")})});

  // The second mutation will go through alone. But it will not go through if
  // the first mutation is outstanding due to the outstanding size limit.
  MutationBatcher batcher(*table_,
                          MutationBatcher::Options().SetMaxOutstandingSize(
                              MutationSize(mutations[1])));

  ExpectInteraction(
      {Exchange({mutations[0]}, {}), Exchange({mutations[1]}, {})});

  auto initially_admitted = Apply(batcher, mutations[0]);
  EXPECT_TRUE(initially_admitted->admitted);
  EXPECT_FALSE(initially_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto initially_not_admitted = Apply(batcher, mutations[1]);
  EXPECT_FALSE(initially_not_admitted->admitted);
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(initially_admitted->completed);
  EXPECT_STATUS_OK(initially_admitted->completion_status);
  EXPECT_TRUE(initially_not_admitted->admitted);
  EXPECT_FALSE(initially_not_admitted->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(initially_not_admitted->completed);
  EXPECT_STATUS_OK(initially_not_admitted->completion_status);
  EXPECT_EQ(0, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, LargeMutationsAreRejected) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col3", 0_ms, "baz")})});

  MutationBatcher batcher(*table_,
                          MutationBatcher::Options().SetMaxSizePerBatch(
                              MutationSize(mutations[0]) - 1));

  auto state = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state->admitted);
  EXPECT_TRUE(state->completed);
  EXPECT_THAT(state->completion_status, Not(IsOk()));
  EXPECT_EQ(0, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, MutationsExceedingMaxOutstandingSizeAreRejected) {
  constexpr std::size_t kMaxOutstandingSize = 1000;
  MutationBatcher::Options const options =
      MutationBatcher::Options().SetMaxOutstandingSize(kMaxOutstandingSize);
  SingleRowMutation too_large(
      "row-large", {bt::SetCell("fam", "col", 0_ms, std::string(2000, 'x'))});
  ASSERT_GT(MutationSize(too_large), options.max_outstanding_size);
  ASSERT_LE(MutationSize(too_large), options.max_size_per_batch);

  SingleRowMutation valid("row-small",
                          {bt::SetCell("fam", "col", 0_ms, "baz")});
  ASSERT_LE(MutationSize(valid), options.max_outstanding_size);

  MutationBatcher batcher(*table_, options);

  ExpectInteraction({Exchange({valid}, {})});

  auto [admission_future, completion_future] =
      batcher.AsyncApply(cq_, std::move(too_large));
  ASSERT_TRUE(admission_future.is_ready());
  admission_future.get();
  ASSERT_TRUE(completion_future.is_ready());
  EXPECT_THAT(completion_future.get(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("1000 bytes is the limit")));
  EXPECT_EQ(0, NumOperationsOutstanding());

  future<void> no_more_pending = batcher.AsyncWaitForNoPendingRequests();
  ASSERT_TRUE(no_more_pending.is_ready());
  no_more_pending.get();

  std::shared_ptr<MutationState> valid_state = Apply(batcher, valid);
  EXPECT_TRUE(valid_state->admitted);
  EXPECT_FALSE(valid_state->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(valid_state->completed);
  EXPECT_STATUS_OK(valid_state->completion_status);
  EXPECT_EQ(0, NumOperationsOutstanding());

  future<void> no_more_pending_after = batcher.AsyncWaitForNoPendingRequests();
  ASSERT_TRUE(no_more_pending_after.is_ready());
  no_more_pending_after.get();
}

TEST_F(MutationBatcherTest, ZeroMaxBatchesIsRejected) {
  SingleRowMutation mut("foo", {bt::SetCell("fam", "col", 0_ms, "baz")});
  MutationBatcher batcher(*table_, MutationBatcher::Options().SetMaxBatches(0));

  auto [admission_future, completion_future] =
      batcher.AsyncApply(cq_, std::move(mut));
  ASSERT_TRUE(admission_future.is_ready());
  admission_future.get();
  ASSERT_TRUE(completion_future.is_ready());
  EXPECT_THAT(completion_future.get(),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("max_batches")));
  EXPECT_EQ(0, NumOperationsOutstanding());

  future<void> no_more_pending = batcher.AsyncWaitForNoPendingRequests();
  ASSERT_TRUE(no_more_pending.is_ready());
  no_more_pending.get();
}

TEST_F(MutationBatcherTest, RequestsWithNoMutationsAreRejected) {
  std::vector<SingleRowMutation> mutations({SingleRowMutation("foo", {})});

  MutationBatcher batcher(*table_);

  auto state = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state->admitted);
  EXPECT_TRUE(state->completed);
  EXPECT_THAT(state->completion_status, Not(IsOk()));
  EXPECT_EQ(0, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, ErrorsArePropagated) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo3", {bt::SetCell("fam", "col", 0_ms, "baz")})});
  MutationBatcher batcher(*table_, MutationBatcher::Options().SetMaxBatches(1));

  ExpectInteraction({Exchange({mutations[0]}, {}),
                     Exchange({mutations[1], mutations[2]}, {1})});

  auto state0 = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state0->admitted);
  EXPECT_FALSE(state0->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto state1 = ApplyMany(batcher, mutations.begin() + 1, mutations.end());
  EXPECT_TRUE(state1.AllAdmitted());
  EXPECT_TRUE(state1.NoneCompleted());
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state0->completed);
  EXPECT_TRUE(state1.NoneCompleted());
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state1.AllCompleted());
  EXPECT_EQ(0, NumOperationsOutstanding());
  EXPECT_STATUS_OK(state1.states_[0]->completion_status);
  EXPECT_THAT(state1.states_[1]->completion_status, Not(IsOk()));
}

TEST_F(MutationBatcherTest, SmallMutationsDontSkipPending) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo3", {bt::SetCell("fam", "col1", 0_ms, "baz"),
                                  bt::SetCell("fam", "col2", 0_ms, "baz")}),
       SingleRowMutation("foo4", {bt::SetCell("fam", "col", 0_ms, "baz")})});
  MutationBatcher batcher(
      *table_,
      MutationBatcher::Options().SetMaxBatches(1).SetMaxMutationsPerBatch(2));

  // The first mutation flushes the batch immediately.
  // The second opens a new batch and waits until the first returns.
  // The third doesn't fit that batch, so becomes pending.
  // The fourth also becomes pending despite fitting in the open batch.

  ExpectInteraction({Exchange({mutations[0]}, {}), Exchange({mutations[1]}, {}),
                     Exchange({mutations[2]}, {}),
                     Exchange({mutations[3]}, {})});

  auto state0 = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state0->admitted);
  EXPECT_FALSE(state0->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto state1 = Apply(batcher, mutations[1]);
  EXPECT_TRUE(state1->admitted);
  EXPECT_FALSE(state1->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto state2 = Apply(batcher, mutations[2]);
  EXPECT_FALSE(state2->admitted);
  EXPECT_FALSE(state2->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto state3 = Apply(batcher, mutations[3]);
  EXPECT_FALSE(state3->admitted);
  EXPECT_FALSE(state3->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state0->completed);
  EXPECT_FALSE(state1->completed);
  EXPECT_FALSE(state2->completed);
  EXPECT_FALSE(state3->completed);
  EXPECT_TRUE(state2->admitted);
  EXPECT_FALSE(state3->admitted);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state1->completed);
  EXPECT_FALSE(state2->completed);
  EXPECT_FALSE(state3->completed);
  EXPECT_TRUE(state3->admitted);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state2->completed);
  EXPECT_FALSE(state3->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_TRUE(state3->completed);
  EXPECT_EQ(0, NumOperationsOutstanding());
}

// Test that waiting until all pending operations finish works in a simple case.
TEST_F(MutationBatcherTest, WaitForNoPendingSimple) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("baz", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  MutationBatcher batcher(
      *table_,
      MutationBatcher::Options().SetMaxMutationsPerBatch(2).SetMaxBatches(1));

  ExpectInteraction({Exchange({mutations[0]}, {}),
                     Exchange({mutations[1], mutations[2]}, {})});

  auto no_more_pending1 = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_EQ(no_more_pending1.wait_for(1_ms), std::future_status::ready);
  no_more_pending1.get();

  auto state0 = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state0->admitted);
  EXPECT_FALSE(state0->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());
  auto state1 = Apply(batcher, mutations[1]);
  auto state2 = Apply(batcher, mutations[2]);
  EXPECT_TRUE(state1->admitted);
  EXPECT_TRUE(state2->admitted);
  EXPECT_FALSE(state1->completed);
  EXPECT_FALSE(state2->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto no_more_pending2 = batcher.AsyncWaitForNoPendingRequests();
  auto no_more_pending3 = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_EQ(no_more_pending2.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending3.wait_for(1_ms), std::future_status::timeout);

  FinishSingleItemStream();

  EXPECT_TRUE(state0->completed);
  EXPECT_TRUE(state1->admitted);
  EXPECT_TRUE(state2->admitted);
  EXPECT_FALSE(state1->completed);
  EXPECT_FALSE(state2->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  EXPECT_EQ(no_more_pending2.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending3.wait_for(1_ms), std::future_status::timeout);

  FinishSingleItemStream();

  EXPECT_TRUE(state1->completed);
  EXPECT_TRUE(state2->completed);
  EXPECT_EQ(0, NumOperationsOutstanding());

  EXPECT_EQ(no_more_pending2.wait_for(1_ms), std::future_status::ready);
  EXPECT_EQ(no_more_pending3.wait_for(1_ms), std::future_status::ready);
}

// Test that pending and failed mutations are properly accounted.
TEST_F(MutationBatcherTest, WaitForNoPendingEdgeCases) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo3", {bt::SetCell("fam", "col", 0_ms, "baz"),
                                  bt::SetCell("fam", "col", 0_ms, "baz")})});

  MutationBatcher batcher(
      *table_,
      MutationBatcher::Options().SetMaxMutationsPerBatch(1).SetMaxBatches(1));

  ExpectInteraction(
      {Exchange({mutations[0]}, {0}), Exchange({mutations[1]}, {0})});

  auto state0 = Apply(batcher, mutations[0]);
  EXPECT_TRUE(state0->admitted);
  EXPECT_FALSE(state0->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto no_more_pending0 = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_EQ(no_more_pending0.wait_for(1_ms), std::future_status::timeout);

  auto state1 = Apply(batcher, mutations[1]);
  EXPECT_TRUE(state1->admitted);
  EXPECT_FALSE(state1->completed);
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto no_more_pending1 = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_EQ(no_more_pending0.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending1.wait_for(1_ms), std::future_status::timeout);

  auto state2 = Apply(batcher, mutations[2]);
  EXPECT_TRUE(state2->admitted);
  EXPECT_TRUE(state2->completed);
  EXPECT_THAT(state2->completion_status, Not(IsOk()));
  EXPECT_EQ(1, NumOperationsOutstanding());

  auto no_more_pending2 = batcher.AsyncWaitForNoPendingRequests();

  EXPECT_EQ(no_more_pending0.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending1.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending2.wait_for(1_ms), std::future_status::timeout);

  FinishSingleItemStream();

  EXPECT_TRUE(state0->completed);
  EXPECT_TRUE(state1->admitted);
  EXPECT_EQ(1, NumOperationsOutstanding());

  EXPECT_EQ(no_more_pending0.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending1.wait_for(1_ms), std::future_status::timeout);
  EXPECT_EQ(no_more_pending2.wait_for(1_ms), std::future_status::timeout);

  FinishSingleItemStream();

  EXPECT_TRUE(state1->completed);
  EXPECT_THAT(state1->completion_status, Not(IsOk()));
  EXPECT_EQ(0, NumOperationsOutstanding());

  EXPECT_EQ(no_more_pending0.wait_for(1_ms), std::future_status::ready);
  EXPECT_EQ(no_more_pending1.wait_for(1_ms), std::future_status::ready);
  EXPECT_EQ(no_more_pending2.wait_for(1_ms), std::future_status::ready);
}

TEST_F(MutationBatcherTest, CompletionContinuationCanWaitForNoPendingRequests) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")})});
  MutationBatcher batcher(*table_, MutationBatcher::Options().SetMaxBatches(1));

  ExpectInteraction({Exchange({mutations[0]}, {}),
                     Exchange({mutations[1], mutations[0]}, {})});

  std::shared_ptr<MutationState> state0 = Apply(batcher, mutations[0]);
  std::pair<future<void>, future<Status>> m1 =
      batcher.AsyncApply(cq_, mutations[1]);
  std::pair<future<void>, future<Status>> m2 =
      batcher.AsyncApply(cq_, mutations[0]);
  m1.first.get();
  m2.first.get();

  FinishSingleItemStream();
  EXPECT_TRUE(state0->completed);
  EXPECT_EQ(1U, NumOperationsOutstanding());

  future<void> no_more_pending = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_FALSE(no_more_pending.is_ready());

  bool m1_callback_finished = false;
  bool m2_callback_finished = false;
  bool wait_inside_m1_ready = true;
  bool inner_wait_saw_both_callbacks = false;
  m1.second.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    future<void> inner_wait = batcher.AsyncWaitForNoPendingRequests();
    wait_inside_m1_ready = inner_wait.is_ready();
    inner_wait.then([&](future<void> done) {
      done.get();
      inner_wait_saw_both_callbacks =
          m1_callback_finished && m2_callback_finished;
    });
    m1_callback_finished = true;
  });
  m2.second.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    m2_callback_finished = true;
  });

  bool outer_wait_saw_both_callbacks = false;
  no_more_pending.then([&](future<void> f) {
    f.get();
    outer_wait_saw_both_callbacks =
        m1_callback_finished && m2_callback_finished;
  });

  FinishSingleItemStream();

  EXPECT_FALSE(wait_inside_m1_ready);
  EXPECT_TRUE(m1_callback_finished);
  EXPECT_TRUE(m2_callback_finished);
  EXPECT_TRUE(inner_wait_saw_both_callbacks);
  EXPECT_TRUE(outer_wait_saw_both_callbacks);
  EXPECT_EQ(0U, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest,
       WaitForNoPendingWaitsForFollowUpAppliedInCompletionContinuation) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  MutationBatcher batcher(*table_, MutationBatcher::Options()
                                       .SetMaxOutstandingMutations(1)
                                       .SetMaxMutationsPerBatch(1)
                                       .SetMaxBatches(1));

  ExpectInteraction(
      {Exchange({mutations[0]}, {0}), Exchange({mutations[1]}, {})});

  std::pair<future<void>, future<Status>> first =
      batcher.AsyncApply(cq_, mutations[0]);
  first.first.get();

  future<void> no_more_pending = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_FALSE(no_more_pending.is_ready());

  future<Status> second_completion;
  first.second.then([&](future<Status> f) {
    EXPECT_THAT(f.get(), Not(IsOk()));
    std::pair<future<void>, future<Status>> second =
        batcher.AsyncApply(cq_, mutations[1]);
    second.first.get();
    second_completion = std::move(second.second);
  });

  FinishSingleItemStream();

  ASSERT_TRUE(second_completion.valid());
  EXPECT_FALSE(second_completion.is_ready());
  EXPECT_FALSE(no_more_pending.is_ready());
  EXPECT_EQ(1U, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_STATUS_OK(second_completion.get());
  EXPECT_TRUE(no_more_pending.is_ready());
  no_more_pending.get();
  EXPECT_EQ(0U, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, WaitForNoPendingAcrossConcurrentBatches) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  MutationBatcher batcher(
      *table_,
      MutationBatcher::Options().SetMaxMutationsPerBatch(1).SetMaxBatches(2));

  ExpectInteraction(
      {Exchange({mutations[0]}, {}), Exchange({mutations[1]}, {})});

  std::pair<future<void>, future<Status>> first =
      batcher.AsyncApply(cq_, mutations[0]);
  std::pair<future<void>, future<Status>> second =
      batcher.AsyncApply(cq_, mutations[1]);
  first.first.get();
  second.first.get();
  ASSERT_EQ(2U, NumOperationsOutstanding());

  future<void> pre_existing_wait = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_FALSE(pre_existing_wait.is_ready());

  promise<void> first_entered_callback;
  promise<void> release_first_callback;
  bool first_callback_done = false;
  first.second.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    first_entered_callback.set_value();
    release_first_callback.get_future().get();
    first_callback_done = true;
  });

  Operation op1 = std::move(operations_.front());
  operations_.pop();
  Operation op2 = std::move(operations_.front());
  operations_.pop();

  std::thread worker1([op = std::move(op1)]() mutable { op->exec(); });
  first_entered_callback.get_future().get();

  // Finish the second batch on another CQ worker while worker1 is still inside
  // the first batch's completion callback.
  std::thread worker2([op = std::move(op2)]() mutable { op->exec(); });
  worker2.join();
  EXPECT_STATUS_OK(second.second.get());

  // Neither the pre-existing wait nor a new wait call may resolve while
  // worker1's completion callback is still executing.
  EXPECT_FALSE(pre_existing_wait.is_ready());
  future<void> mid_callback_wait = batcher.AsyncWaitForNoPendingRequests();
  EXPECT_FALSE(mid_callback_wait.is_ready());

  release_first_callback.set_value();
  worker1.join();

  EXPECT_TRUE(first_callback_done);
  EXPECT_TRUE(pre_existing_wait.is_ready());
  EXPECT_TRUE(mid_callback_wait.is_ready());
  pre_existing_wait.get();
  mid_callback_wait.get();
}

TEST_F(MutationBatcherTest, CompletionContinuationCanApplyAtFlowControlLimit) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  MutationBatcher batcher(*table_, MutationBatcher::Options()
                                       .SetMaxOutstandingMutations(1)
                                       .SetMaxMutationsPerBatch(1)
                                       .SetMaxBatches(1));

  ExpectInteraction(
      {Exchange({mutations[0]}, {0}), Exchange({mutations[1]}, {})});

  std::pair<future<void>, future<Status>> first =
      batcher.AsyncApply(cq_, mutations[0]);
  ASSERT_TRUE(first.first.is_ready());
  first.first.get();
  EXPECT_EQ(1U, NumOperationsOutstanding());

  future<Status> second_completion;
  bool second_admitted_immediately = false;
  first.second.then([&](future<Status> f) {
    EXPECT_THAT(f.get(), Not(IsOk()));
    std::pair<future<void>, future<Status>> second =
        batcher.AsyncApply(cq_, mutations[1]);
    second_admitted_immediately = second.first.is_ready();
    second.first.get();
    second_completion = std::move(second.second);
  });

  FinishSingleItemStream();

  EXPECT_TRUE(second_admitted_immediately);
  ASSERT_TRUE(second_completion.valid());
  EXPECT_FALSE(second_completion.is_ready());
  EXPECT_EQ(1U, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_STATUS_OK(second_completion.get());
  EXPECT_EQ(0U, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest,
       CompletionContinuationObservesPendingAdmissionSatisfied) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  MutationBatcher batcher(*table_, MutationBatcher::Options()
                                       .SetMaxOutstandingMutations(1)
                                       .SetMaxMutationsPerBatch(1)
                                       .SetMaxBatches(1));

  ExpectInteraction(
      {Exchange({mutations[0]}, {}), Exchange({mutations[1]}, {})});

  std::pair<future<void>, future<Status>> first =
      batcher.AsyncApply(cq_, mutations[0]);
  ASSERT_TRUE(first.first.is_ready());
  first.first.get();

  std::pair<future<void>, future<Status>> second =
      batcher.AsyncApply(cq_, mutations[1]);
  EXPECT_FALSE(second.first.is_ready());

  bool second_ready_in_first_callback = false;
  first.second.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    second_ready_in_first_callback = second.first.is_ready();
    second.first.get();
  });

  FinishSingleItemStream();

  EXPECT_TRUE(second_ready_in_first_callback);
  EXPECT_FALSE(second.second.is_ready());
  EXPECT_EQ(1U, NumOperationsOutstanding());

  FinishSingleItemStream();

  EXPECT_STATUS_OK(second.second.get());
  EXPECT_EQ(0U, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest, DestroyBatcherFromCompletionContinuation) {
  std::vector<SingleRowMutation> mutations(
      {SingleRowMutation("foo1", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo2", {bt::SetCell("fam", "col", 0_ms, "baz")}),
       SingleRowMutation("foo3", {bt::SetCell("fam", "col", 0_ms, "baz")})});

  auto batcher = std::make_unique<MutationBatcher>(
      *table_, MutationBatcher::Options().SetMaxBatches(1));

  ExpectInteraction({Exchange({mutations[0]}, {}),
                     Exchange({mutations[1], mutations[2]}, {0})});

  std::shared_ptr<MutationState> state0 = Apply(*batcher, mutations[0]);
  std::pair<future<void>, future<Status>> m1 =
      batcher->AsyncApply(cq_, mutations[1]);
  std::pair<future<void>, future<Status>> m2 =
      batcher->AsyncApply(cq_, mutations[2]);
  m1.first.get();
  m2.first.get();

  FinishSingleItemStream();
  EXPECT_TRUE(state0->completed);
  EXPECT_EQ(1U, NumOperationsOutstanding());

  Status status1;
  Status status2;
  m1.second.then([&](future<Status> f) { status1 = f.get(); });
  m2.second.then([&](future<Status> f) {
    status2 = f.get();
    batcher.reset();
  });

  FinishSingleItemStream();

  EXPECT_EQ(nullptr, batcher);
  EXPECT_THAT(status1, Not(IsOk()));
  EXPECT_STATUS_OK(status2);
  EXPECT_EQ(0U, NumOperationsOutstanding());
}

TEST_F(MutationBatcherTest,
       DestroyBatcherAfterCompletionFutureGetMultiThreaded) {
  SingleRowMutation mut("foo", {bt::SetCell("fam", "col", 0_ms, "baz")});
  EXPECT_CALL(*mock_, AsyncBulkApply)
      .WillRepeatedly([](std::string const&, BulkMutation const&) {
        return make_ready_future(std::vector<FailedMutation>{});
      });

  for (int i = 0; i != 32; ++i) {
    auto batcher = std::make_unique<MutationBatcher>(*table_);
    std::pair<future<void>, future<Status>> admission_and_completion =
        batcher->AsyncApply(cq_, mut);
    admission_and_completion.first.get();
    ASSERT_EQ(1U, NumOperationsOutstanding());

    Operation op = std::move(operations_.front());
    operations_.pop();
    std::thread cq_worker([op = std::move(op)]() mutable { op->exec(); });

    EXPECT_STATUS_OK(admission_and_completion.second.get());
    batcher.reset();
    cq_worker.join();
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
