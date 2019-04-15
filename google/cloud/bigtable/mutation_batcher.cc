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

#include "google/cloud/bigtable/mutation_batcher.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/internal/table.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
MutationBatcher::Options::Options()
    :  // Cloud Bigtable doesn't accept more than this.
      max_mutations_per_batch(100000),
      // Let's make the default slightly smaller, so that overheads or
      // miscalculations don't tip us over.
      max_size_per_batch(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH * 9LL / 10),
      max_batches(8),
      max_outstanding_size(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH * 6) {}

std::pair<future<void>, future<Status>> MutationBatcher::AsyncApply(
    CompletionQueue& cq, SingleRowMutation mut) {
  AdmissionPromise admission_promise;
  CompletionPromise completion_promise;
  auto res = std::make_pair(admission_promise.get_future(),
                            completion_promise.get_future());
  PendingSingleRowMutation pending(std::move(mut),
                                   std::move(completion_promise),
                                   std::move(admission_promise));
  std::unique_lock<std::mutex> lk(mu_);

  grpc::Status mutation_status = IsValid(pending);
  if (!mutation_status.ok()) {
    lk.unlock();
    // Destroy the mutation before satisfying the admission promise so that we
    // can limit the memory usage.
    pending.mut.Clear();
    pending.completion_promise.set_value(
        internal::MakeStatusFromRpcError(mutation_status));
    // No need to consider no_more_pending_promises because this operation
    // didn't lower the number of pending operations.
    pending.admission_promise.set_value();
    return res;
  }
  ++num_requests_pending_;

  if (!CanAppendToBatch(pending)) {
    pending_mutations_.push(std::move(pending));
    return res;
  }
  std::vector<AdmissionPromise> admission_promises_to_satisfy;
  admission_promises_to_satisfy.emplace_back(
      std::move(pending.admission_promise));
  Admit(std::move(pending));
  FlushIfPossible(cq);
  SatisfyPromises(std::move(admission_promises_to_satisfy), lk);
  return res;
}

future<void> MutationBatcher::AsyncWaitForNoPendingRequests() {
  std::unique_lock<std::mutex> lk(mu_);
  if (num_requests_pending_ == 0) {
    // TODO(#2112): Use make_satisfied_future<> once it's implemented.
    promise<void> satisfied_promise;
    satisfied_promise.set_value();
    return satisfied_promise.get_future();
  }
  no_more_pending_promises_.emplace_back();
  return no_more_pending_promises_.back().get_future();
}

MutationBatcher::PendingSingleRowMutation::PendingSingleRowMutation(
    SingleRowMutation mut_arg, CompletionPromise completion_promise,
    AdmissionPromise admission_promise)
    : mut(std::move(mut_arg)),
      completion_promise(std::move(completion_promise)),
      admission_promise(std::move(admission_promise)) {
  ::google::bigtable::v2::MutateRowsRequest::Entry tmp;
  mut.MoveTo(&tmp);
  // This operation might not be cheap, so let's cache it.
  request_size = tmp.ByteSizeLong();
  num_mutations = tmp.mutations_size();
  mut = SingleRowMutation(std::move(tmp));
}

grpc::Status MutationBatcher::IsValid(PendingSingleRowMutation& mut) const {
  // Objects of this class need to be aware of the maximum allowed number of
  // mutations in a batch because it should not pack more. If we have this
  // knowledge, we might as well simplify everything and not admit larger
  // mutations.
  if (mut.num_mutations > options_.max_mutations_per_batch) {
    std::stringstream stream;
    stream << "Too many (" << mut.num_mutations
           << ") mutations in a SingleRowMutations request. "
           << options_.max_mutations_per_batch << " is the limit.";
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, stream.str());
  }
  if (mut.num_mutations == 0) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Supplied SingleRowMutations has no entries");
  }
  if (mut.request_size > options_.max_size_per_batch) {
    std::stringstream stream;
    stream << "Too large (" << mut.request_size
           << " bytes) mutation in a SingleRowMutations request. "
           << options_.max_size_per_batch << " bytes is the limit.";
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, stream.str());
  }
  return grpc::Status();
}

bool MutationBatcher::HasSpaceFor(PendingSingleRowMutation const& mut) const {
  return outstanding_size_ + mut.request_size <=
             options_.max_outstanding_size &&
         cur_batch_->requests_size + mut.request_size <=
             options_.max_size_per_batch &&
         cur_batch_->num_mutations + mut.num_mutations <=
             options_.max_mutations_per_batch;
}

bool MutationBatcher::FlushIfPossible(CompletionQueue& cq) {
  if (cur_batch_->num_mutations > 0 &&
      num_outstanding_batches_ < options_.max_batches) {
    ++num_outstanding_batches_;
    auto batch = cur_batch_;
    table_.impl_.StreamingAsyncBulkApply(
        cq,
        [this, batch](CompletionQueue& cq, std::vector<int> succeeded) {
          OnSuccessfulMutations(cq, *batch, std::move(succeeded));
        },
        [this, batch](CompletionQueue& cq, std::vector<FailedMutation> failed) {
          OnFailedMutations(cq, *batch, std::move(failed));
        },
        [this, batch](CompletionQueue& cq, grpc::Status&) {
          OnBulkApplyAttemptFinished(cq, *batch);
        },
        [this, batch](CompletionQueue& cq, std::vector<FailedMutation>& failed,
                      grpc::Status&) {
          // It means that there are not going to be anymore retries and the
          // final failed mutations are passed here.
          OnFailedMutations(cq, *batch, std::move(failed));
        },
        std::move(cur_batch_->requests));
    cur_batch_ = std::make_shared<Batch>();
    return true;
  }
  return false;
}

void MutationBatcher::OnSuccessfulMutations(CompletionQueue& cq,
                                            MutationBatcher::Batch& batch,
                                            std::vector<int> indices) {
  size_t const num_mutations = indices.size();
  size_t completed_size = 0;

  for (int idx : indices) {
    auto it = batch.mutation_data.find(idx);
    completed_size += it->second.request_size;
    it->second.completion_promise.set_value(Status());
    // Release resources as early as possible.
    batch.mutation_data.erase(it);
  }

  std::unique_lock<std::mutex> lk(mu_);
  outstanding_size_ -= completed_size;
  num_requests_pending_ -= num_mutations;
  SatisfyPromises(TryAdmit(cq), lk);  // unlocks the lock
}

void MutationBatcher::OnFailedMutations(CompletionQueue& cq,
                                        MutationBatcher::Batch& batch,
                                        std::vector<FailedMutation> failed) {
  size_t const num_mutations = failed.size();
  size_t completed_size = 0;

  for (auto const& f : failed) {
    int const idx = f.original_index();
    auto it = batch.mutation_data.find(idx);
    completed_size += it->second.request_size;
    it->second.completion_promise.set_value(f.status());
    // Release resources as early as possible.
    batch.mutation_data.erase(it);
  }
  // TODO(#2093): remove once `FailedMutations` are small.
  failed.clear();
  failed.shrink_to_fit();

  std::unique_lock<std::mutex> lk(mu_);
  outstanding_size_ -= completed_size;
  num_requests_pending_ -= num_mutations;
  SatisfyPromises(TryAdmit(cq), lk);  // unlocks the lock
}

void MutationBatcher::OnBulkApplyAttemptFinished(
    CompletionQueue& cq, MutationBatcher::Batch& batch) {
  if (batch.attempt_finished) {
    // We consider a batch finished if the original request finished. If it is
    // later retried, we don't count it against the limit. The reasoning is that
    // it would usually be some long tail of mutations and it should not take up
    // the resources for the incoming requests.
    return;
  }
  batch.attempt_finished = true;
  std::unique_lock<std::mutex> lk(mu_);
  num_outstanding_batches_ -= 1;
  FlushIfPossible(cq);
  SatisfyPromises(TryAdmit(cq), lk);  // unlocks the lock
}

std::vector<MutationBatcher::AdmissionPromise> MutationBatcher::TryAdmit(
    CompletionQueue& cq) {
  // Defer satisfying promises until we release the lock.
  std::vector<AdmissionPromise> admission_promises;

  do {
    while (!pending_mutations_.empty() &&
           HasSpaceFor(pending_mutations_.front())) {
      auto& mut(pending_mutations_.front());
      admission_promises.emplace_back(std::move(mut.admission_promise));
      Admit(std::move(mut));
      pending_mutations_.pop();
    }
  } while (FlushIfPossible(cq));
  return admission_promises;
}

void MutationBatcher::Admit(PendingSingleRowMutation mut) {
  outstanding_size_ += mut.request_size;
  cur_batch_->requests_size += mut.request_size;
  cur_batch_->num_mutations += mut.num_mutations;
  cur_batch_->requests.emplace_back(std::move(mut.mut));
  cur_batch_->mutation_data.emplace(cur_batch_->last_idx++,
                                    Batch::MutationData(std::move(mut)));
}

void MutationBatcher::SatisfyPromises(
    std::vector<AdmissionPromise> admission_promises,
    std::unique_lock<std::mutex>& lk) {
  std::vector<NoMorePendingPromise> no_more_pending_promises;
  if (num_requests_pending_ == 0 && num_outstanding_batches_ == 0) {
    // We should wait not only on num_requests_pending_ being zero but also on
    // num_outstanding_batches_ because we want to allow the user to kill the
    // completion queue after this promise is fulfilled. Otherwise, the user can
    // destroy the completion queue while the last batch is still being
    // processed - we've had this bug (#2140).
    no_more_pending_promises_.swap(no_more_pending_promises);
  }
  lk.unlock();

  // Inform the user that we've admitted these mutations and there might be some
  // space in the buffer finally.
  for (auto& promise : admission_promises) {
    promise.set_value();
  }
  for (auto& promise : no_more_pending_promises) {
    promise.set_value();
  }
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
