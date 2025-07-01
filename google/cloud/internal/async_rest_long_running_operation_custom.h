// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_LONG_RUNNING_OPERATION_CUSTOM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_LONG_RUNNING_OPERATION_CUSTOM_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_rest_long_running_operation.h"
#include "google/cloud/internal/async_rest_polling_loop_custom.h"
#include "google/cloud/internal/async_rest_retry_loop.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/*
 * AsyncRestLongRunningOperation for services that do not conform to AIP-151.
 */
template <typename ReturnType, typename OperationType,
          typename GetOperationRequestType, typename CancelOperationRequestType,
          typename RequestType, typename StartFunctor, typename RetryPolicyType,
          typename CompletionQueue>
future<StatusOr<ReturnType>> AsyncRestLongRunningOperation(
    CompletionQueue cq, internal::ImmutableOptions options,
    RequestType&& request, StartFunctor&& start,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    LongRunningOperationValueExtractor<ReturnType, OperationType>
        value_extractor,
    std::unique_ptr<RetryPolicyType> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, Idempotency idempotent,
    std::unique_ptr<PollingPolicy> polling_policy, char const* location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name) {
  auto operation =
      AsyncRestRetryLoop(std::move(retry_policy), std::move(backoff_policy),
                         idempotent, cq, std::forward<StartFunctor>(start),
                         options, std::forward<RequestType>(request), location);
  auto loc = std::string{location};
  return AsyncRestPollingLoop<OperationType, GetOperationRequestType,
                              CancelOperationRequestType>(
             std::move(cq), std::move(options), std::move(operation),
             std::move(poll), std::move(cancel), std::move(polling_policy),
             std::move(location), is_operation_done,
             get_request_set_operation_name, cancel_request_set_operation_name)
      .then([value_extractor, loc](future<StatusOr<OperationType>> g) {
        return value_extractor(g.get(), loc);
      });
}

template <typename ReturnType, typename OperationType,
          typename GetOperationRequestType, typename CancelOperationRequestType,
          typename RequestType, typename StartFunctor, typename RetryPolicyType,
          typename CompletionQueue>
future<StatusOr<ReturnType>> AsyncRestLongRunningOperation(
    CompletionQueue cq, internal::ImmutableOptions options,
    RequestType&& request, StartFunctor&& start,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    LongRunningOperationValueExtractor<ReturnType, OperationType>
        value_extractor,
    std::unique_ptr<RetryPolicyType> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, Idempotency idempotent,
    std::unique_ptr<PollingPolicy> polling_policy, char const* location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name,
    std::function<std::string(StatusOr<OperationType> const&)> operation_name) {
  auto operation =
      AsyncRestRetryLoop(std::move(retry_policy), std::move(backoff_policy),
                         idempotent, cq, std::forward<StartFunctor>(start),
                         options, std::forward<RequestType>(request), location);
  auto loc = std::string{location};
  return AsyncRestPollingLoop<OperationType, GetOperationRequestType,
                              CancelOperationRequestType>(
             std::move(cq), std::move(options), std::move(operation),
             std::move(poll), std::move(cancel), std::move(polling_policy),
             std::move(location), is_operation_done,
             get_request_set_operation_name, cancel_request_set_operation_name,
             operation_name)
      .then([value_extractor, loc](future<StatusOr<OperationType>> g) {
        return value_extractor(g.get(), loc);
      });
}

/*
 * AsyncAwaitRestLongRunningOperation for services that do not conform to
 * AIP-151.
 */
template <typename ReturnType, typename OperationType,
          typename GetOperationRequestType, typename CancelOperationRequestType,
          typename CompletionQueue>
future<StatusOr<ReturnType>> AsyncRestAwaitLongRunningOperation(
    CompletionQueue cq, internal::ImmutableOptions options,
    OperationType operation,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    LongRunningOperationValueExtractor<ReturnType, OperationType>
        value_extractor,
    std::unique_ptr<PollingPolicy> polling_policy, char const* location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name) {
  auto loc = std::string{location};
  return AsyncRestPollingLoop<OperationType, GetOperationRequestType,
                              CancelOperationRequestType>(
             std::move(cq), std::move(options),
             make_ready_future(StatusOr<OperationType>(operation)),
             std::move(poll), std::move(cancel), std::move(polling_policy),
             std::move(location), is_operation_done,
             get_request_set_operation_name, cancel_request_set_operation_name)
      .then([value_extractor, loc](future<StatusOr<OperationType>> g) {
        return value_extractor(g.get(), loc);
      });
}

template <typename ReturnType, typename OperationType,
          typename GetOperationRequestType, typename CancelOperationRequestType,
          typename CompletionQueue>
future<StatusOr<ReturnType>> AsyncRestAwaitLongRunningOperation(
    CompletionQueue cq, internal::ImmutableOptions options,
    OperationType operation,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    LongRunningOperationValueExtractor<ReturnType, OperationType>
        value_extractor,
    std::unique_ptr<PollingPolicy> polling_policy, char const* location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name,
    std::function<std::string(StatusOr<OperationType> const&)> operation_name) {
  auto loc = std::string{location};
  return AsyncRestPollingLoop<OperationType, GetOperationRequestType,
                              CancelOperationRequestType>(
             std::move(cq), std::move(options),
             make_ready_future(StatusOr<OperationType>(operation)),
             std::move(poll), std::move(cancel), std::move(polling_policy),
             std::move(location), is_operation_done,
             get_request_set_operation_name, cancel_request_set_operation_name,
             operation_name)
      .then([value_extractor, loc](future<StatusOr<OperationType>> g) {
        return value_extractor(g.get(), loc);
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_LONG_RUNNING_OPERATION_CUSTOM_H
