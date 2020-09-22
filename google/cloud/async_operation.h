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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_OPERATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_OPERATION_H

#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * The result of an async timer operation.
 *
 * Callbacks for async timers will receive an object of this class.
 */
struct AsyncTimerResult {
  std::chrono::system_clock::time_point deadline;
  bool cancelled;
};

/**
 * Represents a pending asynchronous operation.
 *
 * It can either be a simple RPC, or a more complex operation involving
 * potentially many RPCs, sleeping and processing.
 */
class AsyncOperation {
 public:
  virtual ~AsyncOperation() = default;

  /**
   * Requests that the operation be canceled.
   */
  virtual void Cancel() = 0;
};

namespace internal {

/**
 * Represents an AsyncOperation which gRPC understands.
 *
 * When applications create an asynchronous operation with a `CompletionQueue`
 * they provide a callback to be invoked when the operation completes
 * (successfully or not). The completion queue type-erases the callback and
 * hides it in a class derived from `AsyncOperation`. A shared pointer to the
 * `AsyncOperation` is returned by the completion queue so library developers
 * can cancel the operation if needed.
 */
class AsyncGrpcOperation : public AsyncOperation {
 public:
  /**
   * Notifies the application that the operation completed.
   *
   * Derived classes wrap the callbacks provided by the application and invoke
   * the callback when this virtual member function is called.
   *
   * @param ok opaque parameter returned by `grpc::CompletionQueue`.  The
   *   semantics defined by gRPC depend on the type of operation, therefore the
   *   operation needs to interpret this parameter based on those semantics.
   * @return Whether the operation is completed (e.g. in case of streaming
   *   response, it would return true only after the stream is finished).
   */
  virtual bool Notify(bool ok) = 0;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_OPERATION_H
