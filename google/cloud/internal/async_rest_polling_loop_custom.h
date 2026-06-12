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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_CUSTOM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_CUSTOM_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_rest_polling_loop.h"
#include "google/cloud/internal/async_rest_polling_loop_impl.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "google/longrunning/operations.pb.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Customizable polling loop for services that do not conform to AIP-151.
 */
template <typename OperationType, typename GetOperationRequestType,
          typename CancelOperationRequestType>
future<StatusOr<OperationType>> AsyncRestPollingLoop(
    google::cloud::CompletionQueue cq, internal::ImmutableOptions options,
    future<StatusOr<OperationType>> op,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name) {
  auto loop = std::make_shared<AsyncRestPollingLoopImpl<
      OperationType, GetOperationRequestType, CancelOperationRequestType>>(
      std::move(cq), options, std::move(poll), std::move(cancel),
      std::move(polling_policy), std::move(location), is_operation_done,
      get_request_set_operation_name, cancel_request_set_operation_name);
  return loop->Start(std::move(op));
}

template <typename OperationType, typename GetOperationRequestType,
          typename CancelOperationRequestType>
future<StatusOr<OperationType>> AsyncRestPollingLoop(
    google::cloud::CompletionQueue cq, internal::ImmutableOptions options,
    future<StatusOr<OperationType>> op,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name,
    std::function<std::string(StatusOr<OperationType> const&)> operation_name) {
  auto loop = std::make_shared<AsyncRestPollingLoopImpl<
      OperationType, GetOperationRequestType, CancelOperationRequestType>>(
      std::move(cq), options, std::move(poll), std::move(cancel),
      std::move(polling_policy), std::move(location), is_operation_done,
      get_request_set_operation_name, cancel_request_set_operation_name,
      operation_name);
  return loop->Start(std::move(op));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_CUSTOM_H
