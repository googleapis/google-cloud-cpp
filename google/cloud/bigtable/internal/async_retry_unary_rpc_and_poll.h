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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_AND_POLL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_AND_POLL_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_longrunning_op.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/internal/completion_queue_impl.h"
#include "google/cloud/bigtable/polling_policy.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Issue an async RPC with retries and asynchronously poll its result.
 *
 * This asynchronous operation can be used for executing API calls, which return
 * a `google.longrunning.Operation`. It will first retry the API call and if it
 * succeeds, it will poll that operation for result.
 *
 * This operation implements `AsyncOperation`, hence it is cancellable. On
 * cancel, it stops polling - it doesn't cancel the operation on the server
 * side.
 *
 * @tparam Client the class implementing the asynchronous operation, examples
 *     include `DataClient`, `AdminClient`, and `InstanceAdminClient`.
 *
 * @tparam Response object of this class is expected in response to this
 *     longrunning operation.
 *
 * @tparam MemberFunctionType the type of the member function to call on the
 *     `Client` object. This type must meet the requirements of
 *     `internal::CheckAsyncUnaryRpcSignature`, the `AsyncRetryAndPollUnaryRpc`
 *     template is disabled otherwise.
 *
 * @tparam IdempotencyPolicy the policy used to determine if an operation is
 *     idempotent. In most cases this is just `ConstantIdempotentPolicy`
 *     because the decision around idempotency can be made before the retry loop
 *     starts. Some calls may dynamically determine if a retry (or a partial
 *     retry for `BulkApply`) are idempotent.

 * @tparam Functor the type of the function-like object that will receive the
 *     results. This functor must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, Response&, grpc::Status&>
 */
template <typename Client, typename Response, typename MemberFunctionType,
          typename IdempotencyPolicy, typename Functor>
class AsyncRetryAndPollUnaryRpc
    : public std::enable_shared_from_this<AsyncRetryAndPollUnaryRpc<
          Client, Response, MemberFunctionType, IdempotencyPolicy, Functor>>,
      public AsyncOperation {
  using Sig = internal::CheckAsyncUnaryRpcSignature<MemberFunctionType>;
  static_assert(Sig::value, "MemberFunctionType is invalid");
  static_assert(
      std::is_same<typename Sig::ResponseType,
                   google::longrunning::Operation>::value,
      "MemberFunctionType doesn't return a google.longrunning.Operation");
  static_assert(google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&, Response&, grpc::Status&>::value,
                "Provided Functor type doesn't have the expected signature.");

 public:
  using Request = typename Sig::RequestType;

  AsyncRetryAndPollUnaryRpc(
      char const* error_message, std::unique_ptr<PollingPolicy> polling_policy,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotencyPolicy idempotency_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::shared_ptr<Client> client, MemberFunctionType Client::*call,
      Request&& request, Functor&& callback)
      : error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        idempotency_policy_(std::move(idempotency_policy)),
        metadata_update_policy_(metadata_update_policy),
        client_(std::move(client)),
        call_(call),
        request(std::move(request)),
        cancelled_(),
        callback_(callback) {}

  void Cancel() override {
    std::lock_guard<std::mutex> lk(mu_);
    cancelled_ = true;
    if (current_op_) {
      current_op_->Cancel();
    }
  }

  std::shared_ptr<AsyncOperation> Start(CompletionQueue& cq) {
    std::unique_lock<std::mutex> lk(mu_);
    auto self = this->shared_from_this();
    auto on_longrunning_started =
        [self](CompletionQueue& cq, google::longrunning::Operation& operation,
               grpc::Status& status) {
          self->OnLongrunningStarted(cq, operation, status);
        };
    using Retry =
        internal::AsyncRetryUnaryRpc<Client, MemberFunctionType,
                                     IdempotencyPolicy,
                                     decltype(on_longrunning_started)>;
    auto retry = std::make_shared<Retry>(
        error_message_, std::move(rpc_retry_policy_),
        std::move(rpc_backoff_policy_), std::move(idempotency_policy_),
        metadata_update_policy_, client_, call_, std::move(request),
        std::move(on_longrunning_started));
    current_op_ = retry->Start(cq);
    return this->shared_from_this();
  }

 private:
  void OnLongrunningStarted(CompletionQueue& cq,
                            google::longrunning::Operation& operation,
                            grpc::Status& status) {
    std::unique_lock<std::mutex> lk(mu_);
    current_op_.reset();
    if (cancelled_) {
      // Cancel could have been called too late for the RPC to notice - it
      // might have finished with a success. In such a scenario we should
      // still interrupt the execution, i.e. not schedule the poll.
      lk.unlock();
      grpc::Status res_status(grpc::StatusCode::CANCELLED,
                              "User requested to cancel.");
      Response response;
      callback_(cq, response, res_status);
      return;
    }
    if (!status.ok()) {
      lk.unlock();
      grpc::Status res_status(status);
      Response response;
      callback_(cq, response, res_status);
      return;
    }
    // All good, move on to polling for result.
    auto self = this->shared_from_this();
    auto on_longrunning_completed = [self](CompletionQueue& cq,
                                           Response& response,
                                           grpc::Status const& status) {
      self->OnLongrunningCompleted(cq, response, status);
    };
    auto op = std::make_shared<AsyncPollLongrunningOp<
        decltype(on_longrunning_completed), Client, Response>>(
        error_message_, std::move(polling_policy_),
        std::move(metadata_update_policy_), std::move(client_),
        std::move(operation), std::move(on_longrunning_completed));
    current_op_ = op->Start(cq);
  }

  void OnLongrunningCompleted(CompletionQueue& cq, Response& response,
                              grpc::Status const& status) {
    std::unique_lock<std::mutex> lk(mu_);
    current_op_.reset();
    lk.unlock();
    grpc::Status res_status(status);
    callback_(cq, response, res_status);
  };

  std::mutex mu_;
  char const* error_message_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  IdempotencyPolicy idempotency_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<Client> client_;
  MemberFunctionType Client::*call_;
  Request request;
  std::shared_ptr<AsyncOperation> current_op_;
  bool cancelled_;
  Functor callback_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_AND_POLL_H_
