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

#include "google/cloud/internal/default_completion_queue_impl.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/memory/memory.h"
#include <grpcpp/alarm.h>
#include <sstream>

// There is no way to unblock the gRPC event loop, not even calling Shutdown(),
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

void DefaultCompletionQueueImpl::Run() {
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

void DefaultCompletionQueueImpl::Shutdown() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    shutdown_ = true;
  }
  cq_.Shutdown();
}

void DefaultCompletionQueueImpl::CancelAll() {
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
DefaultCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  auto op =
      std::make_shared<AsyncTimerFuture>(absl::make_unique<grpc::Alarm>());
  StartOperation(op, [&](void* tag) { op->Set(cq(), deadline, tag); });
  return op->GetFuture();
}

future<StatusOr<std::chrono::system_clock::time_point>>
DefaultCompletionQueueImpl::MakeRelativeTimer(
    std::chrono::nanoseconds duration) {
  using std::chrono::system_clock;
  auto const d = std::chrono::duration_cast<system_clock::duration>(duration);
  return MakeDeadlineTimer(system_clock::now() + d);
}

void DefaultCompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  auto op = std::make_shared<AsyncFunction>(std::move(function));
  StartOperation(op, [&](void* tag) { op->Set(cq(), tag); });
}

void DefaultCompletionQueueImpl::StartOperation(
    std::shared_ptr<AsyncGrpcOperation> op,
    absl::FunctionRef<void(void*)> start) {
  void* tag = op.get();
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_) {
    lk.unlock();
    op->Notify(/*ok=*/false);
    return;
  }
  auto ins = pending_ops_.emplace(tag, std::move(op));
  if (ins.second) {
    start(tag);
    return;
  }
  std::ostringstream os;
  os << "assertion failure: duplicate operation tag (" << tag << "),"
     << " asynchronous operations should complete before they are rescheduled."
     << " This might be a bug in the library, please report it at"
     << " https://github.com/google-cloud-cpp/issues";
  google::cloud::internal::ThrowRuntimeError(std::move(os).str());
}

grpc::CompletionQueue& DefaultCompletionQueueImpl::cq() { return cq_; }

std::shared_ptr<AsyncGrpcOperation> DefaultCompletionQueueImpl::FindOperation(
    void* tag) {
  std::lock_guard<std::mutex> lk(mu_);
  auto loc = pending_ops_.find(tag);
  if (pending_ops_.end() == loc) {
    google::cloud::internal::ThrowRuntimeError(
        "assertion failure: searching for async op tag");
  }
  return loc->second;
}

void DefaultCompletionQueueImpl::ForgetOperation(void* tag) {
  std::lock_guard<std::mutex> lk(mu_);
  auto const num_erased = pending_ops_.erase(tag);
  if (num_erased != 1) {
    google::cloud::internal::ThrowRuntimeError(
        "assertion failure: searching for async op tag when trying to "
        "unregister");
  }
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
