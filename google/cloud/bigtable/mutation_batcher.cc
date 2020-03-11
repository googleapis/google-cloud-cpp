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
#include "google/cloud/grpc_error_delegate.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

// Cloud Bigtable doesn't accept more than this.
auto constexpr kBigtableMutationLimit = 100000;
// Let's make the default slightly smaller, so that overheads or
// miscalculations don't tip us over.
auto constexpr kDefaultMaxSizePerBatch =
    (BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH * 90LL) / 100LL;
auto constexpr kDefaultMaxBatches = 8;
auto constexpr kDefaultMaxOutstandingSize =
    kDefaultMaxSizePerBatch * kDefaultMaxBatches;

MutationBatcher::Options::Options()
    : max_mutations_per_batch(kBigtableMutationLimit),
      max_size_per_batch(kDefaultMaxSizePerBatch),
      max_batches(kDefaultMaxBatches),
      max_outstanding_size(kDefaultMaxOutstandingSize) {}

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
        MakeStatusFromRpcError(mutation_status));
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
    return make_ready_future();
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
  num_mutations = static_cast<std::size_t>(tmp.mutations_size());
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

bool MutationBatcher::FlushIfPossible(CompletionQueue cq) {
  if (cur_batch_->num_mutations > 0 &&
      num_outstanding_batches_ < options_.max_batches) {
    ++num_outstanding_batches_;

    auto batch = std::make_shared<Batch>();
    cur_batch_.swap(batch);
    table_.AsyncBulkApply(std::move(batch->requests), cq)
        .then([this, cq,
               batch](future<std::vector<FailedMutation>> failed) mutable {
          OnBulkApplyDone(std::move(cq), std::move(*batch), failed.get());
        });
    return true;
  }
  return false;
}

void MutationBatcher::OnBulkApplyDone(
    CompletionQueue cq, MutationBatcher::Batch batch,
    std::vector<FailedMutation> const& failed) {
  // First process all the failures, marking the mutations as done after
  // processing them.
  for (auto const& f : failed) {
    int const idx = f.original_index();
    if (idx < 0 ||
        static_cast<std::size_t>(idx) >= batch.mutation_data.size()) {
      // This is a bug on the server or the client, either terminate (when
      // -fno-exceptions is set) or throw an exception.
      std::ostringstream os;
      os << "Index " << idx << " is out of range [0,"
         << batch.mutation_data.size() << ")";
      google::cloud::internal::ThrowRuntimeError(std::move(os).str());
    }
    MutationData& data = batch.mutation_data[idx];
    data.completion_promise.set_value(f.status());
    data.done = true;
  }
  // Any remaining mutations are treated as successful.
  for (auto& data : batch.mutation_data) {
    if (!data.done) {
      data.completion_promise.set_value(Status());
      data.done = true;
    }
  }
  auto const num_mutations = batch.mutation_data.size();
  batch.mutation_data.clear();

  std::unique_lock<std::mutex> lk(mu_);
  outstanding_size_ -= batch.requests_size;
  num_requests_pending_ -= num_mutations;
  num_outstanding_batches_--;
  SatisfyPromises(TryAdmit(cq), lk);  // unlocks the lock
}

std::vector<MutationBatcher::AdmissionPromise> MutationBatcher::TryAdmit(
    CompletionQueue& cq) {
  // Defer satisfying promises until we release the lock.
  std::vector<AdmissionPromise> admission_promises;

  do {
    while (!pending_mutations_.empty() &&
           HasSpaceFor(pending_mutations_.front())) {
      auto& mut = pending_mutations_.front();
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
  cur_batch_->mutation_data.emplace_back(MutationData(std::move(mut)));
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
