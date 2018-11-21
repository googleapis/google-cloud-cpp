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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LONGRUNNING_OP_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LONGRUNNING_OP_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/completion_queue.h"
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
 * Asynchronously checks the status of a `google.longrunning.Operation`.
 *
 * This class, binds a client and a `google.longrunning.Operation`, so that one
 * can check if the operation is completed via the `Start` member function. It
 * also performs the unwrapping of the result, if one is obtained.
 *
 * It satisfies the requirements to be used as the `Operation` parameter in
 * `AsyncPollOp`.
 *
 * @tparam Client the type of client to execute `AsyncGetOperation` on
 * @tparam ResponseType the type of the response packed in the
 *     `google.longrunning.Operation`
 * @tparam valid_cliend a formal parameter, uses
 *     `std::enable_if<>` to disable this template if the `Client` doesn't have
 * a proper `AsyncGetOperation` member function.
 */
template <typename Client, typename ResponseType,
          typename std::enable_if<
              CheckAsyncUnaryRpcSignature<
                  typename internal::ExtractMemberFunctionType<decltype(
                      &Client::AsyncGetOperation)>::MemberFunction>::value,
              int>::type valid_client = 0>
class AsyncLongrunningOp {
 public:
  using Request = google::longrunning::GetOperationRequest;
  using Response = ResponseType;

  AsyncLongrunningOp(std::shared_ptr<Client> client,
                     google::longrunning::Operation operation)
      : client_(client), operation_(std::move(operation)) {}

  /**
   * Start the bound asynchronous request.
   *
   * @tparam Functor the type of the function-like object that will receive the
   *     results.
   *
   * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
   *     disable this template if the functor does not match the expected
   *     signature.
   *
   * @param cq the completion queue to run the asynchronous operations.
   *
   * @param context the gRPC context used for this request
   *
   * @param callback the functor which will be fired in an unspecified thread
   *     once the response stream completes
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&, bool, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext>&& context,
      Functor&& callback) {
    if (operation_.done()) {
      // The operation supplied in the ctor can be already completed. In such a
      // case, we shouldn't send the RPC - we already have the response.
      //
      // We could fire the callback right here, but we'd be risking a deadlock
      // if the user held a lock while submitting this request. Instead, let's
      // schedule the callback to fire on the thread running the completion
      // queue by submitting an expired timer.
      // TODO(#1467): stop using a timer for this purpose
      return cq.MakeRelativeTimer(
          std::chrono::seconds(0),
          [callback, this](CompletionQueue& cq, AsyncTimerResult result) {
            if (operation_.has_error()) {
              grpc::Status status(
                  static_cast<grpc::StatusCode>(operation_.error().code()),
                  operation_.error().message(),
                  "Error in operation " + operation_.name());
              callback(cq, true, status);
            } else {
              grpc::Status status;
              callback(cq, true, status);
            }
          });
    }
    google::longrunning::GetOperationRequest request;
    request.set_name(operation_.name());
    return cq.MakeUnaryRpc(
        *client_, &Client::AsyncGetOperation, request, std::move(context),
        [this, callback](CompletionQueue& cq,
                         google::longrunning::Operation& operation,
                         grpc::Status& status) {
          if (not status.ok()) {
            callback(cq, false, status);
            return;
          }
          operation_.Swap(&operation);
          if (not operation_.done()) {
            callback(cq, false, status);
            return;
          }
          if (operation_.has_error()) {
            grpc::Status res_status(
                static_cast<grpc::StatusCode>(operation_.error().code()),
                operation_.error().message(),
                "Error in operation " + operation_.name());
            callback(cq, true, res_status);
            return;
          }
          callback(cq, true, status);
          return;
        });
  }

  Response AccumulatedResult() {
    if (operation_.has_response()) {
      auto const& any = operation_.response();
      Response res;
      any.UnpackTo(&res);
      return res;
    } else {
      return Response();
    }
  }

 private:
  std::shared_ptr<Client> client_;
  google::longrunning::Operation operation_;
};

/**
 * Poll `AsyncLongrunningOp` result.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, Response, grpc::Status&>);
 * @tparam Client the type of client to execute AsyncGetOperation on
 * @tparam ResponseType the type of the response packed in the
 *     longrunning.Operation
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor, typename Client, typename Response,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, CompletionQueue&, Response&, grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncPollLongrunningOp
    : public AsyncPollOp<Functor, AsyncLongrunningOp<Client, Response>> {
 public:
  AsyncPollLongrunningOp(char const* error_message,
                         std::unique_ptr<PollingPolicy> polling_policy,
                         MetadataUpdatePolicy metadata_update_policy,
                         std::shared_ptr<Client> client,
                         google::longrunning::Operation operation,
                         Functor&& callback)
      : AsyncPollOp<Functor, AsyncLongrunningOp<Client, Response>>(
            error_message, std::move(polling_policy),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            AsyncLongrunningOp<Client, Response>(std::move(client),
                                                 std::move(operation))) {}
};

/**
 * Issue an async RPC with retries and asynchronously poll its result.
 *
 * This asynchronous operation can be used for executing API calls, which return
 * a `google.longrunning.Operation`. It will first retry the API call and it it
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
 *
 * @tparam Sig A formal parameter to discover if `MemberFunctionType` matches
 *     the required signature for an asynchronous gRPC call, and if so, what are
 *     the request and response parameter types for the RPC.
 *
 * @tparam valid_member_function_type a formal parameter, uses
 *     `std::enable_if<>` to disable this template if the client does not match
 *     have a proper `AsyncGetOperation` member function.
 *
 * @tparam valid_member_function_type a formal parameter, uses
 *     `std::enable_if<>` to disable this template if the member function type
 *     does not match the desired signature.
 *
 * @tparam operation_returns_longrunning a formal parameter, uses
 *     `std::enable_if<>` to disable this template if the member function type
 *     does not return `google::longrunning::Operation`
 */
template <
    typename Client, typename Response, typename MemberFunctionType,
    typename IdempotencyPolicy,
    typename Sig = internal::CheckAsyncUnaryRpcSignature<MemberFunctionType>,
    typename std::enable_if<
        CheckAsyncUnaryRpcSignature<
            typename internal::ExtractMemberFunctionType<decltype(
                &Client::AsyncGetOperation)>::MemberFunction>::value,
        int>::type client_has_get_operation = 0,
    typename std::enable_if<Sig::value, int>::type valid_member_function_type =
        0,
    typename std::enable_if<std::is_same<typename Sig::ResponseType,
                                         google::longrunning::Operation>::value,
                            int>::type operation_returns_longrunning = 0>
class AsyncRetryAndPollUnaryRpc
    : public std::enable_shared_from_this<AsyncRetryAndPollUnaryRpc<
          Client, Response, MemberFunctionType, IdempotencyPolicy>>,
      public AsyncOperation {
 public:
  using Request = typename Sig::RequestType;

  AsyncRetryAndPollUnaryRpc(
      char const* error_message, std::unique_ptr<PollingPolicy> polling_policy,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotencyPolicy idempotency_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::shared_ptr<Client> client, MemberFunctionType Client::*call,
      Request&& request)
      : error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        idempotency_policy_(std::move(idempotency_policy)),
        metadata_update_policy_(metadata_update_policy),
        client_(std::move(client)),
        call_(call),
        request(std::move(request)),
        cancelled_() {}

  bool Notify(CompletionQueue& cq, bool ok) override {
    // TODO(#1389) Notify should be moved from AsyncOperation to some more
    // specific derived class.
    google::cloud::internal::RaiseLogicError(
        "This member function doesn't make sense here");
  }

  void Cancel() override {
    std::lock_guard<std::mutex> lk(mu_);
    cancelled_ = true;
    if (current_op_) {
      current_op_->Cancel();
    }
  }

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&, Response&, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(CompletionQueue& cq,
                                        Functor&& callback) {
    std::unique_lock<std::mutex> lk(mu_);
    using Retry =
        internal::AsyncRetryUnaryRpc<Client, MemberFunctionType,
                                     IdempotencyPolicy,
                                     LongrunningStartedFunctor<Functor>>;
    auto retry = std::make_shared<Retry>(
        error_message_, std::move(rpc_retry_policy_),
        std::move(rpc_backoff_policy_), std::move(idempotency_policy_),
        metadata_update_policy_, client_, call_, std::move(request),
        LongrunningStartedFunctor<Functor>(this->shared_from_this(),
                                           std::forward<Functor>(callback)));
    current_op_ = retry->Start(cq);
    return this->shared_from_this();
  }

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

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&, Response&, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  class LongrunningCompletedFunctor {
   public:
    LongrunningCompletedFunctor(
        std::shared_ptr<AsyncRetryAndPollUnaryRpc> parent, Functor&& callback)
        : parent_(parent), callback_(std::forward<Functor>(callback)) {}

    void operator()(CompletionQueue& cq, Response& response,
                    grpc::Status const& status) {
      std::unique_lock<std::mutex> lk(parent_->mu_);
      parent_->current_op_.reset();
      lk.unlock();
      grpc::Status res_status(status);
      callback_(cq, response, res_status);
    };

   private:
    std::shared_ptr<AsyncRetryAndPollUnaryRpc> parent_;
    Functor callback_;
  };

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&, Response&, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  class LongrunningStartedFunctor {
   public:
    LongrunningStartedFunctor(std::shared_ptr<AsyncRetryAndPollUnaryRpc> parent,
                              Functor&& callback)
        : parent_(parent), callback_(std::forward<Functor>(callback)) {}
    void operator()(CompletionQueue& cq,
                    google::longrunning::Operation& operation,
                    grpc::Status& status) {
      std::unique_lock<std::mutex> lk(parent_->mu_);
      parent_->current_op_.reset();
      if (parent_->cancelled_) {
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
      if (not status.ok()) {
        lk.unlock();
        grpc::Status res_status(status);
        Response response;
        callback_(cq, response, res_status);
        return;
      }
      // All good, move on to polling for result.
      auto op = std::make_shared<AsyncPollLongrunningOp<
          LongrunningCompletedFunctor<Functor>, Client, Response>>(
          parent_->error_message_, std::move(parent_->polling_policy_),
          std::move(parent_->metadata_update_policy_),
          std::move(parent_->client_), std::move(operation),
          LongrunningCompletedFunctor<Functor>(
              parent_, std::forward<Functor>(callback_)));
      parent_->current_op_ = op->Start(cq);
    }

   private:
    std::shared_ptr<AsyncRetryAndPollUnaryRpc> parent_;
    Functor callback_;
  };
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LONGRUNNING_OP_H_
