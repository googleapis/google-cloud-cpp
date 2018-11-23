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
 * A polling pollicy which backs off only on errors.
 *
 * An `AsyncPollOp` operation with this policy would keep polling with no sleeps
 * between successful attempts. Only failures (e.g. transient unavailability)
 * would result in a delay, as dictated by the RPCBackoffPolicy passed in the
 * ctor.
 *
 * This abuse of polling is used for getting responses, which come in pages.
 * There is no reason to wait between portions of data.
 *
 * TODO(#1475) This class should not be used elsewhere. It makes assumptions on
 * how it is used.
 */
class MultipagePollingPolicy : public PollingPolicy {
 public:
  MultipagePollingPolicy(std::unique_ptr<RPCRetryPolicy> retry,
                         std::unique_ptr<RPCBackoffPolicy> backoff);
  std::unique_ptr<PollingPolicy> clone() override;
  bool IsPermanentError(grpc::Status const& status) override;
  bool OnFailure(grpc::Status const& status) override;
  bool Exhausted() override;
  std::chrono::milliseconds WaitPeriod() override;

 private:
  // Indicates if the last seen status was a success.
  bool last_was_success_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_prototype_;
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
 *
 * @tparam valid_callback_type a formal parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, typename Operation::Response&,
 *         grpc::Status&>);
 */
template <typename Functor, typename Operation,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, CompletionQueue&, typename Operation::Response&,
                  grpc::Status&>::value,
              int>::type valid_callback_type = 0,
          typename std::enable_if<
              MeetsAsyncPollOperationRequirements<Operation>::value, int>::type
              operation_meets_requirements = 0>
class AsyncRetryMultiPage : public AsyncPollOp<Functor, Operation> {
 public:
  AsyncRetryMultiPage(char const* error_message,
                      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                      MetadataUpdatePolicy metadata_update_policy,
                      Functor&& callback, Operation&& operation)
      : AsyncPollOp<Functor, Operation>(
            error_message,
            google::cloud::internal::make_unique<MultipagePollingPolicy>(
                std::move(rpc_retry_policy), std::move(rpc_backoff_policy)),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            std::move(operation)) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_MULTI_PAGE_H_
