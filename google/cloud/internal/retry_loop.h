// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_LOOP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_LOOP_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/retry_loop_helpers.h"
#include "google/cloud/retry_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A generic retry loop for gRPC operations.
 *
 * This function implements a retry loop suitable for *most* gRPC operations.
 *
 * @param retry_policy controls the duration of the retry loop.
 * @param backoff_policy controls how the loop backsoff from a recoverable
 *     failure.
 * @param idempotency if `Idempotency::kNonIdempotent`, the operation is not
 *     retried even on transient errors.
 * @param functor the operation to retry, typically a lambda that encapsulates
 *     both the Stub and the function to call.
 * @param context the gRPC context used for the request, previous Stubs in the
 *     stack can set timeouts and metadata through this context.
 * @param options the google::cloud::Options in effect for this call. Typically
 *     the `*ConnectionImpl` class will get these from
 *     `google::cloud::internal::CurrentOptions`.
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
          std::enable_if_t<google::cloud::internal::is_invocable<
                               Functor, grpc::ClientContext&, Options const&,
                               Request const&>::value,
                           int> = 0>
auto RetryLoopImpl(RetryPolicy& retry_policy, BackoffPolicy& backoff_policy,
                   Idempotency idempotency, Functor&& functor,
                   Options const& options, Request const& request,
                   char const* location, Sleeper sleeper)
    -> google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Options const&, Request const&> {
  auto const enable_server_retries = options.get<EnableServerRetriesOption>();
  auto last_status = Status{};
  while (!retry_policy.IsExhausted()) {
    // Need to create a new context for each retry.
    grpc::ClientContext context;
    ConfigureContext(context, options);
    auto result = functor(context, options, request);
    if (result.ok()) return result;
    last_status = GetResultStatus(std::move(result));
    auto delay = Backoff(last_status, location, retry_policy, backoff_policy,
                         idempotency, enable_server_retries);
    if (!delay) return std::move(delay).status();
    sleeper(*delay);
  }
  return internal::RetryLoopError(last_status, location,
                                  retry_policy.IsExhausted());
}

/// @copydoc RetryLoopImpl
template <typename Functor, typename Request,
          std::enable_if_t<google::cloud::internal::is_invocable<
                               Functor, grpc::ClientContext&, Options const&,
                               Request const&>::value,
                           int> = 0>
auto RetryLoop(std::unique_ptr<RetryPolicy> retry_policy,
               std::unique_ptr<BackoffPolicy> backoff_policy,
               Idempotency idempotency, Functor&& functor,
               Options const& options, Request const& request,
               char const* location)
    -> google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Options const&, Request const&> {
  std::function<void(std::chrono::milliseconds)> sleeper =
      [](std::chrono::milliseconds p) { std::this_thread::sleep_for(p); };
  sleeper = MakeTracedSleeper(options, std::move(sleeper), "Backoff");
  return RetryLoopImpl(*retry_policy, *backoff_policy, idempotency,
                       std::forward<Functor>(functor), options, request,
                       location, std::move(sleeper));
}

/// @copydoc RetryLoopImpl
template <
    typename Functor, typename Request,
    std::enable_if_t<google::cloud::internal::is_invocable<
                         Functor, grpc::ClientContext&, Request const&>::value,
                     int> = 0>
auto RetryLoop(std::unique_ptr<RetryPolicy> retry_policy,
               std::unique_ptr<BackoffPolicy> backoff_policy,
               Idempotency idempotency, Functor&& functor,
               Request const& request, char const* location)
    -> google::cloud::internal::invoke_result_t<Functor, grpc::ClientContext&,
                                                Request const&> {
  auto wrapper = [&functor](grpc::ClientContext& context, Options const&,
                            Request const& request) {
    return functor(context, request);
  };
  return RetryLoop(std::move(retry_policy), std::move(backoff_policy),
                   idempotency, std::move(wrapper), CurrentOptions(), request,
                   location);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_LOOP_H
