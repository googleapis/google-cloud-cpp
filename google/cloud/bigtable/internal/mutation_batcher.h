// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATION_BATCHER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATION_BATCHER_H_

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/status.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <deque>
#include <functional>
#include <memory>
#include <queue>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
class Table;
}  // namespace noex

namespace internal {

// Type of callback executed on mutations completion. Just an alias for user
// convenience.
using AsyncApplyCompletionCallback =
    std::function<void(CompletionQueue&, grpc::Status&)>;
// Type of callback executed on mutations admission. Just an alias for user
// convenience.
using AsyncApplyAdmissionCallback = std::function<void(CompletionQueue&)>;

/**
 * Objects of this class pack single row mutations into bulk mutations.
 *
 * This class has two responsibilities:
 *  * packing mutations in batches
 *  * not admitting too many mutations (AKA flow control AKA backpressure)
 */
class MutationBatcher {
 public:
  /// Configuration for MutationBatcher.
  struct Options {
    Options();

    // A single RPC will not have more mutations than this.
    Options& SetMaxMutationsPerBatch(size_t max_mutations_per_batch_arg) {
      max_mutations_per_batch = max_mutations_per_batch_arg;
      return *this;
    }

    // Sum of mutations' sizes in a single RPC will not be larger than this.
    Options& SetMaxSizePerBatch(size_t max_size_per_batch_arg) {
      max_size_per_batch = max_size_per_batch_arg;
      return *this;
    }

    // There will be no more RPCs outstanding (except for retries) than this.
    Options& SetMaxBatches(size_t max_batches_arg) {
      max_batches = max_batches_arg;
      return *this;
    }

    // MutationBatcher will at most admit mutations of this total size.
    Options& SetMaxOustandingSize(size_t max_oustanding_size_arg) {
      max_oustanding_size = max_oustanding_size_arg;
      return *this;
    }

    size_t max_mutations_per_batch;
    size_t max_size_per_batch;
    size_t max_batches;
    size_t max_oustanding_size;
  };

  MutationBatcher(noex::Table& table, Options options = Options())
      : table_(table),
        options_(options),
        num_outstanding_batches_(),
        oustanding_size_(),
        cur_batch_(std::make_shared<Batch>()) {}

  std::shared_ptr<AsyncOperation> AsyncApply(
      CompletionQueue& cq, AsyncApplyCompletionCallback completion_callback,
      AsyncApplyAdmissionCallback admission_callback, SingleRowMutation mut);

 private:
  class Batch;

  /**
   * This structure represents a single mutation before it is admitted.
   */
  struct PendingSingleRowMutation {
    PendingSingleRowMutation(SingleRowMutation mut_arg,
                             AsyncApplyCompletionCallback completion_callback,
                             AsyncApplyAdmissionCallback admission_callback);

    SingleRowMutation mut;
    size_t num_mutations;
    size_t request_size;
    AsyncApplyCompletionCallback completion_callback;
    AsyncApplyAdmissionCallback admission_callback;
  };

  /**
   * This is a handle to the submitted mutation.
   *
   * It implements AsyncOperation to allow the user to cancel it.
   */
  class BatchedSingleRowMutation
      : public google::cloud::bigtable::AsyncOperation {
   public:
    // TODO(#1963): implement
    virtual void Cancel() override {}
  };

  /**
   * This class represents a single batch of mutations sent in one RPC.
   *
   * Objects of this class hold the accumulated mutations, their completion
   * callbacks and basic statistics.
   */
  class Batch {
   public:
    Batch()
        : num_mutations_(),
          requests_size_(),
          last_idx_(),
          attempt_finished_() {}
    size_t requests_size() { return requests_size_; }
    size_t num_mutations() { return num_mutations_; }
    BulkMutation TransferRequest() { return std::move(requests_); }

    void Add(PendingSingleRowMutation mut);
    // Returns the size of the completed mutations.
    size_t FireFailedCallbacks(CompletionQueue& cq,
                               std::vector<FailedMutation> failed);
    // Returns the size of the completed mutations.
    size_t FireSuccessfulCallbacks(CompletionQueue& cq,
                                   std::vector<int> indices);
    // Returns if this was the first attempt on this batch.
    bool AttemptFinished();

   private:
    struct MutationData {
      MutationData(PendingSingleRowMutation pending)
          : callback(std::move(pending.completion_callback)),
            num_mutations(pending.num_mutations),
            request_size(pending.request_size) {}
      AsyncApplyCompletionCallback callback;
      int num_mutations;
      int request_size;
    };

    std::mutex mu_;
    size_t num_mutations_;
    size_t requests_size_;
    BulkMutation requests_;
    int last_idx_;
    // Whether at least one AsyncBulkApply finished.
    bool attempt_finished_;
    // The reason why it's not simple std::vector is that we want this structure
    // to shrink as individual mutations complete, so that the user can have a
    // bound on the amount of overhead per outstanding Apply.
    std::unordered_map<int, MutationData> mutation_data_;
  };

  // Check if a mutation doesn't exceed allowed limits.
  grpc::Status IsValid(PendingSingleRowMutation& mut) const;
  // Check whether there is space for the passed mutation in the currently
  // constructed batch.
  bool HasSpaceFor(PendingSingleRowMutation const& mut) const;
  // Check if one can append a mutation to the currently constructed batch. Even
  // if there is space for the mutation, we shouldn't append mutations if some
  // other are not admitted yet.
  bool CanAppendToBatch(PendingSingleRowMutation const& mut) const {
    // If some mutations are already subject to flow control, don't admit any
    // new, even if there's space for them. Otherwise we might starve big
    // mutations.
    return pending_mutations_.empty() && HasSpaceFor(mut);
  }

  // Send the currently constructed batch if there are not too many outstanding
  // already. If there are no mutations in the batch, it's a noop.
  bool FlushIfPossible(CompletionQueue& cq);
  // Entry point for lower layers indicating that mutations with indices
  // `indices` in batch `batch` have finished successfully.
  void MutationsSucceeded(CompletionQueue& cq, MutationBatcher::Batch& batch,
                          std::vector<int> indices);
  // Entry point for lower layers indicating that mutations listed in `failed`
  // in batch `batch` have failed permanently.
  void MutationsFailed(CompletionQueue& cq, MutationBatcher::Batch& batch,
                       std::vector<FailedMutation> failed);
  // Entry point for lower layers indicating that an attempt to send the
  // `batch` was made. This might be called multiple times in case of retries.
  void BatchAttemptFinished(CompletionQueue& cq, MutationBatcher::Batch& batch);
  // Try to move mutations waiting in `pending_mutations_` to the currently
  // constructed batch. Unlocks `lk`.
  void TryAdmit(CompletionQueue& cq, std::unique_lock<std::mutex>& lk);
  // Append mutation `mut` to the currently constructed batch.
  void Admit(PendingSingleRowMutation mut);

  std::mutex mu_;
  noex::Table& table_;
  Options options_;

  // Num batches sent but not completed.
  size_t num_outstanding_batches_;
  // Size of admitted but uncompleted mutations.
  size_t oustanding_size_;

  // Currently contructed batch of mutations.
  std::shared_ptr<Batch> cur_batch_;

  // These are the mutations which have not been admission yet. If the user is
  // properly reacting to `admission_callback`s, there should be very few of
  // these (likely no more than one).
  std::queue<PendingSingleRowMutation> pending_mutations_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATION_BATCHER_H_
