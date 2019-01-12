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

#include "google/cloud/bigtable/internal/completion_queue_impl.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/throw_delegate.h"

// There is no wait to unblock the gRPC event loop, not even calling Shutdown(),
// so we periodically wake up from the loop to check if the application has
// shutdown the run.
constexpr std::chrono::milliseconds LOOP_TIMEOUT(50);

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
void CompletionQueueImpl::Run(CompletionQueue& cq) {
  while (not shutdown_.load()) {
    void* tag;
    bool ok;
    auto deadline = std::chrono::system_clock::now() + LOOP_TIMEOUT;
    auto status = cq_.AsyncNext(&tag, &ok, deadline);
    if (status == grpc::CompletionQueue::SHUTDOWN) {
      break;
    }
    if (status == grpc::CompletionQueue::TIMEOUT) {
      continue;
    }
    if (status != grpc::CompletionQueue::GOT_EVENT) {
      google::cloud::internal::ThrowRuntimeError(
          "unexpected status from AsyncNext()");
    }
    auto op = FindOperation(tag);
    if (op->Notify(cq, ok)) {
      ForgetOperation(tag);
    }
  }
}

void CompletionQueueImpl::Shutdown() {
  shutdown_.store(true);
  cq_.Shutdown();
}

std::unique_ptr<grpc::Alarm> CompletionQueueImpl::CreateAlarm() const {
  return google::cloud::internal::make_unique<grpc::Alarm>();
}

void* CompletionQueueImpl::RegisterOperation(
    std::shared_ptr<AsyncGrpcOperation> op) {
  void* tag = op.get();
  std::unique_lock<std::mutex> lk(mu_);
  auto ins =
      pending_ops_.emplace(reinterpret_cast<std::intptr_t>(tag), std::move(op));
  // After this point we no longer need the lock, so release it.
  lk.unlock();
  if (ins.second) {
    return tag;
  }
  google::cloud::internal::ThrowRuntimeError(
      "assertion failure: insertion should succeed");
}

std::shared_ptr<AsyncGrpcOperation> CompletionQueueImpl::FindOperation(
    void* tag) {
  std::lock_guard<std::mutex> lk(mu_);
  auto loc = pending_ops_.find(reinterpret_cast<std::intptr_t>(tag));
  if (pending_ops_.end() == loc) {
    google::cloud::internal::ThrowRuntimeError(
        "assertion failure: searching for async op tag");
  }
  return loc->second;
}

void CompletionQueueImpl::ForgetOperation(void* tag) {
  std::lock_guard<std::mutex> lk(mu_);
  auto const num_erased =
      pending_ops_.erase(reinterpret_cast<std::intptr_t>(tag));
  if (1U != num_erased) {
    google::cloud::internal::ThrowRuntimeError(
        "assertion failure: searching for async op tag when trying to "
        "unregister");
  }
}

// This function is used in unit tests to simulate the completion of an
// operation. The unit test is expected to create a class derived from
// `CompletionQueueImpl`, wrap it in a `CompletionQueue` and call this function
// to simulate the operation lifecycle. Note that the unit test must simulate
// the operation results separately.
void CompletionQueueImpl::SimulateCompletion(CompletionQueue& cq,
                                             AsyncOperation* op, bool ok) {
  auto internal_op = FindOperation(op);
  internal_op->Cancel();
  if (internal_op->Notify(cq, ok)) {
    ForgetOperation(op);
  }
}

void CompletionQueueImpl::SimulateCompletion(CompletionQueue& cq, bool ok) {
  // Make a copy to avoid race conditions or iterator invalidation.
  std::vector<void*> tags;
  {
    std::lock_guard<std::mutex> lk(mu_);
    tags.reserve(pending_ops_.size());
    for (auto&& kv : pending_ops_) {
      tags.push_back(reinterpret_cast<void*>(kv.first));
    }
  }
  for (void* tag : tags) {
    auto internal_op = FindOperation(tag);
    internal_op->Cancel();
    if (internal_op->Notify(cq, ok)) {
      ForgetOperation(tag);
    }
  }

  // Discard any pending events.
  grpc::CompletionQueue::NextStatus status;
  do {
    void* tag;
    bool ok;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(1);
    status = cq_.AsyncNext(&tag, &ok, deadline);
  } while (status == grpc::CompletionQueue::GOT_EVENT);
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
