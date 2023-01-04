// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/rest_completion_queue.h"
#include "google/cloud/log.h"
#include "google/cloud/version.h"
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Represents an AsyncOperation which that uses Notify to indicate completion.
 *
 * When applications create an asynchronous operation with a `CompletionQueue`
 * they provide a callback to be invoked when the operation completes
 * (successfully or not). The completion queue type-erases the callback and
 * hides it in a class derived from `AsyncOperation`. A shared pointer to the
 * `AsyncOperation` is returned by the completion queue so library developers
 * can cancel the operation if needed.
 *
 * @note Sub-classes of `AsyncRestOperation` should snapshot the prevailing
 *     `Options` during construction, and restore them using an `OptionsSpan`
 *     during `Notify()` and `Cancel()` callbacks.
 */
class AsyncRestOperation : public AsyncOperation {
 public:
  /**
   * Notifies the application that the operation completed.
   *
   * Derived classes wrap the callbacks provided by the application and invoke
   * the callback when this virtual member function is called.
   *
   * @param ok opaque parameter returned by `RestCompletionQueue`.  The
   *   semantics depend on the type of operation, therefore the operation needs
   *   to interpret this parameter based on those semantics.
   * @return Whether the operation is completed.
   */
  virtual bool Notify(bool ok) = 0;
};

/**
 * Implementation for CompletionQueue that does NOT use a grpc::CompletionQueue.
 * Due to the lack of a completion queue that can manage multiple, simultaneous
 * REST requests, at least 2 threads need to be provided as the implementation
 * always reserves at least 1 thread to service timers.
 */
class RestCompletionQueueImpl final
    : public internal::CompletionQueueImpl,
      public std::enable_shared_from_this<RestCompletionQueueImpl> {
 public:
  ~RestCompletionQueueImpl() override = default;
  /// Creates an instance with 1 thread servicing the TimerQueue.
  RestCompletionQueueImpl();
  /// Creates and instance with `num_timer_threads` servicing the TimerQueue.
  //  explicit RestCompletionQueueImpl(std::size_t num_timer_threads);

  /// Run the event loop until Shutdown() is called.
  void Run() override;

  /// Terminate the event loop.
  void Shutdown() override;

  /// Cancel all existing operations.
  void CancelAll() override;

  /// Create a new timer.
  future<StatusOr<std::chrono::system_clock::time_point>> MakeDeadlineTimer(
      std::chrono::system_clock::time_point deadline) override;

  /// Create a new timer.
  future<StatusOr<std::chrono::system_clock::time_point>> MakeRelativeTimer(
      std::chrono::nanoseconds duration) override;

  /// Enqueue a new asynchronous function.
  void RunAsync(std::unique_ptr<internal::RunAsyncBase> function) override;

  /// This function is not supported by RestCompletionQueueImpl, but as the
  /// function is pure virtual, it must be overridden.
  void StartOperation(std::shared_ptr<internal::AsyncGrpcOperation>,
                      absl::FunctionRef<void(void*)>) override {
    GCP_LOG(FATAL) << " function not supported.\n";
  }

  /// The underlying gRPC completion queue, which does not exist.
  grpc::CompletionQueue* cq() override { return nullptr; }

  /// Some counters for testing and debugging.
  std::int64_t notify_counter() const { return notify_counter_.load(); }
  std::int64_t run_async_counter() const { return run_async_counter_.load(); }
  std::int64_t timer_counter() const { return cq_.timer_counter(); }
  std::size_t thread_pool_hwm() const { return thread_pool_hwm_; }
  std::size_t run_async_pool_hwm() const { return run_async_pool_hwm_; }

 private:
  void RunStart();
  void RunStop();

  std::mutex mu_;
  grpc::CompletionQueue grpc_cq_;
  RestCompletionQueue cq_;
  std::size_t thread_pool_size_ = 0;
  bool shutdown_{false};  // GUARDED_BY(mu_)
  std::shared_ptr<void> shutdown_guard_;

  // These are metrics used in testing.
  std::atomic<std::int64_t> notify_counter_{0};
  std::atomic<std::int64_t> run_async_counter_{0};
  std::size_t thread_pool_hwm_ = 0;
  std::size_t run_async_pool_hwm_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_IMPL_H
