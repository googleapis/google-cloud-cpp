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
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

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
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;  // NOLINT
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

    table_ = absl::make_unique<Table>(mock_, TableResource("p", "i", "t"));
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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
