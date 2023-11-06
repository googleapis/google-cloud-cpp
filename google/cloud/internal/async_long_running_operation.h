// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_LONG_RUNNING_OPERATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_LONG_RUNNING_OPERATION_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_polling_loop.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/extract_long_running_result.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include <google/longrunning/operations.pb.h>
#include <grpcpp/grpcpp.h>
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename ReturnType>
using LongRunningOperationValueExtractor = std::function<StatusOr<ReturnType>(
    StatusOr<google::longrunning::Operation>, std::string const&)>;

/**
 * Asynchronously starts and polls a long-running operation.
 *
 * Long-running operations [aip/151] are used for API methods that take a
 * significant amount of time to complete (think minutes, maybe an hour). The
 * gRPC API returns a "promise" object, represented by the
 * `google::longrunning::Operation` proto, and the application (or client
 * library) should periodically poll this object until it is "done".
 *
 * In the C++ client libraries we represent these long-running operations by
 * a member function that returns `future<StatusOr<ReturnType>>`. This function
 * is a helper to implement these member functions. It first starts the
 * operation using an asynchronous retry loop, and then starts an asynchronous
 * loop to poll the operation until it completes.
 *
 * The promise can complete with an error, which is represented by a
 * `google::cloud::Status` object, or with success and some `ReturnType` value.
 * The application may also configure the "polling policy", which may stop the
 * polling even though the operation has not completed.
 *
 * Library developers would use this function as follows:
 *
 * @code
 * class BarStub {
 *  public:
 *   virtual future<StatusOr<google::longrunning::Operation>> AsyncFoo(
 *     google::cloud::CompletionQueue& cq,
 *     std::shared_ptr<grpc::ClientContext> context,
 *     Options const& options,
 *     FooRequest const& request) = 0;
 *
 *   virtual future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
 *     google::cloud::CompletionQueue& cq,
 *     std::shared_ptr<grpc::ClientContext> context,
 *     Options const& options,
 *     google::longrunning::GetOperationRequest const& request) = 0;
 *   virtual future<Status> AsyncCancelOperation(
 *     google::cloud::CompletionQueue& cq,
 *     std::shared_ptr<grpc::ClientContext> context,
 *     Options const& options,
 *     google::longrunning::CancelOperationRequest const& request) = 0;
 * };
 * @endcode
 *
 * The corresponding `*ConnectionImpl` class would look as follows:
 *
 * @code
 * class BarConnectionImpl : public BarConnection {
 *  public:
 *   future<StatusOr<FooResponse>> Foo(FooRequest const& request) override {
 *     auto current = google::cloud::internal::SaveCurrentOptions();
 *     return google::cloud::internal::AsyncLongRunningOperation(
 *       cq_, request,
 *       [stub = stub_](
 *           auto& cq, auto context, auto const& options, auto const& request) {
 *         return stub->AsyncFoo(cq, std::move(context), options, request);
 *       },
 *       [stub = stub_](
 *           auto& cq, auto context, auto const& options, auto const& request) {
 *         return stub->AsyncGetOperation(
 *             cq, std::move(context), options, request);
 *       },
 *       [stub = stub_](
 *           auto cq, auto context, auto const& options, auto const& r) {
 *         return stub->AsyncCancelOperation(
 *             cq, std::move(context), options, r);
 *       },
 *       retry_policy(*current), backoff_policy(*current),
 *       IdempotencyPolicy::kIdempotent,
 *       polling_policy(*current),
 *       current, __func__ // for debugging
 *       );
 *   }
 *
 *  private:
 *    google::cloud::CompletionQueue cq_;
 *    std::shared_ptr<BarStub> stub_;
 * };
 * @endcode
 *
 * [aip/151]: https://google.aip.dev/151
 */
template <typename ReturnType, typename RequestType, typename StartFunctor,
          typename RetryPolicyType>
future<StatusOr<ReturnType>> AsyncLongRunningOperation(
    google::cloud::CompletionQueue cq, ImmutableOptions options,
    RequestType&& request, StartFunctor&& start,
    AsyncPollLongRunningOperation poll, AsyncCancelLongRunningOperation cancel,
    LongRunningOperationValueExtractor<ReturnType> value_extractor,
    std::unique_ptr<RetryPolicyType> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, Idempotency idempotent,
    std::unique_ptr<PollingPolicy> polling_policy, char const* location) {
  using ::google::longrunning::Operation;
  auto operation =
      AsyncRetryLoop(std::move(retry_policy), std::move(backoff_policy),
                     idempotent, cq, std::forward<StartFunctor>(start), options,
                     std::forward<RequestType>(request), location);
  auto loc = std::string{location};
  return AsyncPollingLoop(std::move(cq), std::move(options),
                          std::move(operation), std::move(poll),
                          std::move(cancel), std::move(polling_policy),
                          std::move(location))
      .then([value_extractor, loc](future<StatusOr<Operation>> g) {
        return value_extractor(g.get(), loc);
      });
}

// TODO(#12359) - remove this overload once it becomes unused
template <typename ReturnType, typename RequestType, typename StartFunctor,
          typename RetryPolicyType>
future<StatusOr<ReturnType>> AsyncLongRunningOperation(
    google::cloud::CompletionQueue cq, RequestType&& request,
    StartFunctor&& start, AsyncPollLongRunningOperationImplicitOptions poll,
    AsyncCancelLongRunningOperationImplicitOptions cancel,
    LongRunningOperationValueExtractor<ReturnType> value_extractor,
    std::unique_ptr<RetryPolicyType> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, Idempotency idempotent,
    std::unique_ptr<PollingPolicy> polling_policy, char const* location) {
  auto start_wrapper = [start = std::forward<StartFunctor>(start)](
                           google::cloud::CompletionQueue& cq,
                           std::shared_ptr<grpc::ClientContext> context,
                           Options const&, RequestType const& request) {
    return start(cq, std::move(context), request);
  };
  auto poll_wrapper =
      [poll = std::move(poll)](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          Options const&,
          google::longrunning::GetOperationRequest const& request) {
        return poll(cq, std::move(context), request);
      };
  auto cancel_wrapper =
      [cancel = std::move(cancel)](
          CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
          Options const&,
          google::longrunning::CancelOperationRequest const& request) {
        return cancel(cq, std::move(context), request);
      };
  return AsyncLongRunningOperation<ReturnType>(
      std::move(cq), google::cloud::internal::SaveCurrentOptions(),
      std::forward<RequestType>(request), std::move(start_wrapper),
      std::move(poll_wrapper), std::move(cancel_wrapper),
      std::move(value_extractor), std::move(retry_policy),
      std::move(backoff_policy), idempotent, std::move(polling_policy),
      location);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_LONG_RUNNING_OPERATION_H
