// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_LOG_WRAPPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_LOG_WRAPPER_H

#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, rest_internal::RestContext&, Request const&>>
Result LogWrapper(Functor&& functor, rest_internal::RestContext& context,
                  Request const& request, char const* where,
                  absl::string_view request_name,
                  absl::string_view response_name,
                  TracingOptions const& options) {
  auto formatter = [options](std::string* out, auto const& header) {
    const auto* delim = options.single_line_mode() ? "&" : "\n";
    absl::StrAppend(
        out, " { name: \"", header.first, "\" value: \"",
        internal::DebugString(absl::StrJoin(header.second, delim), options),
        "\" }");
  };
  GCP_LOG(DEBUG) << where << "() << "
                 << request.DebugString(request_name, options) << ", Context {"
                 << absl::StrJoin(context.headers(), "", formatter) << " }";

  auto response = functor(context, request);
  if (!response) {
    GCP_LOG(DEBUG) << where << "() >> status=" << response.status();
  } else {
    GCP_LOG(DEBUG) << where << "() >> response="
                   << response->DebugString(response_name, options);
  }

  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_LOG_WRAPPER_H
