// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POLLING_LOOP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POLLING_LOOP_H

#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/longrunning/operations.pb.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <string>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * Extract the result of a long-running operation from the `response` field.
 */
template <typename ResultType>
struct PollingLoopResponseExtractor {
  using ReturnType = StatusOr<ResultType>;

  static ReturnType Extract(google::longrunning::Operation const& operation,
                            char const* location) {
    if (!operation.has_response()) {
      return Status(StatusCode::kInternal,
                    std::string(location) +
                        "() operation completed "
                        "without error or response, name=" +
                        operation.name());
    }
    google::protobuf::Any const& any = operation.response();
    if (!any.Is<ResultType>()) {
      return Status(StatusCode::kInternal,
                    std::string(location) +
                        "() operation completed "
                        "with an invalid response type, name=" +
                        operation.name());
    }
    ResultType result;
    any.UnpackTo(&result);
    return result;
  }
};

/**
 * Extract the result of a long-running operation from the `metadata` field.
 */
template <typename ResultType>
struct PollingLoopMetadataExtractor {
  using ReturnType = StatusOr<ResultType>;

  static ReturnType Extract(google::longrunning::Operation const& operation,
                            char const* location) {
    if (!operation.has_metadata()) {
      return Status(StatusCode::kInternal,
                    std::string(location) +
                        "() operation completed "
                        "without error or metadata, name=" +
                        operation.name());
    }
    google::protobuf::Any const& any = operation.metadata();
    if (!any.Is<ResultType>()) {
      return Status(StatusCode::kInternal,
                    std::string(location) +
                        "() operation completed "
                        "with an invalid metadata type, name=" +
                        operation.name());
    }
    ResultType result;
    any.UnpackTo(&result);
    return result;
  }
};

/**
 * A generic retry loop for gRPC operations.
 *
 * This function implements a retry loop suitable for *most* gRPC operations.
 *
 * @param retry_policy controls the duration of the retry loop.
 * @param backoff_policy controls how the loop backsoff from a recoverable
 *     failure.
 * @param is_idempotent if false, the operation is not retried even on transient
 *     errors.
 * @param functor the operation to retry, typically a lambda that encapsulates
 *     both the Stub and the function to call.
 * @param context the gRPC context used for the request, previous Stubs in the
 *     stack can set timeouts and metadata through this context.
 * @param request the parameters for the request.
 * @param location a string to annotate any error returned by this function.
 * @tparam Functor the type of @p functor.
 * @tparam Request the type of @p request.
 * @tparam Sleeper a dependency injection point to verify (in tests) that the
 *     backoff policy is used.
 * @return the result of the first successful call to @p functor, or a
 *     `google::cloud::Status` that indicates the final error for this request.
 */
template <typename ValueExtractor, typename Functor, typename Sleeper,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, grpc::ClientContext&,
                  google::longrunning::GetOperationRequest const&>::value,
              int>::type = 0>
typename ValueExtractor::ReturnType PollingLoopImpl(
    std::unique_ptr<PollingPolicy> polling_policy, Functor&& functor,
    google::longrunning::Operation operation, char const* location,
    Sleeper sleeper) {
  Status last_status;

  while (!operation.done()) {
    sleeper(polling_policy->WaitPeriod());

    grpc::ClientContext poll_context;
    google::longrunning::GetOperationRequest poll_request;
    poll_request.set_name(operation.name());
    auto update = functor(poll_context, poll_request);
    if (update && update->done()) {
      // Before updating the polling policy make sure we do not discard a
      // successful result that completes the request.
      using std::swap;
      swap(*update, operation);
      break;
    }
    // Update the polling policy even on successful requests, so we can stop
    // after too many polling attempts.
    if (!polling_policy->OnFailure(update.status())) {
      if (update) {
        return Status(StatusCode::kDeadlineExceeded,
                      "exhausted polling policy with no previous error");
      }
      return std::move(update).status();
    }
    if (update) {
      using std::swap;
      swap(*update, operation);
    }
  }

  if (operation.has_error()) {
    // The long running operation failed, return the error to the caller.
    return google::cloud::MakeStatusFromRpcError(operation.error());
  }
  return ValueExtractor::Extract(operation, location);
}

/// @copydoc RetryLoopImpl
template <typename ValueExtractor, typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, grpc::ClientContext&,
                  google::longrunning::GetOperationRequest const&>::value,
              int>::type = 0>
typename ValueExtractor::ReturnType PollingLoop(
    std::unique_ptr<PollingPolicy> polling_policy, Functor&& functor,
    google::longrunning::Operation operation, char const* location) {
  return PollingLoopImpl<ValueExtractor>(
      std::move(polling_policy), std::forward<Functor>(functor),
      std::move(operation), location,
      [](std::chrono::milliseconds p) { std::this_thread::sleep_for(p); });
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_POLLING_LOOP_H
