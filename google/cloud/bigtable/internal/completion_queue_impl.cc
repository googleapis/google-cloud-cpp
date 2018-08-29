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
void CompletionQueueImpl::Run() {
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
      google::cloud::internal::RaiseRuntimeError(
          "unexpected status from AsyncNext()");
    }
    auto op = CompletedOperation(tag);
    op->Notify(ok);
  }
}

void CompletionQueueImpl::Shutdown() {
  shutdown_.store(true);
  cq_.Shutdown();
}

void* CompletionQueueImpl::RegisterOperation(
    std::shared_ptr<AsyncOperation> op) {
  void* tag = op.get();
  std::lock_guard<std::mutex> lk(mu_);
  auto ins =
      pending_ops_.emplace(reinterpret_cast<std::intptr_t>(tag), std::move(op));
  if (ins.second) {
    return tag;
  }
  google::cloud::internal::RaiseRuntimeError(
      "assertion failure: insertion should succeed");
}

std::shared_ptr<AsyncOperation> CompletionQueueImpl::CompletedOperation(
    void* tag) {
  std::lock_guard<std::mutex> lk(mu_);
  auto loc = pending_ops_.find(reinterpret_cast<std::intptr_t>(tag));
  if (pending_ops_.end() == loc) {
    google::cloud::internal::RaiseRuntimeError(
        "assertion failure: searching for async op tag");
  }
  auto op = std::move(loc->second);
  pending_ops_.erase(loc);
  return op;
}

// This function is used in unit tests to simulate the completion of an
// operation, the unit test is expected to
void CompletionQueueImpl::SimulateCompletion(AsyncOperation* op, bool ok) {
  auto internal_op = CompletedOperation(op);
  internal_op->Notify(ok);
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
