// Copyright 2023 Google LLC
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

#include "google/cloud/future.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/timer_queue.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include <atomic>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implementation for CompletionQueue that does NOT use a grpc::CompletionQueue.
 *
 * Due to the lack of a completion queue that can manage multiple, simultaneous
 * REST requests, asynchronous calls should be launched on a thread of their own
 * and RunAsync should only be called with a function to join that thread after
 * it completes its work.
 */
class RestCompletionQueueImpl final
    : public internal::CompletionQueueImpl,
      public std::enable_shared_from_this<RestCompletionQueueImpl> {
 public:
  ~RestCompletionQueueImpl() override = default;
  RestCompletionQueueImpl();

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
                      absl::FunctionRef<void(void*)>) override;

  /// The underlying gRPC completion queue, which does not exist.
  grpc::CompletionQueue* cq() override { return nullptr; }

  /// Some counters for testing and debugging.
  std::int64_t run_async_counter() const { return run_async_counter_.load(); }

 private:
  std::shared_ptr<internal::TimerQueue> tq_;

  // These are metrics used in testing.
  std::atomic<std::int64_t> run_async_counter_{0};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_IMPL_H
