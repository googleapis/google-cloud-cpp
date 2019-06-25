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
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/status_or.h"
#include <grpcpp/alarm.h>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <atomic>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class CompletionQueue;
namespace internal {

/**
 * Represents an AsyncOperation which gRPC understands.
 *
 * When applications create an asynchronous operations with a `CompletionQueue`
 * they provide a callback to be invoked when the operation completes
 * (successfully or not). The completion queue type-erases the callback and
 * hides it in a class derived from `AsyncOperation`. A shared pointer to the
 * `AsyncOperation` is returned by the completion queue so library developers
 * can cancel the operation if needed.
 */
class AsyncGrpcOperation : public AsyncOperation {
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
   * @param ok opaque parameter returned by grpc::CompletionQueue.  The
   *   semantics defined by gRPC depend on the type of operation, therefore the
   *   operation needs to interpret this parameter based on those semantics.
   * @return Whether the operation is completed (e.g. in case of streaming
   *   response, it would return true only after the stream is finished).
   */
  virtual bool Notify(CompletionQueue& cq, bool ok) = 0;
};

/**
 * Wrap a unary RPC callback into a `AsyncOperation`.
 *
 * This class is used by the implementation of `CompletionQueue` to associate
 * a future with an asynchronous unary RPC call. gRPC requires applications to
 * provide a `grpc::ClientContext` object, an object of the response type, and a
 * `grpc::Status` object to make an asynchronous RPC. The lifetime of these
 * objects must be at least as long as the duration of the asynchronous call.
 * Furthermore, the application must provide a unique `void*` that is associated
 * with the RPC.
 *
 * This class is used in the implementation of `CompletionQueue` to hold the
 * objects mentioned above. Furthermore, when the operation is completed, it
 * transfers the result to satisfy the future associated with the RPC.
 *
 * @tparam Request the type of the RPC request.
 * @tparam Response the type of the RPC response.
 */
template <typename Request, typename Response>
class AsyncUnaryRpcFuture : public AsyncGrpcOperation {
 public:
  AsyncUnaryRpcFuture() = default;

  future<StatusOr<Response>> GetFuture() { return promise_.get_future(); }

  /// Prepare the operation to receive the response and start the RPC.
  template <typename AsyncFunctionType>
  void Start(AsyncFunctionType async_call,
             std::unique_ptr<grpc::ClientContext> context,
             Request const& request, grpc::CompletionQueue* cq, void* tag) {
    context_ = std::move(context);
    auto rpc = async_call(context_.get(), request, cq);
    rpc->Finish(&response_, &status_, tag);
  }

  void Cancel() override { context_->TryCancel(); }

 private:
  bool Notify(CompletionQueue&, bool ok) override {
    if (!ok) {
      // This would mean a bug in gRPC. The documentation states that Finish()
      // always returns `true` for unary RPCs.
      promise_.set_value(::google::cloud::Status(
          google::cloud::StatusCode::kUnknown, "Finish() returned false"));
      return true;
    }
    if (!status_.ok()) {
      // Convert the error to a `google::cloud::Status` and satisfy the future.
      promise_.set_value(grpc_utils::MakeStatusFromRpcError(status_));
      return true;
    }
    // Success, use `response_` to satisfy the future.
    promise_.set_value(std::move(response_));
    return true;
  }

  // These are the parameters for the RPC, most of them have obvious semantics.
  // `context_` is stored as a `unique_ptr` because (a) we need to receive it
  // as a parameter, otherwise the caller could not set timeouts, metadata, or
  // any other attributes, and (b) there is no move or assignment operator for
  // `grpc::ClientContext`.
  std::unique_ptr<grpc::ClientContext> context_;
  grpc::Status status_;
  Response response_;

  promise<StatusOr<Response>> promise_;
};

/// Verify that @p Functor meets the requirements for an AsyncUnaryRpc callback.
template <typename Functor, typename Response>
using CheckUnaryRpcCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&, Response&,
                                          grpc::Status&>;

/**
 * Verify that @p Functor meets the requirements for an AsyncUnaryStreamRpc
 * data callback.
 */
template <typename Functor, typename Response>
using CheckUnaryStreamRpcDataCallback = google::cloud::internal::is_invocable<
    Functor, CompletionQueue&, const grpc::ClientContext&, Response&>;

/**
 * Verify that @p Functor meets the requirements for an AsyncUnaryStreamRpc
 * finishing callback.
 */
template <typename Functor, typename Response>
using CheckUnaryStreamRpcFinishedCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                          grpc::ClientContext&, grpc::Status&>;

/**
 * Tests if @p Functor meets the requirements for a RunAsync callback.
 *
 * @tparam Functor a type the application wants to use as a callback.
 */
template <typename Functor>
using CheckRunAsyncCallback =
    google::cloud::internal::is_invocable<Functor, CompletionQueue&>;

/**
 * A meta function to extract the `ResponseType` from an AsyncCall return type.
 *
 * This meta function extracts, if possible, the response type from an
 * asynchronous RPC callable. These callables return a
 * `std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<T>>` and we are
 * interested in the `T` type.
 *
 * This is the generic version, implementing the "does not match the expected
 * type" path.
 */
template <typename ResponseType>
struct AsyncCallResponseTypeUnwrap : public std::false_type {
  using type = void;
};

/**
 * A meta function to extract the `ResponseType` from an AsyncCall return type.
 *
 * This meta function extracts, if possible, the response type from an
 * asynchronous RPC callable. These callables return a
 * `std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<T>>` and we are
 * interested in the `T` type.
 *
 * This is the specialization implementing the "matched with the expected type"
 * path.
 */
template <typename ResponseType>
struct AsyncCallResponseTypeUnwrap<
    std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<ResponseType>>>
    : public std::true_type {
  using type = ResponseType;
};

/**
 * A meta function to determine the `ResponseType` from an asynchronous RPC
 * callable.
 *
 * Asynchronous calls have the form:
 *
 * @code
 *   std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<ResponseType>>(
 *      grpc::ClientContext*,
 *      RequestType const&,
 *      grpc::CompletionQueue*
 *   );
 * @endcode
 *
 * This meta-function extracts the `ResponseType` given the type of a callable
 * and the `RequestType`.
 */
template <typename AsyncCallType, typename RequestType>
using AsyncCallResponseType = AsyncCallResponseTypeUnwrap<
    typename google::cloud::internal::invoke_result_t<
        AsyncCallType, grpc::ClientContext*, RequestType const&,
        grpc::CompletionQueue*>>;

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

  /**
   * Run the event loop until Shutdown() is called.
   *
   * @param cq the completion queue wrapping this implementation class, used to
   *   notify any asynchronous operation that completes.
   */
  void Run(CompletionQueue& cq);

  /// Terminate the event loop.
  void Shutdown();

  /// Create a new alarm object.
  virtual std::unique_ptr<grpc::Alarm> CreateAlarm() const;

  /// The underlying gRPC completion queue.
  grpc::CompletionQueue& cq() { return cq_; }

  /// Add a new asynchronous operation to the completion queue.
  void* RegisterOperation(std::shared_ptr<AsyncGrpcOperation> op);

 protected:
  /// Return the asynchronous operation associated with @p tag.
  std::shared_ptr<AsyncGrpcOperation> FindOperation(void* tag);

  /// Unregister @p tag from pending operations.
  void ForgetOperation(void* tag);

  /// Simulate a completed operation, provided only to support unit tests.
  void SimulateCompletion(CompletionQueue& cq, AsyncOperation* op, bool ok);

  /// Simulate completion of all pending operations, provided only to support
  /// unit tests.
  void SimulateCompletion(CompletionQueue& cq, bool ok);

  bool empty() const {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_.empty();
  }

  std::size_t size() const {
    std::unique_lock<std::mutex> lk(mu_);
    return pending_ops_.size();
  }

 private:
  grpc::CompletionQueue cq_;
  std::atomic<bool> shutdown_;
  mutable std::mutex mu_;
  std::unordered_map<std::intptr_t, std::shared_ptr<AsyncGrpcOperation>>
      pending_ops_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_COMPLETION_QUEUE_IMPL_H_
