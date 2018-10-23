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
 * Perform an asynchronous operation, with retries.
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
 * @tparam Operation a class responsible for submitting requests. Its Start()
 *     method will be used for sending the retries and the original request. In
 *     case of simple operations, it will just keep sending the same request,
 *     but in case of more sophisticated ones (e.g. BulkApply), the content
 *     might change with every retry. For reference, consult AsyncUnaryRpc.
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename IdempotencyPolicy, typename Functor, typename Operation,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, CompletionQueue&, typename Operation::Response&,
                  grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncRetryOp : public std::enable_shared_from_this<
                         AsyncRetryOp<IdempotencyPolicy, Functor, Operation>> {
 public:
  explicit AsyncRetryOp(char const* error_message,
                        std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                        std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                        IdempotencyPolicy idempotent_policy,
                        MetadataUpdatePolicy metadata_update_policy,
                        Functor&& callback, Operation&& impl)
      : error_message_(error_message),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        idempotent_policy_(std::move(idempotent_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        callback_(std::forward<Functor>(callback)),
        impl_(std::move(impl)) {}

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

    impl_.Start(cq, std::move(context),
                [self](CompletionQueue& cq, grpc::Status& status) {
                  self->OnCompletion(cq, status);
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
  void OnCompletion(CompletionQueue& cq, grpc::Status& status) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      auto res = impl_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::CANCELLED,
          FullErrorMessage("pending operation cancelled", status),
          status.error_details());
      callback_(cq, res, res_status);
      return;
    }
    if (status.ok()) {
      // Success, just report the result.
      auto res = impl_.AccumulatedResult();
      callback_(cq, res, status);
      return;
    }
    if (not idempotent_policy_.is_idempotent()) {
      auto res = impl_.AccumulatedResult();
      grpc::Status res_status(
          status.error_code(),
          FullErrorMessage("non-idempotent operation failed", status),
          status.error_details());
      callback_(cq, res, res_status);
      return;
    }
    if (not rpc_retry_policy_->OnFailure(status)) {
      std::string full_message =
          FullErrorMessage(RPCRetryPolicy::IsPermanentFailure(status)
                               ? "permanent error"
                               : "too many transient errors",
                           status);
      auto res = impl_.AccumulatedResult();
      grpc::Status res_status(status.error_code(), full_message,
                              status.error_details());
      callback_(cq, res, res_status);
      return;
    }

    auto delay = rpc_backoff_policy_->OnCompletion(status);
    auto self = this->shared_from_this();
    cq.MakeRelativeTimer(delay,
                         [self](CompletionQueue& cq, AsyncTimerResult result) {
                           self->OnTimer(cq, result);
                         });
  }

  void OnTimer(CompletionQueue& cq, AsyncTimerResult& timer) {
    if (timer.cancelled) {
      // Cancelled, no more action to take.
      auto res = impl_.AccumulatedResult();
      grpc::Status res_status(grpc::StatusCode::CANCELLED,
                              FullErrorMessage("pending timer cancelled"));
      callback_(cq, res, res_status);
      return;
    }
    Start(cq);
  }

  char const* error_message_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  IdempotencyPolicy idempotent_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  Functor callback_;

 protected:
  Operation impl_;
};

/**
 * An idempotent policy for `AsyncRetryOp` based on a pre-computed value.
 *
 * In most APIs the idempotency of the API is either known at compile-time or
 * the value is unchanged during the retry loop. This class can be used in those
 * cases as the `IdempotentPolicy` template parameter for `AsyncRetryOp`.
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
