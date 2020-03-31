// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_RETRY_LOOP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_RETRY_LOOP_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/status_or.h"
#include <grpcpp/grpcpp.h>
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/// A helper function to treat all results the same way in `RetryLoop()`.
inline Status GetResultStatus(Status status) { return status; }

/// @copydoc GetResultStatus(Status)
template <typename T>
Status GetResultStatus(StatusOr<T> result) {
  return std::move(result).status();
}

/// Generate an error Status for `RetryLoop()`
Status RetryLoopError(char const* loop_message, char const* location,
                      Status const& last_status);

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
 * @param functor the operation to retry, typically a lambda that encasulates
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
template <typename Functor, typename Request, typename Sleeper,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, grpc::ClientContext&, Request const&>::value,
              int>::type = 0>
auto RetryLoopImpl(std::unique_ptr<RetryPolicy> retry_policy,
                   std::unique_ptr<BackoffPolicy> backoff_policy,
                   bool is_idempotent, Functor&& functor,
                   Request const& request, char const* location,
                   Sleeper sleeper)
    -> google::cloud::internal::invoke_result_t<Functor, grpc::ClientContext&,
                                                Request const&> {
  Status last_status;
  while (!retry_policy->IsExhausted()) {
    // Need to create a new context for each retry.
    grpc::ClientContext context;
    auto result = functor(context, request);
    if (result.ok()) {
      return result;
    }
    last_status = GetResultStatus(std::move(result));
    if (!is_idempotent) {
      return RetryLoopError("Error in non-idempotent operation", location,
                            last_status);
    }
    if (!retry_policy->OnFailure(last_status)) {
      // The retry policy is exhausted or the error is not retryable, either
      // way, exit the loop.
      break;
    }
    sleeper(backoff_policy->OnCompletion());
  }
  if (!retry_policy->IsExhausted()) {
    // The last error cannot be retried, but it is not because the retry
    // policy is exhausted, we call these "permanent errors", and they
    // get a special message.
    return RetryLoopError("Permanent error in", location, last_status);
  }
  return RetryLoopError("Retry policy exhausted in", location, last_status);
}

/// @copydoc RetryLoopImpl
template <typename Functor, typename Request,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, grpc::ClientContext&, Request const&>::value,
              int>::type = 0>
auto RetryLoop(std::unique_ptr<RetryPolicy> retry_policy,
               std::unique_ptr<BackoffPolicy> backoff_policy,
               bool is_idempotent, Functor&& functor, Request const& request,
               char const* location)
    -> google::cloud::internal::invoke_result_t<Functor, grpc::ClientContext&,
                                                Request const&> {
  return RetryLoopImpl(
      std::move(retry_policy), std::move(backoff_policy), is_idempotent,
      std::forward<Functor>(functor), request, location,
      [](std::chrono::milliseconds p) { std::this_thread::sleep_for(p); });
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_RETRY_LOOP_H
