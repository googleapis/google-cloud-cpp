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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_OP_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_OP_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_loop_op.h"
#include "google/cloud/bigtable/internal/async_op_traits.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/throw_delegate.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A dummy function object only to ease specification of retriable operations.
 *
 * It is an example type which could be passed to `Start()` member function of
 * the operation to be retried.
 */
struct PrototypeStartCallback {
  void operator()(CompletionQueue&, grpc::Status&) const {}
};

/**
 * A check if the template parameter meets criteria for `AsyncRetryOp`.
 */
template <typename Operation>
struct MeetsAsyncOperationRequirements {
  static_assert(HasStart<Operation, PrototypeStartCallback>::value,
                "Operation has to have a templated Start() member function "
                "instantiatable with functors like PrototypeStartCallback");
  static_assert(
      HasAccumulatedResult<Operation>::value,
      "Operation has to have an AccumulatedResult() member function.");
  static_assert(
      google::cloud::internal::is_invocable<
          decltype(&Operation::template Start<PrototypeStartCallback>),
          Operation&, CompletionQueue&, std::unique_ptr<grpc::ClientContext>&&,
          PrototypeStartCallback&&>::value,
      "Operation::Start<PrototypeStartCallback> has to be "
      "non-static and invocable with "
      "CompletionQueue&, std::unique_ptr<grpc::ClientContext>&&, "
      "PrototypeStartCallback&&.");
  static_assert(google::cloud::internal::is_invocable<
                    decltype(&Operation::AccumulatedResult), Operation&>::value,
                "Operation::AccumulatedResult has to be non-static and "
                "invokable with no arguments");
  static_assert(
      std::is_same<
          google::cloud::internal::invoke_result_t<
              decltype(&Operation::template Start<PrototypeStartCallback>),
              Operation&, CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>&&, PrototypeStartCallback&&>,
          std::shared_ptr<AsyncOperation>>::value,
      "Operation::Start<>(...) has to return a "
      "std::shared_ptr<AsyncOperation>");

  using Response = google::cloud::internal::invoke_result_t<
      decltype(&Operation::AccumulatedResult), Operation&>;
};

/**
 * A wrapper for operations adding retry logic if passed to `AsyncLoopOp`.
 *
 * If used in `AsyncLoopOp`, it will asynchronously retry `Operation`
 *
 * @tparam IdempotencyPolicy the policy used to determine if an operation is
 *     idempotent. In most cases this is just `ConstantIdempotentPolicy`
 *     because the decision around idempotency can be made before the retry loop
 *     starts. Some calls may dynamically determine if a retry (or a partial
 *     retry for `BulkApply`) are idempotent.
 *
 * @tparam UserFunctor the type of the function-like object that will receive
 *     the results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. In case of simple operations, it will just keep sending the same
 *     request, but in case of more sophisticated ones (e.g. `BulkApply`), the
 *     content might change with every retry. For reference, consult
 *     `AsyncUnaryRpc` and `MeetsAsyncOperationRequirements`.
 */
template <typename IdempotencyPolicy, typename UserFunctor, typename Operation>
class RetriableLoopAdapter {
  using Response =
      typename MeetsAsyncOperationRequirements<Operation>::Response;
  static_assert(
      google::cloud::internal::is_invocable<UserFunctor, CompletionQueue&,
                                            Response&, grpc::Status&>::value,
      "UserFunctor should be callable with Operation::AccumulatedResult()'s "
      "return value.");

 public:
  explicit RetriableLoopAdapter(
      char const* error_message,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotencyPolicy idempotent_policy,
      MetadataUpdatePolicy metadata_update_policy, UserFunctor&& callback,
      Operation&& operation)
      : error_message_(error_message),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        idempotent_policy_(std::move(idempotent_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        user_callback_(std::forward<UserFunctor>(callback)),
        operation_(std::move(operation)) {}

  template <typename AttemptFunctor>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, AttemptFunctor&& attempt_completed_callback) {
    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
    rpc_retry_policy_->Setup(*context);
    rpc_backoff_policy_->Setup(*context);
    metadata_update_policy_.Setup(*context);

    return operation_.Start(
        cq, std::move(context),
        [this, attempt_completed_callback](CompletionQueue& cq,
                                           grpc::Status& status) {
          OnCompletion(cq, status, std::move(attempt_completed_callback));
        });
  }

  std::chrono::milliseconds WaitPeriod() {
    return rpc_backoff_policy_->OnCompletion(status_);
  }

  void Cancel(CompletionQueue& cq) {
    auto res = operation_.AccumulatedResult();
    grpc::Status res_status(
        grpc::StatusCode::CANCELLED,
        FullErrorMessageUnlocked("pending operation cancelled"));
    user_callback_(cq, res, res_status);
  }

 private:
  /// The callback to handle one asynchronous request completing.
  template <typename AttemptFunctor>
  void OnCompletion(CompletionQueue& cq, grpc::Status& status,
                    AttemptFunctor&& attempt_completed_callback) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      Cancel(cq);
      attempt_completed_callback(cq, true);
      return;
    }
    if (status.ok()) {
      // Success, just report the result.
      auto res = operation_.AccumulatedResult();
      user_callback_(cq, res, status);
      attempt_completed_callback(cq, true);
      return;
    }
    if (!idempotent_policy_.is_idempotent()) {
      grpc::Status res_status(
          status.error_code(),
          FullErrorMessageUnlocked("non-idempotent operation failed", status),
          status.error_details());
      auto res = operation_.AccumulatedResult();
      user_callback_(cq, res, res_status);
      attempt_completed_callback(cq, true);
      return;
    }
    if (!rpc_retry_policy_->OnFailure(status)) {
      std::string full_message =
          FullErrorMessageUnlocked(RPCRetryPolicy::IsPermanentFailure(status)
                                       ? "permanent error"
                                       : "too many transient errors",
                                   status);
      grpc::Status res_status(status.error_code(), full_message,
                              status.error_details());
      auto res = operation_.AccumulatedResult();
      user_callback_(cq, res, res_status);
      attempt_completed_callback(cq, true);
      return;
    }
    status_ = status;
    attempt_completed_callback(cq, false);
  }

  std::string FullErrorMessageUnlocked(char const* where) {
    std::string full_message = error_message_;
    full_message += "(" + metadata_update_policy_.value() + ") ";
    full_message += where;
    return full_message;
  }

  std::string FullErrorMessageUnlocked(char const* where,
                                       grpc::Status const& status) {
    std::string full_message = FullErrorMessageUnlocked(where);
    full_message += ", last error=";
    full_message += status.error_message();
    return full_message;
  }

  char const* error_message_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  IdempotencyPolicy idempotent_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  UserFunctor user_callback_;
  Operation operation_;
  grpc::Status status_;
};

/**
 * Perform an asynchronous operation, with retries.
 *
 * @tparam IdempotencyPolicy the policy used to determine if an operation is
 *     idempotent. In most cases this is just `ConstantIdempotentPolicy`
 *     because the decision around idempotency can be made before the retry loop
 *     starts. Some calls may dynamically determine if a retry (or a partial
 *     retry for `BulkApply`) are idempotent.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. In case of simple operations, it will just keep sending the same
 *     request, but in case of more sophisticated ones (e.g. `BulkApply`), the
 *     content might change with every retry. For reference, consult
 *     `AsyncUnaryRpc` and `MeetsAsyncOperationRequirements`.
 */
template <typename IdempotencyPolicy, typename Functor, typename Operation>
class AsyncRetryOp
    : public AsyncLoopOp<
          RetriableLoopAdapter<IdempotencyPolicy, Functor, Operation>> {
 public:
  AsyncRetryOp(char const* error_message,
               std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
               std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
               IdempotencyPolicy idempotent_policy,
               MetadataUpdatePolicy metadata_update_policy, Functor&& callback,
               Operation&& operation)
      : AsyncLoopOp<
            RetriableLoopAdapter<IdempotencyPolicy, Functor, Operation>>(
            RetriableLoopAdapter<IdempotencyPolicy, Functor, Operation>(
                error_message, std::move(rpc_retry_policy),
                std::move(rpc_backoff_policy), std::move(idempotent_policy),
                metadata_update_policy, std::forward<Functor>(callback),
                std::move(operation))) {}
};

/**
 * An idempotent policy for `AsyncRetryOp` based on a pre-computed value.
 *
 * In most APIs the idempotency of the API is either known at compile-time or
 * the value is unchanged during the retry loop. This class can be used in
 * those cases as the `IdempotentPolicy` template parameter for
 * `AsyncRetryOp`.
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_OP_H_
