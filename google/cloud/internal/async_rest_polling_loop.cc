// Copyright 2023 Google LLC
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

#include "google/cloud/internal/async_rest_polling_loop.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_rest_polling_loop_impl.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/longrunning/operations.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::longrunning::Operation;

future<StatusOr<Operation>> AsyncRestPollingLoopAip151(
    google::cloud::CompletionQueue cq, internal::ImmutableOptions options,
    future<StatusOr<Operation>> op,
    AsyncRestPollLongRunningOperation<google::longrunning::Operation,
                                      google::longrunning::GetOperationRequest>
        poll,
    AsyncRestCancelLongRunningOperation<
        google::longrunning::CancelOperationRequest>
        cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location) {
  auto loop = std::make_shared<AsyncRestPollingLoopImpl<
      google::longrunning::Operation, google::longrunning::GetOperationRequest,
      google::longrunning::CancelOperationRequest>>(
      std::move(cq), std::move(options), std::move(poll), std::move(cancel),
      std::move(polling_policy), std::move(location));
  return loop->Start(std::move(op));
}

// TODO(#12359) - remove once it is no longer used.
future<StatusOr<Operation>> AsyncRestPollingLoopAip151(
    google::cloud::CompletionQueue cq, future<StatusOr<Operation>> op,
    AsyncRestPollLongRunningOperationImplicitOptions<
        google::longrunning::Operation,
        google::longrunning::GetOperationRequest>
        poll,
    AsyncRestCancelLongRunningOperationImplicitOptions<
        google::longrunning::CancelOperationRequest>
        cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location) {
  auto poll_wrapper =
      [poll = std::move(poll)](
          google::cloud::CompletionQueue& cq,
          std::unique_ptr<RestContext> context, Options const&,
          google::longrunning::GetOperationRequest const& request) {
        return poll(cq, std::move(context), request);
      };
  auto cancel_wrapper =
      [cancel = std::move(cancel)](
          google::cloud::CompletionQueue& cq,
          std::unique_ptr<RestContext> context, Options const&,
          google::longrunning::CancelOperationRequest const& request) {
        return cancel(cq, std::move(context), request);
      };
  return AsyncRestPollingLoopAip151(
      std::move(cq), internal::SaveCurrentOptions(), std::move(op),
      std::move(poll_wrapper), std::move(cancel_wrapper),
      std::move(polling_policy), std::move(location));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
