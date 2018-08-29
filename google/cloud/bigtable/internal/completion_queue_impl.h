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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_

#include "google/cloud/bigtable/async_operation.h"
#include <grpcpp/alarm.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/async_unary_call.h>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The result of an async timer operation.
 *
 * Callbacks for async timers will receive an object of this class.
 */
struct AsyncTimerResult {
  std::chrono::system_clock::time_point deadline;
};

namespace internal {
class CompletionQueueImpl;

/**
 * Wrap a timer callback into a `AsyncOperation`.
 *
 * Applications (or more likely other components in the client library) will
 * associate callbacks of many different types with a completion queue. This
 * class is created by the completion queue implementation to type-erase the
 * callbacks, and thus be able to treat them homogenously in the completion
 * queue. Note that this class lives in the `internal` namespace and thus is
 * not intended for general use.
 *
 * @tparam Functor the callback type.
 */
template <typename Functor>
class AsyncTimerFunctor : public AsyncOperation {
 public:
  explicit AsyncTimerFunctor(Functor&& functor)
      : functor_(std::move(functor)), alarm_(new grpc::Alarm) {}

  void Notify(bool ok) override {
    alarm_.reset();
    functor_(timer_, ok);
  }

  void Set(grpc::CompletionQueue& cq,
           std::chrono::system_clock::time_point deadline, void* tag) {
    timer_.deadline = deadline;
    alarm_->Set(&cq, deadline, tag);
  }

  void Cancel() override {
    if (alarm_) {
      alarm_->Cancel();
    }
  }

 private:
  Functor functor_;
  AsyncTimerResult timer_;
  std::unique_ptr<grpc::Alarm> alarm_;
};

/**
 * The implementation details for `CompletionQueue`.
 *
 * `CompletionQueue` is implemented using the PImpl idiom:
 *     https://en.wikipedia.org/wiki/Opaque_pointer
 * This is the implementation class in that idiom.
 */
class CompletionQueueImpl {
 public:
  CompletionQueueImpl() : cq_(), shutdown_(false) {}
  virtual ~CompletionQueueImpl() = default;

  void Run();
  void Shutdown();

  grpc::CompletionQueue& cq() { return cq_; }

  /// Add a new asynchronous operation to the completion queue.
  void* RegisterOperation(std::shared_ptr<AsyncOperation> op);

  /// Return the asynchronous operation associated with @p tag.
  std::shared_ptr<AsyncOperation> CompletedOperation(void* tag);

 protected:
  /// Simulate a completed operation, provided only to support unit tests.
  void SimulateCompletion(AsyncOperation* op, bool ok);

 private:
  grpc::CompletionQueue cq_;
  std::atomic<bool> shutdown_;
  std::mutex mu_;
  std::unordered_map<std::intptr_t, std::shared_ptr<AsyncOperation>>
      pending_ops_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_
