// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DEFAULT_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DEFAULT_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/version.h"
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <deque>
#include <unordered_map>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * The default implementation for `CompletionQueue`.
 */
class DefaultCompletionQueueImpl
    : public CompletionQueueImpl,
      public std::enable_shared_from_this<DefaultCompletionQueueImpl> {
 public:
  DefaultCompletionQueueImpl();
  ~DefaultCompletionQueueImpl() override = default;

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
  void RunAsync(std::unique_ptr<RunAsyncBase> function) override;

  /// Atomically add a new operation to the completion queue and start it.
  void StartOperation(std::shared_ptr<AsyncGrpcOperation> op,
                      absl::FunctionRef<void(void*)> start) override;

  /// The underlying gRPC completion queue.
  grpc::CompletionQueue& cq() override;

  /// Some counters for testing and debugging.
  std::int64_t notify_counter() const { return notify_counter_.load(); }
  std::size_t thread_pool_hwm() const { return thread_pool_hwm_; }
  std::size_t run_async_pool_hwm() const { return run_async_pool_hwm_; }

 private:
  /// Start an operation with the lock already held.
  void StartOperation(std::unique_lock<std::mutex> lk,
                      std::shared_ptr<AsyncGrpcOperation> op,
                      absl::FunctionRef<void(void*)> start);

  /// Return the asynchronous operation associated with @p tag.
  std::shared_ptr<AsyncGrpcOperation> FindOperation(void* tag);

  /// Unregister @p tag from pending operations.
  void ForgetOperation(void* tag);

  void RunStart() {
    std::lock_guard<std::mutex> lk(mu_);
    ++thread_pool_size_;
    thread_pool_hwm_ = (std::max)(thread_pool_hwm_, thread_pool_size_);
  }

  void RunStop() {
    std::lock_guard<std::mutex> lk(mu_);
    --thread_pool_size_;
  }

  void DrainRunAsyncLoop();
  void DrainRunAsyncOnIdle();
  void WakeUpRunAsyncThread(std::unique_lock<std::mutex> lk);

  class WakeUpRunAsyncLoop;
  class WakeUpRunAsyncOnIdle;

  std::mutex mu_;
  grpc::CompletionQueue cq_;
  std::size_t thread_pool_size_ = 0;
  std::size_t run_async_pool_size_ = 0;
  std::deque<std::unique_ptr<internal::RunAsyncBase>> run_async_queue_;
  bool shutdown_{false};  // GUARDED_BY(mu_)
  std::unordered_map<void*, std::shared_ptr<AsyncGrpcOperation>>
      pending_ops_;  // GUARDED_BY(mu_)
  // This member acts as a ref counter. When it drops to 0, it calls
  // `cq_.Shutdown()`. Look into `StartOperation` for why it is necessary.
  std::shared_ptr<void> shutdown_guard_;

  // These are metrics used in testing.
  std::atomic<std::int64_t> notify_counter_{0};
  std::size_t thread_pool_hwm_ = 0;
  std::size_t run_async_pool_hwm_ = 0;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_DEFAULT_COMPLETION_QUEUE_IMPL_H
