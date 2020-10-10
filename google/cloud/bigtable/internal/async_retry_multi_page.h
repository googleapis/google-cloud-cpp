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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/version.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

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
          typename Response = typename google::cloud::internal::
              AsyncCallResponseType<AsyncCallType, Request>::type>
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
      // Something is working, so let's reset backoff policy, so that if a
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
        .then([self](future<StatusOr<std::chrono::system_clock::time_point>>
                         result) {
          if (auto tp = result.get()) {
            self->StartIteration(self);
          } else {
            self->final_result_.set_value(
                self->DetailedStatus("timer error", tp.status()));
          }
        });
  }

  /// The callback to start another iteration of the retry loop.
  static void StartIteration(std::shared_ptr<AsyncRetryMultiPageFuture> self) {
    auto context = absl::make_unique<grpc::ClientContext>();
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
    // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
    MetadataUpdatePolicy metadata_update_policy, AsyncCallType async_call,
    Request request, Accumulator accumulator,
    // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H
