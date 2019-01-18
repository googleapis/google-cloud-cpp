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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/internal/conjunction.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A wrapper for enabling fetching multiple pages if passed to `AsyncLoopOp`.
 *
 * If used in `AsyncLoopOp` `MultipageRetriableAdapter` will keep calling
 * `Operation::Start()` to retrieve all parts (pages) of an API call whose
 * response come in parts. The callback passed to `Operation::Start()` is
 * expected to be called with `finished == true` iff all parts of the response
 * have arrived. There will be no delays between sending successful requests for
 * parts of data.
 *
 * @tparam UserFunctor the type of the function-like object that will receive
 *     the results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the requests for individual
 *     pages and their retries. It should also accumulate the result. It should
 *     satisfy `MeetsAsyncPollOperationRequirements`.
 */
template <typename UserFunctor, typename Operation>
class MultipageRetriableAdapter {
  using Response =
      typename MeetsAsyncPollOperationRequirements<Operation>::Response;
  static_assert(
      google::cloud::internal::is_invocable<UserFunctor, CompletionQueue&,
                                            Response&, grpc::Status&>::value,
      "UserFunctor should be callable with Operation::AccumulatedResult()'s "
      "return value.");

 public:
  explicit MultipageRetriableAdapter(
      char const* error_message,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy, UserFunctor&& callback,
      Operation&& operation)
      : error_message_(error_message),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        rpc_backoff_policy_prototype_(rpc_backoff_policy_->clone()),
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
        [this, attempt_completed_callback](CompletionQueue& cq, bool finished,
                                           grpc::Status& status) {
          OnCompletion(cq, finished, status,
                       std::move(attempt_completed_callback));
        });
  }

  std::chrono::milliseconds WaitPeriod() {
    if (status_.ok()) {
      return std::chrono::milliseconds(0);
    }
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
  void OnCompletion(CompletionQueue& cq, bool finished, grpc::Status& status,
                    AttemptFunctor&& attempt_completed_callback) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      Cancel(cq);
      attempt_completed_callback(cq, true);
      return;
    }
    if (finished) {
      // Finished, just report the result.
      auto res = operation_.AccumulatedResult();
      user_callback_(cq, res, status);
      attempt_completed_callback(cq, true);
      return;
    }
    if (status.ok()) {
      // Somethig is working, so let's reset backoff policy, so that if a
      // failure happens, we start from small wait periods.
      rpc_backoff_policy_ = rpc_backoff_policy_prototype_->clone();
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
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_prototype_;
  MetadataUpdatePolicy metadata_update_policy_;
  UserFunctor user_callback_;
  Operation operation_;
  grpc::Status status_;
};

/**
 * Retry a multi-page API call.
 *
 * `AsyncRetryMultiPage` will keep calling `Operation::Start()` to retrieve all
 * parts (pages) of an API call whose response come in parts. The callback
 * passed to `Operation::Start()` is expected to be called with `finished ==
 * true` iff all parts of the response have arrived. There will be no delays
 * between sending successful requests for parts of data.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the requests for individual
 *     pages and their retries. It should also accumulate the result. It should
 *     satisfy `MeetsAsyncPollOperationRequirements`.
 */
template <typename Functor, typename Operation>
class AsyncRetryMultiPage
    : public AsyncLoopOp<MultipageRetriableAdapter<Functor, Operation>> {
 public:
  AsyncRetryMultiPage(char const* error_message,
                      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                      MetadataUpdatePolicy metadata_update_policy,
                      Functor&& callback, Operation&& operation)
      : AsyncLoopOp<MultipageRetriableAdapter<Functor, Operation>>(
            MultipageRetriableAdapter<Functor, Operation>(
                error_message, std::move(rpc_retry_policy),
                std::move(rpc_backoff_policy),
                std::move(metadata_update_policy),
                std::forward<Functor>(callback), std::move(operation))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H_
