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

#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/memory/memory.h"
#include <sstream>

// There is no wait to unblock the gRPC event loop, not even calling Shutdown(),
// so we periodically wake up from the loop to check if the application has
// shutdown the run.
std::chrono::milliseconds constexpr kLoopTimeout(50);

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

/**
 * Wrap a gRPC timer into an `AsyncOperation`.
 *
 * Applications (or more likely, other components in the client library) will
 * associate timers with a completion queue. gRPC timers require applications
 * to create a unique `grpc::Alarm` object for each timer, and then to
 * associate them with the completion queue using a `void*` tag.
 *
 * This class collaborates with our wrapper for `CompletionQueue` to associate
 * a `future<AsyncTimerResult>` for each timer. This class takes care of
 * allocating the `grpc::Alarm`, creating a unique `void*` associated with the
 * timer, and satisfying the future when the timer expires.
 *
 * Note that this class is an implementation detail, hidden from the
 * application developers.
 */
class AsyncTimerFuture : public internal::AsyncGrpcOperation {
 public:
  explicit AsyncTimerFuture(std::unique_ptr<grpc::Alarm> alarm)
      : promise_(/*cancellation_callback=*/[this] { Cancel(); }),
        alarm_(std::move(alarm)) {}

  future<StatusOr<std::chrono::system_clock::time_point>> GetFuture() {
    return promise_.get_future();
  }

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    deadline_ = deadline;

    if (alarm_) {
      alarm_->Set(&cq, deadline, tag);
    }
  }

  void Cancel() override {
    auto a = std::move(alarm_);
    if (a) a->Cancel();
  }

 private:
  bool Notify(bool ok) override {
    if (!ok) {
      promise_.set_value(Status(StatusCode::kCancelled, "timer canceled"));
    } else {
      promise_.set_value(deadline_);
    }
    return true;
  }

  promise<StatusOr<std::chrono::system_clock::time_point>> promise_;
  std::chrono::system_clock::time_point deadline_;
  /// Holds the underlying handle. It might be a nullptr in tests.
  std::unique_ptr<grpc::Alarm> alarm_;
};

class AsyncFunction : public internal::AsyncGrpcOperation {
 public:
  explicit AsyncFunction(std::unique_ptr<internal::RunAsyncBase> fun)
      : fun_(std::move(fun)), alarm_(absl::make_unique<grpc::Alarm>()) {}

  void Set(grpc::CompletionQueue& cq, void* tag) {
    alarm_->Set(&cq, std::chrono::system_clock::now(), tag);
  }

  void Cancel() override {}

 private:
  bool Notify(bool ok) override {
    auto f = std::move(fun_);
    alarm_.reset();
    if (!ok) return true;  // do not run async operations on shutdown CQs
    f->exec();
    return true;
  }

  std::unique_ptr<internal::RunAsyncBase> fun_;
  std::unique_ptr<grpc::Alarm> alarm_;
};

}  // namespace

void CompletionQueueImpl::Run() {
  void* tag;
  bool ok;
  auto deadline = [] {
    return std::chrono::system_clock::now() + kLoopTimeout;
  };

  for (auto status = cq_.AsyncNext(&tag, &ok, deadline());
       status != grpc::CompletionQueue::SHUTDOWN;
       status = cq_.AsyncNext(&tag, &ok, deadline())) {
    if (status == grpc::CompletionQueue::TIMEOUT) continue;
    if (status != grpc::CompletionQueue::GOT_EVENT) {
      google::cloud::internal::ThrowRuntimeError(
          "unexpected status from AsyncNext()");
    }
    auto op = FindOperation(tag);
    if (op->Notify(ok)) {
      ForgetOperation(tag);
    }
  }
}

void CompletionQueueImpl::Shutdown() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    shutdown_ = true;
  }
  cq_.Shutdown();
}

void CompletionQueueImpl::CancelAll() {
  // Cancel all operations. We need to make a copy of the operations because
  // canceling them may trigger a recursive call that needs the lock. And we
  // need the lock because canceling might trigger calls that invalidate the
  // iterators.
  auto pending = [this] {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_;
  }();
  for (auto& kv : pending) {
    kv.second->Cancel();
  }
}

future<StatusOr<std::chrono::system_clock::time_point>>
CompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  auto op =
      std::make_shared<AsyncTimerFuture>(absl::make_unique<grpc::Alarm>());
  StartOperation(op, [&](void* tag) { op->Set(cq(), deadline, tag); });
  return op->GetFuture();
}

void CompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  auto op = std::make_shared<AsyncFunction>(std::move(function));
  StartOperation(op, [&](void* tag) { op->Set(cq(), tag); });
}

void CompletionQueueImpl::StartOperation(std::shared_ptr<AsyncGrpcOperation> op,
                                         absl::FunctionRef<void(void*)> start) {
  void* tag = op.get();
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) {
    lk.unlock();
    op->Notify(/*ok=*/false);
    return;
  }
  auto const itag = reinterpret_cast<std::intptr_t>(tag);
  auto ins = pending_ops_.emplace(itag, std::move(op));
  if (ins.second) {
    start(tag);
    lk.unlock();
    return;
  }
  std::ostringstream os;
  os << "assertion failure: duplicate operation tag (" << itag << "),"
     << "did you try to start the same asynchronous operation twice?";
  google::cloud::internal::ThrowRuntimeError(std::move(os).str());
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
  if (num_erased != 1) {
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
void CompletionQueueImpl::SimulateCompletion(AsyncOperation* op, bool ok) {
  auto internal_op = FindOperation(op);
  internal_op->Cancel();
  if (internal_op->Notify(ok)) {
    ForgetOperation(op);
  }
}

void CompletionQueueImpl::SimulateCompletion(bool ok) {
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
    if (internal_op->Notify(ok)) {
      ForgetOperation(tag);
    }
  }

  // Discard any pending events.
  grpc::CompletionQueue::NextStatus status;
  do {
    void* tag;
    bool async_next_ok;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(1);
    status = cq_.AsyncNext(&tag, &async_next_ok, deadline);
  } while (status == grpc::CompletionQueue::GOT_EVENT);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
