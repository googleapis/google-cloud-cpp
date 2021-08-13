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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RPC_DETAILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RPC_DETAILS_H

#include "google/cloud/async_operation.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <grpcpp/support/async_unary_call.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
class CompletionQueue;
namespace internal {

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
             std::unique_ptr<grpc::ClientContext> ctx, Request const& request,
             grpc::CompletionQueue* cq, void* tag) {
    // Need a copyable holder to use in the `std::function<>` callback:
    auto context = std::shared_ptr<grpc::ClientContext>(std::move(ctx));
    promise_ = promise<StatusOr<Response>>([context] { context->TryCancel(); });
    auto rpc = async_call(context.get(), request, cq);
    rpc->Finish(&response_, &status_, tag);
  }

  void Cancel() override {}

  bool Notify(bool ok) override {
    if (!ok) {
      // `Finish()` always returns `true` for unary RPCs, so the only time we
      // get `!ok` is after `Shutdown()` was called; treat that as "cancelled".
      promise_.set_value(Status(StatusCode::kCancelled, "call cancelled"));
      return true;
    }
    if (!status_.ok()) {
      // Convert the error to a `google::cloud::Status` and satisfy the future.
      promise_.set_value(MakeStatusFromRpcError(status_));
      return true;
    }
    // Success, use `response_` to satisfy the future.
    promise_.set_value(std::move(response_));
    return true;
  }

 private:
  // These are the parameters for the RPC, most of them have obvious semantics.
  // The promise will hold the grpc::ClientContext (in its cancel callback). It
  // uses a `unique_ptr` because (a) we need to receive it as a parameter,
  // otherwise the caller could not set timeouts, metadata, or any other
  // attributes, and (b) there is no move or assignment operator for
  // `grpc::ClientContext`.
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
using CheckUnaryStreamRpcDataCallback = ::google::cloud::internal::is_invocable<
    Functor, CompletionQueue&, grpc::ClientContext const&, Response&>;

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

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RPC_DETAILS_H
