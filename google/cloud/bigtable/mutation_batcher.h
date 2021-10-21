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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATION_BATCHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATION_BATCHER_H

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status.h"
#include "absl/memory/memory.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <deque>
#include <functional>
#include <memory>
#include <queue>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Objects of this class pack single row mutations into bulk mutations.
 *
 * In order to maximize throughput when applying a lot of mutations to Cloud
 * Bigtable, one should pack the mutations in `BulkMutations`. This class helps
 * in doing so. Create a `MutationBatcher` and use
 * `MutationBatcher::AsyncApply()` to apply a large stream of mutations to the
 * same `Table`. Objects of this class will efficiently create batches of
 * `SingleRowMutations` and maintain multiple batches "in flight".
 *
 * This class also offers an easy-to-use flow control mechanism to avoid
 * unbounded growth in its internal buffers.
 *
 * Applications must provide a `CompletionQueue` to (asynchronously) execute
 * these operations. The application is responsible of executing the
 * `CompletionQueue` event loop in one or more threads.
 *
 * @par Thread-safety
 * Instances of this class are guaranteed to work when accessed concurrently
 * from multiple threads.
 */
class MutationBatcher {
 public:
  /// Configuration for `MutationBatcher`.
  struct Options {
    Options();

    /// A single RPC will not have more mutations than this.
    Options& SetMaxMutationsPerBatch(size_t max_mutations_per_batch_arg) {
      max_mutations_per_batch = max_mutations_per_batch_arg;
      return *this;
    }

    /// Sum of mutations' sizes in a single RPC will not be larger than this.
    Options& SetMaxSizePerBatch(size_t max_size_per_batch_arg) {
      max_size_per_batch = max_size_per_batch_arg;
      return *this;
    }

    /// There will be no more RPCs outstanding (except for retries) than this.
    Options& SetMaxBatches(size_t max_batches_arg) {
      max_batches = max_batches_arg;
      return *this;
    }

    /// MutationBatcher will at most admit mutations of this total size.
    Options& SetMaxOutstandingSize(size_t max_outstanding_size_arg) {
      max_outstanding_size = max_outstanding_size_arg;
      return *this;
    }

    std::size_t max_mutations_per_batch;
    std::size_t max_size_per_batch;
    std::size_t max_batches;
    std::size_t max_outstanding_size;
  };

  explicit MutationBatcher(Table table, Options options = Options())
      : table_(std::move(table)),
        options_(options),
        num_outstanding_batches_(),
        outstanding_size_(),
        num_requests_pending_(),
        cur_batch_(std::make_shared<Batch>()) {}

  virtual ~MutationBatcher() = default;

  /**
   * Asynchronously apply mutation.
   *
   * The mutation will most likely be batched together with others to optimize
   * for throughput. As a result, latency is likely to be worse than
   * `Table::AsyncApply`.
   *
   * @param mut the mutation. Note that this function takes ownership
   *    (and then discards) the data in the mutation.  In general, a
   *    `SingleRowMutation` can be used to modify and/or delete
   *    multiple cells, across different columns and column families.
   * @param cq the completion queue that will execute the asynchronous
   *    calls, the application must ensure that one or more threads are
   *    blocked on `cq.Run()`.
   *
   * @return *admission* and *completion* futures
   *
   * The *completion* future will report the mutation's status once it
   * completes.
   *
   * The *admission* future should be used for flow control. In order to bound
   * the memory usage used by `MutationBatcher`, one should not submit more
   * mutations before the *admission* future is satisfied. Note that while the
   * future is often already satisfied when the function returns, applications
   * should not assume that this is always the case.
   *
   * One should not make assumptions on which future will be satisfied first.
   *
   * This quasi-synchronous example shows the intended use:
   * @code
   * bigtable::MutationBatcher batcher(bigtable::Table(...args...));
   * bigtable::CompletionQueue cq;
   * std::thread cq_runner([]() { cq.Run(); });
   *
   * while (HasMoreMutations()) {
   *   auto admission_completion = batcher.AsyncApply(cq, GenerateMutation());
   *   auto& admission_future = admission_completion.first;
   *   auto& completion_future = admission_completion.second;
   *   completion_future.then([](future<Status> completion_status) {
   *       // handle mutation completion asynchronously
   *       });
   *   // Potentially slow down submission not to make buffers in
   *   // MutationBatcher grow unbounded.
   *   admission_future.get();
   * }
   * // Wait for all mutations to complete
   * batcher.AsyncWaitForNoPendingRequests().get();
   * cq.Shutdown();
   * cq_runner.join();
   * @endcode
   */
  std::pair<future<void>, future<Status>> AsyncApply(CompletionQueue& cq,
                                                     SingleRowMutation mut);

  /**
   * Asynchronously wait until all submitted mutations complete.
   *
   * @return a future which will be satisfied once all mutations submitted
   *     before calling this function finish; if there are no such operations,
   *     the returned future is already satisfied.
   */
  future<void> AsyncWaitForNoPendingRequests();

 protected:
  // Wrap calling underlying operation in a virtual function to ease testing.
  virtual future<std::vector<FailedMutation>> AsyncBulkApplyImpl(
      Table& table, BulkMutation&& mut);

 private:
  using CompletionPromise = promise<Status>;
  using AdmissionPromise = promise<void>;
  using NoMorePendingPromise = promise<void>;
  struct Batch;

  /**
   * This structure represents a single mutation before it is admitted.
   */
  struct PendingSingleRowMutation {
    PendingSingleRowMutation(SingleRowMutation mut_arg,
                             CompletionPromise completion_promise,
                             AdmissionPromise admission_promise);

    SingleRowMutation mut;
    size_t num_mutations;
    size_t request_size;
    CompletionPromise completion_promise;
    AdmissionPromise admission_promise;
  };

  /**
   * A mutation that has been sent to the Cloud Bigtable service.
   *
   * We need to save the `CompletionPromise` associated with each mutation.
   * Because only failures are reported, we need to track whether the mutation
   * is "done", so we can simulate a success report.
   */
  struct MutationData {
    explicit MutationData(PendingSingleRowMutation pending)
        : completion_promise(std::move(pending.completion_promise)),
          done(false) {}
    CompletionPromise completion_promise;
    bool done;
  };

  /**
   * This class represents a single batch of mutations sent in one RPC.
   *
   * Objects of this class hold the accumulated mutations, their completion
   * promises and basic statistics.
   *
   * Objects of this class don't need separate synchronization.
   * There are 2 important stages of these objects' lifecycle: when mutations
   * are accumulated and when the batch is worked on by `AsyncBulkApply`. In the
   * first stage, `MutationBatcher`'s synchronization ensures that its data is
   * not accessed from multiple threads. In the second stage we rely on the fact
   * that `AsyncBulkApply` invokes the callbacks serially. This in turn
   * relies on the fact that `CompletionQueue` invokes callbacks from a
   * streaming response in sequence and that `AsyncRetryOp` doesn't schedule
   * another attempt before invoking callbacks for the previous one.
   */
  struct Batch {
    Batch() = default;

    size_t num_mutations{};
    size_t requests_size{};
    BulkMutation requests;
    std::vector<MutationData> mutation_data;
  };

  /// Check if a mutation doesn't exceed allowed limits.
  grpc::Status IsValid(PendingSingleRowMutation& mut) const;

  /**
   * Check whether there is space for the passed mutation in the currently
   * constructed batch.
   */
  bool HasSpaceFor(PendingSingleRowMutation const& mut) const;

  /**
   * Check if one can append a mutation to the currently constructed batch.
   * Even if there is space for the mutation, we shouldn't append mutations if
   * some other are not admitted yet.
   */
  bool CanAppendToBatch(PendingSingleRowMutation const& mut) const {
    // If some mutations are already subject to flow control, don't admit any
    // new, even if there's space for them. Otherwise we might starve big
    // mutations.
    return pending_mutations_.empty() && HasSpaceFor(mut);
  }

  /**
   * Send the currently constructed batch if there are not too many outstanding
   * already. If there are no mutations in the batch, it's a noop.
   */
  bool FlushIfPossible(CompletionQueue cq);

  /// Handle a completed batch.
  void OnBulkApplyDone(CompletionQueue cq, MutationBatcher::Batch batch,
                       std::vector<FailedMutation> const& failed);

  /**
   * Try to move mutations waiting in `pending_mutations_` to the currently
   * constructed batch.
   *
   * @return the admission promises of the newly admitted mutations.
   */
  std::vector<MutationBatcher::AdmissionPromise> TryAdmit(CompletionQueue& cq);

  /**
   * Append mutation `mut` to the currently constructed batch.
   */
  void Admit(PendingSingleRowMutation mut);

  /**
   * Satisfies passed admission promises and potentially the promises of no more
   * pending requests. Unlocks `lk`.
   */
  void SatisfyPromises(std::vector<AdmissionPromise>,
                       std::unique_lock<std::mutex>& lk);

  std::mutex mu_;
  Table table_;
  Options options_;

  /// Num batches sent but not completed.
  size_t num_outstanding_batches_;
  /// Size of admitted but uncompleted mutations.
  size_t outstanding_size_;
  // Number of uncompleted SingleRowMutations (including not admitted).
  size_t num_requests_pending_;

  /// Currently constructed batch of mutations.
  std::shared_ptr<Batch> cur_batch_;

  /**
   * These are the mutations which have not been admitted yet. If the user is
   * properly reacting to `admission_promise`s, there should be very few of
   * these (likely no more than one).
   */
  std::queue<PendingSingleRowMutation> pending_mutations_;

  /**
   * The list of promises made to this point.
   *
   * These promises are satisfied as part of calling
   * `AsyncWaitForNoPendingRequests()`.
   */
  std::vector<NoMorePendingPromise> no_more_pending_promises_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATION_BATCHER_H
