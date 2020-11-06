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
  using ValueType = StatusOr<std::chrono::system_clock::time_point>;

  // We need to create the shared_ptr before completing the initialization, so
  // use a factory member function.
  static std::pair<std::shared_ptr<AsyncTimerFuture>, future<ValueType>>
  Create() {
    auto self = std::shared_ptr<AsyncTimerFuture>(new AsyncTimerFuture);
    auto weak = std::weak_ptr<AsyncTimerFuture>(self);
    self->promise_ = promise<AsyncTimerFuture::ValueType>([weak] {
      if (auto self = weak.lock()) self->Cancel();
    });
    return {self, self->promise_.get_future()};
  }

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    deadline_ = deadline;
    alarm_.Set(&cq, deadline, tag);
  }

  void Cancel() override { alarm_.Cancel(); }

 private:
  explicit AsyncTimerFuture() : promise_(null_promise_t{}) {}

  bool Notify(bool ok) override {
    promise_.set_value(ok ? ValueType(deadline_) : Canceled());
    return true;
  }

  static ValueType Canceled() {
    return Status{StatusCode::kCancelled, "timer canceled"};
  }

  promise<ValueType> promise_;
  std::chrono::system_clock::time_point deadline_;
  grpc::Alarm alarm_;
};

}  // namespace

// A helper class to wake up the asynchronous thread and drain the RunAsync()
// queue in a loop.
class DefaultCompletionQueueImpl::WakeUpRunAsyncLoop
    : public internal::AsyncGrpcOperation {
 public:
  explicit WakeUpRunAsyncLoop(std::weak_ptr<DefaultCompletionQueueImpl> w)
      : weak_(std::move(w)) {}

  void Set(grpc::CompletionQueue& cq, void* tag) {
    alarm_.Set(&cq, std::chrono::system_clock::now(), tag);
  }

  void Cancel() override {}

 private:
  bool Notify(bool ok) override {
    if (!ok) return true;  // do not run async operations on shutdown CQs
    if (auto self = weak_.lock()) self->DrainRunAsyncLoop();
    return true;
  }

  std::weak_ptr<DefaultCompletionQueueImpl> weak_;
  grpc::Alarm alarm_;
};

// A helper class to wake up the asynchronous thread and drain the RunAsync()
// one element at a time.
class DefaultCompletionQueueImpl::WakeUpRunAsyncOnIdle
    : public internal::AsyncGrpcOperation {
 public:
  explicit WakeUpRunAsyncOnIdle(std::weak_ptr<DefaultCompletionQueueImpl> w)
      : weak_(std::move(w)) {}

  void Set(grpc::CompletionQueue& cq, void* tag) {
    alarm_.Set(&cq, std::chrono::system_clock::now(), tag);
  }

  void Cancel() override {}

 private:
  bool Notify(bool ok) override {
    if (!ok) return true;  // do not run async operations on shutdown CQs
    if (auto self = weak_.lock()) self->DrainRunAsyncOnIdle();
    return true;
  }

  std::weak_ptr<DefaultCompletionQueueImpl> weak_;
  grpc::Alarm alarm_;
};

void DefaultCompletionQueueImpl::Run() {
  class ThreadPoolCount {
   public:
    explicit ThreadPoolCount(DefaultCompletionQueueImpl* self) : self_(self) {
      self_->RunStart();
    }
    ~ThreadPoolCount() { self_->RunStop(); }

   private:
    DefaultCompletionQueueImpl* self_;
  } count(this);

  auto deadline = [] {
    return std::chrono::system_clock::now() + kLoopTimeout;
  };

  void* tag;
  bool ok;
  for (auto status = cq_.AsyncNext(&tag, &ok, deadline());
       status != grpc::CompletionQueue::SHUTDOWN;
       status = cq_.AsyncNext(&tag, &ok, deadline())) {
    if (status == grpc::CompletionQueue::TIMEOUT) continue;
    if (status != grpc::CompletionQueue::GOT_EVENT) {
      google::cloud::internal::ThrowRuntimeError(
          "unexpected status from AsyncNext()");
    }
    auto op = FindOperation(tag);
    ++notify_counter_;
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
  auto p = AsyncTimerFuture::Create();
  auto op = std::move(p.first);
  StartOperation(op, [&](void* tag) { op->Set(cq(), deadline, tag); });
  return std::move(p.second);
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
  std::unique_lock<std::mutex> lk(mu_);
  run_async_queue_.push_back(std::move(function));
  WakeUpRunAsyncThread(std::move(lk));
}

void DefaultCompletionQueueImpl::StartOperation(
    std::shared_ptr<AsyncGrpcOperation> op,
    absl::FunctionRef<void(void*)> start) {
  StartOperation(std::unique_lock<std::mutex>(mu_), std::move(op),
                 std::move(start));
}

grpc::CompletionQueue& DefaultCompletionQueueImpl::cq() { return cq_; }

void DefaultCompletionQueueImpl::StartOperation(
    std::unique_lock<std::mutex> lk, std::shared_ptr<AsyncGrpcOperation> op,
    absl::FunctionRef<void(void*)> start) {
  void* tag = op.get();
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

void DefaultCompletionQueueImpl::DrainRunAsyncLoop() {
  std::unique_lock<std::mutex> lk(mu_);
  while (!run_async_queue_.empty() && !shutdown_) {
    auto f = std::move(run_async_queue_.front());
    run_async_queue_.pop_front();
    lk.unlock();
    f->exec();
    lk.lock();
  }
  --run_async_pool_size_;
}

void DefaultCompletionQueueImpl::DrainRunAsyncOnIdle() {
  std::unique_lock<std::mutex> lk(mu_);
  if (run_async_queue_.empty()) return;
  auto f = std::move(run_async_queue_.front());
  run_async_queue_.pop_front();
  lk.unlock();
  f->exec();
  lk.lock();
  if (run_async_queue_.empty()) {
    --run_async_pool_size_;
    return;
  }
  auto op = std::make_shared<WakeUpRunAsyncOnIdle>(shared_from_this());
  StartOperation(std::move(lk), op, [&](void* tag) { op->Set(cq(), tag); });
}

void DefaultCompletionQueueImpl::WakeUpRunAsyncThread(
    std::unique_lock<std::mutex> lk) {
  if (run_async_queue_.empty() || shutdown_) return;
  if (thread_pool_size_ <= 1) {
    if (run_async_pool_size_ > 0) return;
    ++run_async_pool_size_;
    run_async_pool_hwm_ = (std::max)(run_async_pool_hwm_, run_async_pool_size_);
    auto op = std::make_shared<WakeUpRunAsyncOnIdle>(shared_from_this());
    StartOperation(std::move(lk), op, [&](void* tag) { op->Set(cq(), tag); });
    return;
  }
  // Always leave one thread for I/O
  if (run_async_pool_size_ >= thread_pool_size_ - 1) return;
  auto op = std::make_shared<WakeUpRunAsyncLoop>(shared_from_this());
  ++run_async_pool_size_;
  run_async_pool_hwm_ = (std::max)(run_async_pool_hwm_, run_async_pool_size_);
  StartOperation(std::move(lk), op, [&](void* tag) { op->Set(cq(), tag); });
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
