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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_CHECK_CONSISTENCY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_CHECK_CONSISTENCY_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A CheckConsistency call bound with client and table.
 *
 * It satisfies the requirements to be used as the `Operation` parameter in
 * `AsyncPollOp`.
 *
 * It encapsulates calling this RPC and holds the result.
 */
class AsyncCheckConsistency {
 public:
  using Request = google::bigtable::admin::v2::CheckConsistencyRequest;
  using Response = bool;

  AsyncCheckConsistency(std::shared_ptr<AdminClient> client,
                        ConsistencyToken&& consistency_token,
                        std::string const& table_name)
      : client_(client), response_() {
    request_.set_name(table_name);
    request_.set_consistency_token(consistency_token.get());
  }

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
    return cq.MakeUnaryRpc(
        *client_, &AdminClient::AsyncCheckConsistency, request_,
        std::move(context),
        [this, callback](
            CompletionQueue& cq,
            google::bigtable::admin::v2::CheckConsistencyResponse& response,
            grpc::Status& status) {
          if (status.ok() && response.consistent()) {
            response_ = true;
            callback(cq, true, status);
            return;
          }
          callback(cq, false, status);
        });
  }

  bool AccumulatedResult() { return response_; }

 private:
  std::shared_ptr<bigtable::AdminClient> client_;
  Request request_;
  bool response_;
};

/**
 * Poll `AsyncCheckConsistency` result.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, bool, grpc::Status&>);
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                    bool, grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncPollCheckConsistency
    : public AsyncPollOp<Functor, AsyncCheckConsistency> {
 public:
  AsyncPollCheckConsistency(char const* error_message,
                            std::unique_ptr<PollingPolicy> polling_policy,
                            MetadataUpdatePolicy metadata_update_policy,
                            std::shared_ptr<bigtable::AdminClient> client,
                            ConsistencyToken consistency_token,
                            std::string const& table_name, Functor&& callback)
      : AsyncPollOp<Functor, AsyncCheckConsistency>(
            error_message, std::move(polling_policy),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            AsyncCheckConsistency(std::move(client),
                                  std::move(consistency_token),
                                  std::move(table_name))) {}
};

/**
 * Await until replication catches up.
 *
 * This implementation of `AsyncOperation` wraps getting a `ConsistencyToken`
 * via a retried `AsyncGenerateConsistencyToken` call and passing it on to
 * `AsyncPollCheckConsistency` to poll until consistency is reached.
 *
 * It holds all the necessary data to launch the following
 * `AsyncPollCheckConsistency` after `AsyncGenerateConsistencyToken` finishes
 * and implements `AsyncOperation` so that it can return itself to the user as a
 * handle for cancellation.
 */
class AsyncAwaitConsistency
    : public std::enable_shared_from_this<AsyncAwaitConsistency>,
      public AsyncOperation {
 public:
  AsyncAwaitConsistency(char const* error_message,
                        std::unique_ptr<PollingPolicy> polling_policy,
                        std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                        std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                        MetadataUpdatePolicy metadata_update_policy,
                        std::shared_ptr<bigtable::AdminClient> client,
                        std::string const& table_name)
      : error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        metadata_update_policy_(metadata_update_policy),
        client_(std::move(client)),
        table_name_(table_name),
        cancelled_() {}

  void Cancel() override {
    std::lock_guard<std::mutex> lk(mu_);
    cancelled_ = true;
    if (current_op_) {
      current_op_->Cancel();
    }
  }

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(CompletionQueue& cq,
                                        Functor&& callback) {
    std::unique_lock<std::mutex> lk(mu_);
    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncGenerateConsistencyToken)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncGenerateConsistencyToken)>::MemberFunction;
    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     ConsistencyTokenGeneratedFunctor<Functor>>;
    google::bigtable::admin::v2::GenerateConsistencyTokenRequest request;
    request.set_name(table_name_);

    auto retry = std::make_shared<Retry>(
        error_message_, std::move(rpc_retry_policy_),
        std::move(rpc_backoff_policy_),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &AdminClient::AsyncGenerateConsistencyToken,
        std::move(request),
        ConsistencyTokenGeneratedFunctor<Functor>(
            shared_from_this(), std::forward<Functor>(callback)));
    current_op_ = retry->Start(cq);
    return std::static_pointer_cast<AsyncOperation>(this->shared_from_this());
  }

  std::mutex mu_;
  char const* error_message_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<bigtable::AdminClient> client_;
  std::string table_name_;
  std::shared_ptr<AsyncOperation> current_op_;
  bool cancelled_;

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  class CheckConsistencyFunctor {
   public:
    CheckConsistencyFunctor(std::shared_ptr<AsyncAwaitConsistency> parent,
                            Functor&& callback)
        : parent_(parent), callback_(std::forward<Functor>(callback)) {}

    void operator()(CompletionQueue& cq, bool response,
                    grpc::Status const& status) {
      std::unique_lock<std::mutex> lk(parent_->mu_);
      parent_->current_op_.reset();
      if (status.ok() && !response) {
        // I don't think it could happen, TBH.
        grpc::Status res_status(grpc::StatusCode::UNKNOWN,
                                "The state is not consistent and for some "
                                "unknown reason we didn't retry. Expected "
                                "that either a consistent state is reached or "
                                "a polling error is reported. That's a bug, "
                                "please report it to "
                                "https://github.com/googleapis/"
                                "google-cloud-cpp/issues/new");
        lk.unlock();
        callback_(cq, res_status);
        return;
      }
      lk.unlock();
      grpc::Status res_status(status);
      callback_(cq, res_status);
    };

   private:
    std::shared_ptr<AsyncAwaitConsistency> parent_;
    Functor callback_;
  };

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  class ConsistencyTokenGeneratedFunctor {
   public:
    ConsistencyTokenGeneratedFunctor(
        std::shared_ptr<AsyncAwaitConsistency> parent, Functor&& callback)
        : parent_(parent), callback_(std::forward<Functor>(callback)) {}
    void operator()(
        CompletionQueue& cq,
        google::bigtable::admin::v2::GenerateConsistencyTokenResponse& response,
        grpc::Status& status) {
      std::unique_lock<std::mutex> lk(parent_->mu_);
      parent_->current_op_.reset();
      if (parent_->cancelled_) {
        // Cancel could have been called too late for GenerateConsistencyToken
        // to notice - it might have finished with a success. In such a scenario
        // we should still interrupt the execution, i.e. not schedule
        // CheckConsistency.
        lk.unlock();
        grpc::Status res_status(grpc::StatusCode::CANCELLED,
                                "User requested to cancel.");
        callback_(cq, res_status);
        return;
      }
      if (!status.ok()) {
        lk.unlock();
        grpc::Status res_status(status);
        callback_(cq, res_status);
        return;
      }
      // All good, move on to polling for consistency.
      auto op = std::make_shared<
          AsyncPollCheckConsistency<CheckConsistencyFunctor<Functor>>>(
          __func__, std::move(parent_->polling_policy_),
          std::move(parent_->metadata_update_policy_),
          std::move(parent_->client_),
          ConsistencyToken(std::move(response.consistency_token())),
          std::move(parent_->table_name_),
          CheckConsistencyFunctor<Functor>(parent_,
                                           std::forward<Functor>(callback_)));
      parent_->current_op_ = op->Start(cq);
    }

   private:
    std::shared_ptr<AsyncAwaitConsistency> parent_;
    Functor callback_;
  };
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_CHECK_CONSISTENCY_H_
