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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Perform an asynchronous unary RPC request, with retries.
 *
 * @tparam Client the class implementing the asynchronous operation, examples
 *     include `DataClient`, `AdminClient`, and `InstanceAdminClient`.
 *
 * @tparam MemberFunctionType the type of the member function to call on the
 *     `Client` object. This type must meet the requirements of
 *     `internal::CheckAsyncUnaryRpcSignature`, the `AsyncRetryUnaryRpc`
 *     template is disabled otherwise.
 *
 * @tparam IdempotencyPolicy the policy used to determine if an operation is
 *     idempotent. In most cases this is just `ConstantIdempotentPolicy`
 *     because the decision around idempotency can be made before the retry loop
 *     starts. Some calls may dynamically determine if a retry (or a partial
 *     retry for BulkApply) are idempotent.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results.
 *
 * @tparam Sig A formal parameter to discover if `MemberFunctionType` matches
 *     the required signature for an asynchronous gRPC call, and if so, what are
 *     the request and response parameter types for the RPC.
 *
 * @tparam valid_member_function_type a formal parameter, uses
 *     `std::enable_if<>` to disable this template if the member function type
 *     does not match the desired signature.
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <
    typename Client, typename MemberFunctionType, typename IdempotencyPolicy,
    typename Functor,
    typename Sig = internal::CheckAsyncUnaryRpcSignature<MemberFunctionType>,
    typename std::enable_if<Sig::value, int>::type valid_member_function_type =
        0,
    typename std::enable_if<
        google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                              typename Sig::ResponseType&,
                                              grpc::Status&>::value,
        int>::type valid_callback_type = 0>
class AsyncRetryUnaryRpc
    : public std::enable_shared_from_this<AsyncRetryUnaryRpc<
          Client, MemberFunctionType, IdempotencyPolicy, Functor>> {
 public:
  //@{
  /// @name Convenience aliases for the RPC request and response types.
  using Request = typename Sig::RequestType;
  using Response = typename Sig::ResponseType;
  //@}

  /// The type used by `CompletionQueue` to return the RPC results.
  using AsyncResult = AsyncUnaryRpcResult<Response>;

  explicit AsyncRetryUnaryRpc(
      char const* error_message,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotencyPolicy idempotent_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::shared_ptr<Client> client, MemberFunctionType Client::*call,
      Request&& request, Functor&& callback)
      : error_message_(error_message),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        idempotent_policy_(std::move(idempotent_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        client_(std::move(client)),
        call_(std::move(call)),
        request_(std::move(request)),
        callback_(std::forward<Functor>(callback)) {}

  /**
   * Kick off the asynchronous request.
   *
   * @param cq the completion queue to run the asynchronous operations.
   */
  void Start(CompletionQueue& cq) {
    auto self = this->shared_from_this();
    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
    rpc_retry_policy_->Setup(*context);
    rpc_backoff_policy_->Setup(*context);
    metadata_update_policy_.Setup(*context);

    cq.MakeUnaryRpc(*client_, call_, request_, std::move(context),
                    [self](CompletionQueue& cq, AsyncResult& op,
                           AsyncOperation::Disposition d) {
                      self->OnCompletion(cq, op, d);
                    });
  }

 private:
  std::string FullErrorMessage(char const* where) {
    std::string full_message = error_message_;
    full_message += "(" + metadata_update_policy_.value() + ") ";
    full_message += where;
    return full_message;
  }

  std::string FullErrorMessage(char const* where, grpc::Status const& status) {
    std::string full_message = FullErrorMessage(where);
    full_message += ", last error=";
    full_message += status.error_message();
    return full_message;
  }

  /// The callback to handle one asynchronous request completing.
  void OnCompletion(CompletionQueue& cq, AsyncResult& op,
                    AsyncOperation::Disposition d) {
    if (d == AsyncOperation::CANCELLED) {
      // Cancelled, no retry necessary.
      callback_(cq, op.response,
                grpc::Status(
                    grpc::StatusCode::CANCELLED,
                    FullErrorMessage("pending operation cancelled", op.status),
                    op.status.error_details()));
      return;
    }
    if (op.status.ok()) {
      // Success, just report the result.
      callback_(cq, op.response, op.status);
      return;
    }
    if (not idempotent_policy_.is_idempotent()) {
      callback_(cq, op.response,
                grpc::Status(op.status.error_code(),
                             FullErrorMessage("non-idempotent operation failed",
                                              op.status),
                             op.status.error_details()));
      return;
    }
    if (not rpc_retry_policy_->OnFailure(op.status)) {
      std::string full_message =
          FullErrorMessage(RPCRetryPolicy::IsPermanentFailure(op.status)
                               ? "permanent error"
                               : "too many transient errors",
                           op.status);
      callback_(cq, op.response,
                grpc::Status(op.status.error_code(), full_message,
                             op.status.error_details()));
      return;
    }

    auto delay = rpc_backoff_policy_->OnCompletion(op.status);
    auto self = this->shared_from_this();
    cq.MakeRelativeTimer(
        delay,
        [self](CompletionQueue& cq, AsyncTimerResult timer,
               AsyncOperation::Disposition d) { self->OnTimer(cq, timer, d); });
  }

  void OnTimer(CompletionQueue& cq, AsyncTimerResult& timer,
               AsyncOperation::Disposition d) {
    if (d == AsyncOperation::CANCELLED) {
      // Cancelled, no more action to take.
      Response placeholder{};
      callback_(cq, placeholder,
                grpc::Status(grpc::StatusCode::CANCELLED,
                             FullErrorMessage("pending timer cancelled")));
      return;
    }
    Start(cq);
  }

  char const* error_message_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  IdempotencyPolicy idempotent_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<Client> client_;
  MemberFunctionType Client::*call_;
  Request request_;

  Functor callback_;
};

/**
 * An idempotent policy for `AsyncRetryUnaryRpc` based on a pre-computed value.
 *
 * In most APIs the idempotency of the API is either known at compile-time or
 * the value is unchanged during the retry loop. This class can be used in those
 * cases as the `IdempotentPolicy` template parameter for `AsyncRetryUnaryRpc`.
 */
class ConstantIdempotencyPolicy {
 public:
  explicit ConstantIdempotencyPolicy(bool is_idempotent)
      : is_idempotent_(is_idempotent) {}

  bool is_idempotent() const { return is_idempotent_; }

 private:
  bool is_idempotent_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H_
