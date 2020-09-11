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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/async_operation.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

// Type erase the callables in RunAsync()
struct RunAsyncBase {
  virtual ~RunAsyncBase() = default;
  virtual void exec() = 0;
};

/**
 * The implementation details for `CompletionQueue`.
 *
 * `CompletionQueue` is implemented using the PImpl idiom:
 *     https://en.wikipedia.org/wiki/Opaque_pointer
 *
 * This class defines the interface for the Impl class in that idiom.
 */
class CompletionQueueImpl {
 public:
  virtual ~CompletionQueueImpl() = default;

  /// Run the event loop until Shutdown() is called.
  virtual void Run() = 0;

  /// Terminate the event loop.
  virtual void Shutdown() = 0;

  /// Cancel all existing operations.
  virtual void CancelAll() = 0;

  /// Create a new timer.
  virtual future<StatusOr<std::chrono::system_clock::time_point>>
  MakeDeadlineTimer(std::chrono::system_clock::time_point deadline) = 0;

  /// Create a new timer.
  virtual future<StatusOr<std::chrono::system_clock::time_point>>
  MakeRelativeTimer(std::chrono::nanoseconds duration) = 0;

  /// Enqueue a new asynchronous function.
  virtual void RunAsync(std::unique_ptr<RunAsyncBase> function) = 0;

  /// Atomically add a new operation to the completion queue and start it.
  virtual void StartOperation(std::shared_ptr<AsyncGrpcOperation> op,
                              absl::FunctionRef<void(void*)> start) = 0;

  /// The underlying gRPC completion queue.
  virtual grpc::CompletionQueue& cq() = 0;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPLETION_QUEUE_IMPL_H
