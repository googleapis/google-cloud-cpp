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

#include "google/cloud/bigtable/internal/mutation_batcher.h"
#include "google/cloud/bigtable/internal/table.h"
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

MutationBatcher::Options::Options(size_t max_mutations_per_batch,
                                  size_t max_size_per_batch, size_t max_batches,
                                  size_t max_oustanding_size)
    : max_mutations_per_batch(max_mutations_per_batch),
      max_size_per_batch(max_size_per_batch),
      max_batches(max_batches),
      max_oustanding_size(max_oustanding_size) {}

MutationBatcher::Options& MutationBatcher::Options::SetMaxMutationsPerBatch(
    size_t max_mutations_per_batch_arg) {
  max_mutations_per_batch = max_mutations_per_batch_arg;
  return *this;
}

MutationBatcher::Options& MutationBatcher::Options::SetMaxSizePerBatch(
    size_t max_size_per_batch_arg) {
  max_size_per_batch = max_size_per_batch_arg;
  return *this;
}

MutationBatcher::Options& MutationBatcher::Options::SetMaxBatches(
    size_t max_batches_arg) {
  max_batches = max_batches_arg;
  return *this;
}

MutationBatcher::Options& MutationBatcher::Options::SetMaxOustandingSize(
    size_t max_oustanding_size_arg) {
  max_oustanding_size = max_oustanding_size_arg;
  return *this;
}

MutationBatcher::MutationBatcher(noex::Table& table, Options options)
    : table_(table),
      options_(options),
      num_outstanding_batches_(),
      oustanding_size_(),
      cur_batch_(cloud::internal::make_unique<Batch>()) {}

std::shared_ptr<AsyncOperation> MutationBatcher::AsyncApply(
    CompletionQueue& cq, AsyncApplyCompletionCallback&& completion_callback,
    AsyncApplyAdmissionCallback&& admission_callback, SingleRowMutation&& mut) {
  PendingSingleRowMutation pending(std::move(mut),
                                   std::move(completion_callback),
                                   std::move(admission_callback));
  auto res = std::make_shared<BatchedSingleRowMutation>();
  std::unique_lock<std::mutex> lk(mu_);

  grpc::Status mutation_status = IsValid(pending);
  if (!mutation_status.ok()) {
    lk.unlock();
    {
      // Destroy the mutation before calling the admission callback so that we
      // can limit the memory usage.
      google::bigtable::v2::MutateRowsRequest::Entry entry;
      pending.mut.MoveTo(&entry);
    }
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
  cur_batch_->Add(std::move(pending));
  FlushIfPossible(cq);

  lk.unlock();

  admission_callback_to_fire(cq);
  return res;
}

MutationBatcher::PendingSingleRowMutation::PendingSingleRowMutation(
    SingleRowMutation&& mut_arg,
    AsyncApplyCompletionCallback&& completion_callback,
    AsyncApplyAdmissionCallback&& admission_callback)
    : mut(mut_arg),
      completion_callback(completion_callback),
      admission_callback(admission_callback) {
  ::google::bigtable::v2::MutateRowsRequest::Entry tmp;
  mut.MoveTo(&tmp);
  // This operation might not be cheap, so let's cache it.
  request_size = tmp.ByteSizeLong();
  num_mutations = tmp.mutations_size();
  mut = SingleRowMutation(std::move(tmp));
}

void MutationBatcher::Batch::Add(PendingSingleRowMutation&& mut) {
  requests_size_ += mut.request_size;
  num_mutations_ += mut.num_mutations;
  requests_.emplace_back(std::move(mut.mut));
  callbacks_.emplace_back(std::move(mut.completion_callback));
}

void MutationBatcher::Batch::FireCallbacks(
    CompletionQueue& cq, std::vector<FailedMutation> const& failed) {
  std::unordered_set<int> failed_indices;
  for (auto const& f : failed) {
    int const idx = f.original_index();
    failed_indices.insert(idx);
    // For some reason clang-tidy thinks that callbacks_[idx] would be fine with
    // a const reference to status.
    // NOLINTNEXTLINE (performance-unnecessary-copy-initialization)
    grpc::Status status(f.status());
    callbacks_[idx](cq, status);
  }
  for (size_t i = 0; i < callbacks_.size(); ++i) {
    if (failed_indices.find(i) != failed_indices.end()) {
      continue;
    }
    grpc::Status status;
    callbacks_[i](cq, status);
  }
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

class BatchFinishedCallback {
 public:
  BatchFinishedCallback(MutationBatcher& parent,
                        std::unique_ptr<MutationBatcher::Batch> batch)
      : parent_(parent), batch_(std::move(batch)) {}

  void operator()(CompletionQueue& cq, std::vector<FailedMutation>& failed,
                  grpc::Status&) {
    // status is ignored - it's basically an logical AND on all mutations'
    // statuses.
    parent_.BatchFinished(cq, std::move(batch_), failed);
  }

 private:
  MutationBatcher& parent_;
  std::unique_ptr<MutationBatcher::Batch> batch_;
};

void MutationBatcher::FlushIfPossible(CompletionQueue& cq) {
  if (cur_batch_->num_mutations() > 0 &&
      num_outstanding_batches_ < options_.max_batches) {
    oustanding_size_ += cur_batch_->requests_size();
    ++num_outstanding_batches_;
    auto req = cur_batch_->TransferRequest();
    table_.AsyncBulkApply(cq,
                          BatchFinishedCallback(*this, std::move(cur_batch_)),
                          std::move(req));
    cur_batch_ = cloud::internal::make_unique<Batch>();
  }
}

void MutationBatcher::BatchFinished(
    CompletionQueue& cq, std::unique_ptr<MutationBatcher::Batch> batch,
    std::vector<FailedMutation> const& failed) {
  batch->FireCallbacks(cq, failed);

  std::unique_lock<std::mutex> lk(mu_);

  oustanding_size_ -= batch->requests_size();
  num_outstanding_batches_ -= 1;

  // We want to release as much resources as we can because the next thing we're
  // going to do is flush some mutations and admit more.
  batch.reset();

  FlushIfPossible(cq);

  // Defer callbacks until we release the lock
  std::vector<AsyncApplyAdmissionCallback> admission_callbacks;

  while (!pending_mutations_.empty() &&
         HasSpaceFor(pending_mutations_.front())) {
    PendingSingleRowMutation& mut(pending_mutations_.front());
    admission_callbacks.push_back(std::move(mut.admission_callback));
    cur_batch_->Add(std::move(mut));
    pending_mutations_.pop();
  }
  // There is no reason to call FlushIfPossible() here.
  // If there weren't any mutations waiting pending_mutations_, then there is
  // nothing to flush.
  // If there were mutation on pending_mutations_, it means that there were some
  // mutations already in cur_batch_ and cur_batch wasn't flushed. If it wasn't
  // flushed, it meant that there were no free slots. This function has just
  // released one slot and taken it again by running FlushIfPossible() so there
  // are no free slots again.

  lk.unlock();

  // Inform the user that we've admitted these mutations and there might be some
  // space in the buffer finally.
  for (auto& cb : admission_callbacks) {
    grpc::Status status;
    cb(cq);
  }
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
