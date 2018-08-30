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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_OPERATION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_OPERATION_H_

#include "google/cloud/bigtable/version.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class CompletionQueue;

namespace internal {
class CompletionQueueImpl;
}  // namespace internal
class CompletionQueue;

/**
 * The result of an async timer operation.
 *
 * Callbacks for async timers will receive an object of this class.
 */
struct AsyncTimerResult {
  std::chrono::system_clock::time_point deadline;
};

/**
 * The result of a unary asynchronous RPC operation.
 *
 * Applications provide a callback to be invoked when an asynchronous RPC
 * completes. The callback receives this parameter as the result of the
 * operation.
 *
 * @tparam Response
 */
template <typename Response>
struct AsyncUnaryRpcResult {
  Response response;
  grpc::Status status;
  std::unique_ptr<grpc::ClientContext> context;
};

/**
 * Represents a pending asynchronous operation.
 *
 * When applications create an asynchronous operations with a `CompletionQueue`
 * they provide a callback to be invoked when the operation completes
 * (successfully or not). The completion queue type-erases the callback and
 * hides it in a class derived from `AsyncOperation`.  A shared pointer to the
 * `AsyncOperation` is returned by the completion queue so library developers
 * can cancel the operation if needed.
 */
class AsyncOperation {
 public:
  virtual ~AsyncOperation() = default;

  /**
   * Requests that the operation be canceled.
   *
   * The result of canceling the operation is reported via `Notify()`, which
   * invokes the callback registered when the operation was created.
   */
  virtual void Cancel() = 0;

  enum Disposition {
    CANCELLED,
    COMPLETED,
  };

 private:
  friend class internal::CompletionQueueImpl;
  /**
   * Notifies the application that the operation completed.
   *
   * Derived classes wrap the callbacks provided by the application and invoke
   * the callback when this virtual member function is called.
   *
   * @param cq the completion queue sending the notification, this is useful in
   *   case the callback needs to retry the operation.
   * @param disposition `COMPLETED` if the operation completed, `CANCELED` if
   *   the operation were canceled. Note that errors are a "normal" completion.
   */
  virtual void Notify(CompletionQueue& cq, Disposition disposition) = 0;

  virtual void SimulateNotify() = 0;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_OPERATION_H_
