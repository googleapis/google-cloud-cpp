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

#include "google/cloud/bigtable/internal/mutation_batcher.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/bigtable/internal/table.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

MutationBatcher::Options::Options()
    :  // Cloud Bigtable doesn't accept more than this.
      max_mutations_per_batch(100000),
      // Let's make the default slightly smaller, so that overheads or
      // miscalculations don't tip us over.
      max_size_per_batch(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH * 9 / 10),
      max_batches(8),
      max_oustanding_size(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH * 6) {}

std::shared_ptr<AsyncOperation> MutationBatcher::AsyncApply(
    CompletionQueue& cq, AsyncApplyCompletionCallback completion_callback,
    AsyncApplyAdmissionCallback admission_callback, SingleRowMutation mut) {
  PendingSingleRowMutation pending(std::move(mut),
                                   std::move(completion_callback),
                                   std::move(admission_callback));
  auto res = std::make_shared<BatchedSingleRowMutation>();
  std::unique_lock<std::mutex> lk(mu_);

  grpc::Status mutation_status = IsValid(pending);
  if (!mutation_status.ok()) {
    lk.unlock();
    // Destroy the mutation before calling the admission callback so that we can
    // limit the memory usage.
    pending.mut.Clear();
    pending.completion_callback(cq, mutation_status);
    pending.completion_callback = AsyncApplyCompletionCallback();
    pending.admission_callback(cq);
    return res;
  }

  if (!CanAppendToBatch(pending)) {
    pending_mutations_.push(std::move(pending));
    return res;
  }
  AsyncApplyAdmissionCallback admission_callback_to_fire(
      std::move(pending.admission_callback));
  Admit(std::move(pending));
  FlushIfPossible(cq);

  lk.unlock();

  admission_callback_to_fire(cq);
  return res;
}

MutationBatcher::PendingSingleRowMutation::PendingSingleRowMutation(
    SingleRowMutation mut_arg, AsyncApplyCompletionCallback completion_callback,
    AsyncApplyAdmissionCallback admission_callback)
    : mut(std::move(mut_arg)),
      completion_callback(std::move(completion_callback)),
      admission_callback(std::move(admission_callback)) {
  ::google::bigtable::v2::MutateRowsRequest::Entry tmp;
  mut.MoveTo(&tmp);
  // This operation might not be cheap, so let's cache it.
  request_size = tmp.ByteSizeLong();
  num_mutations = tmp.mutations_size();
  mut = SingleRowMutation(std::move(tmp));
}

void MutationBatcher::Batch::Add(PendingSingleRowMutation mut) {
  std::unique_lock<std::mutex> lk(mu_);
  requests_size_ += mut.request_size;
  num_mutations_ += mut.num_mutations;
  requests_.emplace_back(std::move(mut.mut));
  mutation_data_.emplace(last_idx_++, MutationData(std::move(mut)));
}

size_t MutationBatcher::Batch::FireFailedCallbacks(
    CompletionQueue& cq, std::vector<FailedMutation> failed) {
  std::unique_lock<std::mutex> lk(mu_);

  std::vector<MutationData> to_fire;
  size_t completed_size = 0;

  for (auto const& f : failed) {
    int const idx = f.original_index();
    auto it = mutation_data_.find(idx);
    completed_size += it->second.request_size;
    to_fire.emplace_back(std::move(it->second));
    // Release resources as early as possible.
    mutation_data_.erase(it);
  }
  lk.unlock();
  int idx = 0;
  for (auto& f : failed) {
    // For some reason clang-tidy thinks that AsyncApplyCompletionCallback would
    // be fine with a const reference to status.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    grpc::Status status(f.status());
    to_fire[idx++].callback(cq, status);
  }
  return completed_size;
}

size_t MutationBatcher::Batch::FireSuccessfulCallbacks(
    CompletionQueue& cq, std::vector<int> indices) {
  std::unique_lock<std::mutex> lk(mu_);

  std::vector<MutationData> to_fire;
  size_t completed_size = 0;

  for (int idx : indices) {
    auto it = mutation_data_.find(idx);
    completed_size += it->second.request_size;
    to_fire.emplace_back(std::move(it->second));
    // Release resources as early as possible.
    mutation_data_.erase(it);
  }
  lk.unlock();
  for (auto& data : to_fire) {
    grpc::Status status;
    data.callback(cq, status);
  }
  return completed_size;
}

bool MutationBatcher::Batch::AttemptFinished() {
  std::unique_lock<std::mutex> lk(mu_);
  bool was_first = !attempt_finished_;
  attempt_finished_ = true;
  return was_first;
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
  return oustanding_size_ + mut.request_size <= options_.max_oustanding_size &&
         cur_batch_->requests_size() + mut.request_size <=
             options_.max_size_per_batch &&
         cur_batch_->num_mutations() + mut.num_mutations <=
             options_.max_mutations_per_batch;
}

bool MutationBatcher::FlushIfPossible(CompletionQueue& cq) {
  if (cur_batch_->num_mutations() > 0 &&
      num_outstanding_batches_ < options_.max_batches) {
    ++num_outstanding_batches_;
    auto batch = cur_batch_;
    table_.StreamingAsyncBulkApply(
        cq,
        [this, batch](CompletionQueue& cq, std::vector<int> succeeded) {
          MutationsSucceeded(cq, *batch, std::move(succeeded));
        },
        [this, batch](CompletionQueue& cq, std::vector<FailedMutation> failed) {
          MutationsFailed(cq, *batch, std::move(failed));
        },
        [this, batch](CompletionQueue& cq, grpc::Status&) {
          BatchAttemptFinished(cq, *batch);
        },
        [this, batch](CompletionQueue& cq, std::vector<FailedMutation>& failed,
                      grpc::Status&) {
          // It means that there are not going to be anymore retries and the
          // final failed mutations are passed here.
          MutationsFailed(cq, *batch, failed);
        },
        cur_batch_->TransferRequest());
    cur_batch_ = std::make_shared<Batch>();
    return true;
  }
  return false;
}

void MutationBatcher::MutationsSucceeded(CompletionQueue& cq,
                                         MutationBatcher::Batch& batch,
                                         std::vector<int> indices) {
  size_t completed_size = batch.FireSuccessfulCallbacks(cq, std::move(indices));

  std::unique_lock<std::mutex> lk(mu_);
  oustanding_size_ -= completed_size;
  TryAdmit(cq, lk);  // unlocks the lock
}

void MutationBatcher::MutationsFailed(CompletionQueue& cq,
                                      MutationBatcher::Batch& batch,
                                      std::vector<FailedMutation> failed) {
  size_t completed_size = batch.FireFailedCallbacks(cq, std::move(failed));

  std::unique_lock<std::mutex> lk(mu_);
  oustanding_size_ -= completed_size;
  TryAdmit(cq, lk);  // unlocks the lock
}

void MutationBatcher::BatchAttemptFinished(CompletionQueue& cq,
                                           MutationBatcher::Batch& batch) {
  bool was_first_attempt = batch.AttemptFinished();
  if (!was_first_attempt) {
    return;
  }
  std::unique_lock<std::mutex> lk(mu_);
  // We consider a batch finished if the original request finished. If it is
  // later retried, we don't count it against the limit. The reasoning is that
  // it would usually be some long tail of mutations and it should not take up
  // the resources for the incoming requests.
  num_outstanding_batches_ -= 1;
  FlushIfPossible(cq);
  TryAdmit(cq, lk);
}

void MutationBatcher::TryAdmit(CompletionQueue& cq,
                               std::unique_lock<std::mutex>& lk) {
  // Defer callbacks until we release the lock
  std::vector<AsyncApplyAdmissionCallback> admission_callbacks;

  do {
    while (!pending_mutations_.empty() &&
           HasSpaceFor(pending_mutations_.front())) {
      auto& mut(pending_mutations_.front());
      admission_callbacks.emplace_back(std::move(mut.admission_callback));
      Admit(std::move(mut));
      pending_mutations_.pop();
    }
  } while (FlushIfPossible(cq));

  lk.unlock();

  // Inform the user that we've admitted these mutations and there might be
  // some space in the buffer finally.
  for (auto& cb : admission_callbacks) {
    grpc::Status status;
    cb(cq);
  }
}

void MutationBatcher::Admit(PendingSingleRowMutation mut) {
  oustanding_size_ += mut.request_size;
  cur_batch_->Add(std::move(mut));
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
