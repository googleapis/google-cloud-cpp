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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_REST_STUB_UTILS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_REST_STUB_UTILS_H

#include "google/cloud/bigquery/v2/minimal/internal/bigquery_http_response.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename ReturnType>
StatusOr<ReturnType> ParseFromRestResponse(
    StatusOr<std::unique_ptr<rest_internal::RestResponse>> rest_response) {
  if (!rest_response) return std::move(rest_response).status();

  auto http_response =
      BigQueryHttpResponse::BuildFromRestResponse(std::move(*rest_response));
  if (!http_response) return std::move(http_response).status();

  return ReturnType::BuildFromHttpResponse(*http_response);
}

template <typename RequestType>
StatusOr<rest_internal::RestRequest> PrepareRestRequest(
    rest_internal::RestContext& rest_context, RequestType const& request) {
  auto rest_request = BuildRestRequest(request);
  if (!rest_request) return std::move(rest_request).status();

  // Copy over any headers if provided.
  if (!rest_context.headers().empty()) {
    for (auto const& h : rest_context.headers()) {
      if (!h.second.empty()) {
        rest_request->AddHeader(h.first, absl::StrJoin(h.second, "&"));
      }
    }
  }

  return rest_request;
}

std::string GetBaseEndpoint(Options const& opts);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_REST_STUB_UTILS_H
