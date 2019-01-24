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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATION_BATCHER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_MUTATION_BATCHER_H_

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

using AsyncApplyCompletionCallback =
    std::function<void(CompletionQueue&, grpc::Status&)>;
using AsyncApplyAdmissionCallback = std::function<void(CompletionQueue&)>;

class MutationBatcher {
 public:
  struct Options {
    Options(
        // Cloud Bigtable doesn't accept more than this.
        size_t max_mutations_per_batch = 100000,
        // Grpc has a 4MB limit. Let's make the default slightly smaller, so
        // that overheads or miscalculations don't tip us over.
        size_t max_size_per_batch = 4 * 1048576 * 9 / 10,
        size_t max_batches = 8, size_t max_oustanding_size = 24 * 1048576);

    Options& SetMaxMutationsPerBatch(size_t max_mutations_per_batch_arg);
    Options& SetMaxSizePerBatch(size_t max_size_per_batch_arg);
    Options& SetMaxBatches(size_t max_batches_arg);
    Options& SetMaxOustandingSize(size_t max_oustanding_size_arg);

    size_t max_mutations_per_batch;
    size_t max_size_per_batch;
    size_t max_batches;
    size_t max_oustanding_size;
  };

  MutationBatcher(noex::Table& table, Options options = Options());

  std::shared_ptr<AsyncOperation> AsyncApply(
      CompletionQueue& cq, AsyncApplyCompletionCallback&& completion_callback,
      AsyncApplyAdmissionCallback&& admission_callback,
      SingleRowMutation&& mut);

 private:
  class Batch;
  friend class BatchFinishedCallback;

  /**
   * This structure represents a single mutation before it is admitted.
   */
  struct PendingSingleRowMutation {
    PendingSingleRowMutation(SingleRowMutation&& mut_arg,
                             AsyncApplyCompletionCallback&& completion_callback,
                             AsyncApplyAdmissionCallback&& admission_callback);

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
    // TODO(dopiera): implement
    virtual void Cancel() override {}
  };

  class Batch {
   public:
    Batch() : num_mutations_(), requests_size_() {}
    size_t requests_size() { return requests_size_; }
    size_t num_mutations() { return num_mutations_; }
    BulkMutation TransferRequest() { return std::move(requests_); }

    void Add(PendingSingleRowMutation&& mut);
    void FireCallbacks(CompletionQueue& cq,
                       std::vector<FailedMutation> const& failed);

   private:
    size_t num_mutations_;
    size_t requests_size_;
    BulkMutation requests_;
    // The reason why it's not simple std::vector is that whenever the vector
    // grows, it will copy the callbacks. The callbacks might be large, so we
    // want to avoid that.
    //
    // In order for std::vector to move rather than copy, std::function's move
    // ctor would have to be noexcept (it is not until C++20).
    std::deque<AsyncApplyCompletionCallback> callbacks_;
  };

  grpc::Status IsValid(PendingSingleRowMutation& mut) const;
  bool HasSpaceFor(PendingSingleRowMutation const& mut) const;
  bool CanAppendToBatch(PendingSingleRowMutation const& mut) const {
    // If some mutations are already subject to flow control, don't admit any
    // new, even if there's space for them. Otherwise we might starve big
    // mutations.
    return pending_mutations_.empty() && HasSpaceFor(mut);
  }

  void FlushIfPossible(CompletionQueue& cq);
  void BatchFinished(CompletionQueue& cq, std::unique_ptr<Batch> batch,
                     std::vector<FailedMutation> const& failed);

  std::mutex mu_;
  noex::Table& table_;
  Options options_;

  size_t num_outstanding_batches_;
  size_t oustanding_size_;

  std::unique_ptr<Batch> cur_batch_;

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
