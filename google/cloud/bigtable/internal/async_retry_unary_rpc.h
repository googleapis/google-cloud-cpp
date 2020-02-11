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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/make_unique.h"
#include <google/protobuf/empty.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Make an asynchronous unary RPC with retries.
 *
 * This class creates a future<> that becomes satisfied when an asynchronous
 * operation either:
 *
 * - Succeeds.
 * - Fails with a non-retryable error.
 * - The retry policy expires.
 *
 * The class retries the operation, using a backoff policy to wait between
 * retries. The class does not block, it uses the completion queue to wait.
 *
 * @tparam AsyncCallType the type of the callable used to start the asynchronous
 *     operation. This is typically a lambda that wraps both the `Client` object
 *     and the member function to invoke.
 * @tparam RequestType the type of the request object.
 * @tparam IdempotencyPolicy the type of the idempotency policy.
 * @tparam Sig discover the signature and return type of `AsyncCallType`.
 * @tparam ResponseType the discovered response type for `AsyncCallType`.
 * @tparam validate_parameters validate the other template parameters.
 */
template <
    typename AsyncCallType, typename RequestType, typename IdempotencyPolicy,
    typename Sig = google::cloud::internal::AsyncCallResponseType<AsyncCallType,
                                                                  RequestType>,
    typename ResponseType = typename Sig::type,
    typename std::enable_if<Sig::value, int>::type validate_parameters = 0>
class RetryAsyncUnaryRpcFuture {
 public:
  //@{
  /// @name Convenience aliases for the RPC request response and callback types.
  using Request = RequestType;
  using Response = ResponseType;
  //@}

  /**
   * Start the asynchronous retry loop.
   *
   * @param location typically the name of the function that created this
   *     asynchronous retry loop.
   * @param rpc_retry_policy controls the number of retries, and what errors are
   *     considered retryable.
   * @param rpc_backoff_policy determines the wait time between retries.
   * @param idempotent_policy determines if a request is retryable.
   * @param metadata_update_policy controls how to update the metadata fields in
   *     the request.
   * @param async_call the callable to start a new asynchronous operation.
   * @param request the parameters of the request.
   * @param cq the completion queue where the retry loop is executed.
   * @return a future that becomes satisfied when (a) one of the retry attempts
   *     is successful, or (b) one of the retry attempts fails with a
   *     non-retryable error, or (c) one of the retry attempts fails with a
   *     retryable error, but the request is non-idempotent, or (d) the
   *     retry policy is expired.
   */
  static future<StatusOr<Response>> Start(
      char const* location, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotencyPolicy idempotent_policy,
      MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
      Request request, CompletionQueue cq) {
    std::shared_ptr<RetryAsyncUnaryRpcFuture> self(new RetryAsyncUnaryRpcFuture(
        location, std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
        std::move(idempotent_policy), std::move(metadata_update_policy),
        async_call, std::move(request)));
    auto future = self->final_result_.get_future();
    self->StartIteration(self, cq);
    return future;
  }

 private:
  // The constructor is private because we always want to wrap the object in
  // a shared pointer. The lifetime is controlled by any pending operations in
  // the CompletionQueue.
  RetryAsyncUnaryRpcFuture(char const* location,
                           std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                           std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                           IdempotencyPolicy idempotent_policy,
                           MetadataUpdatePolicy metadata_update_policy,
                           AsyncCallType async_call, Request request)
      : location_(location),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        idempotent_policy_(std::move(idempotent_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        async_call_(std::move(async_call)),
        request_(std::move(request)) {}

  /// The callback for a completed request, successful or not.
  static void OnCompletion(std::shared_ptr<RetryAsyncUnaryRpcFuture> self,
                           CompletionQueue cq, StatusOr<Response> result) {
    if (result) {
      self->final_result_.set_value(std::move(result));
      return;
    }
    if (!self->idempotent_policy_.is_idempotent()) {
      self->final_result_.set_value(self->DetailedStatus(
          "non-idempotent operation failed", result.status()));
      return;
    }
    if (!self->rpc_retry_policy_->OnFailure(result.status())) {
      char const* context = RPCRetryPolicy::IsPermanentFailure(result.status())
                                ? "permanent error"
                                : "too many transient errors";
      self->final_result_.set_value(
          self->DetailedStatus(context, result.status()));
      return;
    }
    cq.MakeRelativeTimer(
          self->rpc_backoff_policy_->OnCompletion(result.status()))
        .then([self, cq](future<StatusOr<std::chrono::system_clock::time_point>>
                             result) {
          if (auto tp = result.get()) {
            self->StartIteration(self, cq);
          } else {
            self->final_result_.set_value(
                self->DetailedStatus("timer error", tp.status()));
          }
        });
  }

  /// The callback to start another iteration of the retry loop.
  static void StartIteration(std::shared_ptr<RetryAsyncUnaryRpcFuture> self,
                             CompletionQueue cq) {
    auto context =
        ::google::cloud::internal::make_unique<grpc::ClientContext>();
    self->rpc_retry_policy_->Setup(*context);
    self->rpc_backoff_policy_->Setup(*context);
    self->metadata_update_policy_.Setup(*context);

    cq.MakeUnaryRpc(self->async_call_, self->request_, std::move(context))
        .then([self, cq](future<StatusOr<Response>> fut) {
          self->OnCompletion(self, cq, fut.get());
        });
  }

  /// Generate an error message
  Status DetailedStatus(char const* context, Status const& status) {
    std::string full_message = location_;
    full_message += "(" + metadata_update_policy_.value() + ") ";
    full_message += context;
    full_message += ", last error=";
    full_message += status.message();
    return Status(status.code(), std::move(full_message));
  }

  char const* location_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  IdempotencyPolicy idempotent_policy_;
  MetadataUpdatePolicy metadata_update_policy_;

  AsyncCallType async_call_;
  Request request_;
  Response response_;

  promise<StatusOr<Response>> final_result_;
};

/**
 * Automatically deduce the type for `RetryAsyncUnaryRpc` and start the
 * asynchronous retry loop.
 *
 * @param location typically the name of the function that created this
 *     asynchronous retry loop.
 * @param rpc_retry_policy controls the number of retries, and what errors are
 *     considered retryable.
 * @param rpc_backoff_policy determines the wait time between retries.
 * @param idempotent_policy determines if a request is retryable.
 * @param metadata_update_policy controls how to update the metadata fields in
 *     the request.
 * @param async_call the callable to start a new asynchronous operation.
 * @param request the parameters of the request.
 * @param cq the completion queue where the retry loop is executed.
 *
 * @return a future that becomes satisfied when (a) one of the retry attempts
 *     is successful, or (b) one of the retry attempts fails with a
 *     non-retryable error, or (c) one of the retry attempts fails with a
 *     retryable error, but the request is non-idempotent, or (d) the
 *     retry policy is expired.
 */
template <
    typename AsyncCallType, typename RequestType, typename IdempotencyPolicy,
    typename Sig = google::cloud::internal::AsyncCallResponseType<AsyncCallType,
                                                                  RequestType>,
    typename ResponseType = typename Sig::type,
    typename std::enable_if<Sig::value, int>::type validate_parameters = 0>
future<StatusOr<ResponseType>> StartRetryAsyncUnaryRpc(
    char const* location, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    IdempotencyPolicy idempotent_policy,
    MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
    RequestType request, CompletionQueue cq) {
  return RetryAsyncUnaryRpcFuture<
      AsyncCallType, RequestType,
      IdempotencyPolicy>::Start(location, std::move(rpc_retry_policy),
                                std::move(rpc_backoff_policy),
                                std::move(idempotent_policy),
                                std::move(metadata_update_policy),
                                std::move(async_call), std::move(request),
                                std::move(cq));
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H
