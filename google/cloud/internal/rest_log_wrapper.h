// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_LOG_WRAPPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_LOG_WRAPPER_H

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/log_wrapper_helpers.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename T>
struct IsStatusOr : public std::false_type {};
template <typename T>
struct IsStatusOr<StatusOr<T>> : public std::true_type {};

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, rest_internal::RestContext&, Request const&>,
    typename std::enable_if<std::is_same<Result, google::cloud::Status>::value,
                            int>::type = 0>
Result RestLogWrapper(Functor&& functor,
                      rest_internal::RestContext& rest_request,
                      Request const& request, char const* where,
                      TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(rest_request, request);
  GCP_LOG(DEBUG) << where << "() >> status=" << DebugString(response, options);
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, rest_internal::RestContext&, Request const&>,
          typename std::enable_if<IsStatusOr<Result>::value, int>::type = 0>
Result RestLogWrapper(Functor&& functor,
                      rest_internal::RestContext& rest_request,
                      Request const& request, char const* where,
                      TracingOptions const& options) {
  GCP_LOG(DEBUG) << where << "() << " << DebugString(request, options);
  auto response = functor(rest_request, request);
  if (!response) {
    GCP_LOG(DEBUG) << where << "() >> status="
                   << DebugString(response.status(), options);
  } else {
    GCP_LOG(DEBUG) << where
                   << "() >> response=" << DebugString(*response, options);
  }
  return response;
}
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_LOG_WRAPPER_H
