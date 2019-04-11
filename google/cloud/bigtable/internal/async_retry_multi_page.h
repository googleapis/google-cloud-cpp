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
#include "google/cloud/bigtable/version.h"
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
      MetadataUpdatePolicy metadata_update_policy, UserFunctor callback,
      Operation operation)
      : error_message_(error_message),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        rpc_backoff_policy_prototype_(rpc_backoff_policy_->clone()),
        metadata_update_policy_(std::move(metadata_update_policy)),
        user_callback_(std::move(callback)),
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
                      Functor callback, Operation operation)
      : AsyncLoopOp<MultipageRetriableAdapter<Functor, Operation>>(
            MultipageRetriableAdapter<Functor, Operation>(
                error_message, std::move(rpc_retry_policy),
                std::move(rpc_backoff_policy),
                std::move(metadata_update_policy), std::move(callback),
                std::move(operation))) {}
};

template <typename AsyncCallType, typename Request, typename Accumulator,
          typename CombiningFunction>
future<StatusOr<Accumulator>> StartAsyncRetryMultiPage(
    char const* location, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
    Request request, Accumulator accumulator,
    CombiningFunction combining_function, CompletionQueue cq);

/**
 * Retry a multi-page API call.
 *
 * `AsyncRetryMultiPageFuture` will keep calling an underlying operation to
 * retrieve all parts (pages) of an API call whose response come in parts.
 * Retries are also performed.
 *
 * In order to aggregate the results, the user should provide an accumulator and
 * a combining function which will be used in a "fold left" like fashion to
 * obtain the final result.
 *
 * @tparam AsyncCallType the type of the callable used to start the asynchronous
 *     operation. This is typically a lambda that wraps both the `Client` object
 *     and the member function to invoke.
 * @tparam RequestType the type of the request object.
 * @tparam Accumulator the type of the accumulator where intermediate results
 *     will be stored and at the same time, the returned type
 * @tparam CombiningFunction the type of the callable used to compute the final
 *     results from individual pages; it should have the following signature:
 *     `Accumulator(Accumulator, Request)`
 * @tparam Response the discovered response type for `AsyncCallType`.
 */
template <typename AsyncCallType, typename Request, typename Accumulator,
          typename CombiningFunction,
          typename Response = typename internal::AsyncCallResponseType<
              AsyncCallType, Request>::type>
class AsyncRetryMultiPageFuture {
 private:
  static_assert(
      std::is_same<Accumulator,
                   cloud::internal::invoke_result_t<
                       CombiningFunction, Accumulator, Response>>::value,
      "CombiningFunction signature is expected to be "
      "Accumulator CombiningFunction(Accumulator, Response)");

  // The constructor is private because we always want to wrap the object in
  // a shared pointer. The lifetime is controlled by any pending operations in
  // the CompletionQueue.
  AsyncRetryMultiPageFuture(
      char const* location, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
      Request request, Accumulator accumulator,
      CombiningFunction combining_function, CompletionQueue cq)
      : location_(location),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        rpc_backoff_policy_prototype_(rpc_backoff_policy_->clone()),
        metadata_update_policy_(std::move(metadata_update_policy)),
        async_call_(std::move(async_call)),
        request_(std::move(request)),
        accumulator_(std::move(accumulator)),
        combining_function_(std::move(combining_function)),
        cq_(std::move(cq)) {}

  /// The callback for a completed request, successful or not.
  static void OnCompletion(std::shared_ptr<AsyncRetryMultiPageFuture> self,
                           StatusOr<Response> result) {
    if (result) {
      // Somethig is working, so let's reset backoff policy, so that if a
      // failure happens, we start from small wait periods.
      self->rpc_backoff_policy_ = self->rpc_backoff_policy_prototype_->clone();
      self->next_page_token_ = result->next_page_token();
      self->accumulator_ = self->combining_function_(
          std::move(self->accumulator_), std::move(*result));
      if (self->next_page_token_.empty()) {
        self->final_result_.set_value(std::move(self->accumulator_));
        return;
      }
      self->StartIteration(self);
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
    self->cq_
        .MakeRelativeTimer(
            self->rpc_backoff_policy_->OnCompletion(result.status()))
        .then([self](future<std::chrono::system_clock::time_point>) {
          self->StartIteration(self);
        });
  }

  /// The callback to start another iteration of the retry loop.
  static void StartIteration(std::shared_ptr<AsyncRetryMultiPageFuture> self) {
    auto context =
        ::google::cloud::internal::make_unique<grpc::ClientContext>();
    self->rpc_retry_policy_->Setup(*context);
    self->rpc_backoff_policy_->Setup(*context);
    self->metadata_update_policy_.Setup(*context);
    self->request_.set_page_token(self->next_page_token_);

    self->cq_
        .MakeUnaryRpc(self->async_call_, self->request_, std::move(context))
        .then([self](future<StatusOr<Response>> fut) {
          self->OnCompletion(self, fut.get());
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
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_prototype_;
  MetadataUpdatePolicy metadata_update_policy_;
  AsyncCallType async_call_;
  Request request_;
  Accumulator accumulator_;
  CombiningFunction combining_function_;
  std::string next_page_token_;
  promise<StatusOr<Accumulator>> final_result_;
  CompletionQueue cq_;

  friend future<StatusOr<Accumulator>> StartAsyncRetryMultiPage<
      AsyncCallType, Request, Accumulator, CombiningFunction>(
      char const* location, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
      Request request, Accumulator accumulator,
      CombiningFunction combining_function, CompletionQueue cq);
};

/**
 * Start the asynchronous fetching of multiple pages.
 *
 * @param location typically the name of the function that created this
 *     asynchronous retry loop.
 * @param rpc_retry_policy controls the number of retries, and what errors are
 *     considered retryable.
 * @param rpc_backoff_policy determines the wait time between retries.
 * @param metadata_update_policy controls how to update the metadata fields in
 *     the request.
 * @param async_call the callable to start a new asynchronous operation.
 * @param request the parameters of the request.
 * @param accumulator the initial value of the accumulated result
 * @param combining_function a callable used to accumulate intermediate
 *     results into `accumulator`; it should have the following signature:
 *     `Accumulator(Accumulator, Result)`
 * @param cq the completion queue where the retry loop is executed.
 * @return a future that becomes satisfied when (a) all of the pages are
 *     successfully fetched (last value of accumulator is returned), or (b) one
 *     of the retry attempts fails with a non-retryable error, or (d) the retry
 *     policy is expired.
 */
template <typename AsyncCallType, typename Request, typename Accumulator,
          typename CombiningFunction>
future<StatusOr<Accumulator>> StartAsyncRetryMultiPage(
    char const* location, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
    Request request, Accumulator accumulator,
    CombiningFunction combining_function, CompletionQueue cq) {
  std::shared_ptr<AsyncRetryMultiPageFuture<AsyncCallType, Request, Accumulator,
                                            CombiningFunction>>
      self(new AsyncRetryMultiPageFuture<AsyncCallType, Request, Accumulator,
                                         CombiningFunction>(
          location, std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
          std::move(metadata_update_policy), async_call, std::move(request),
          std::move(accumulator), std::move(combining_function),
          std::move(cq)));
  auto future = self->final_result_.get_future();
  self->StartIteration(self);
  return future;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H_
