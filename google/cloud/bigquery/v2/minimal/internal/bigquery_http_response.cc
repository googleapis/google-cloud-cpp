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

#include "google/cloud/bigquery/v2/minimal/internal/bigquery_http_response.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace rest = ::google::cloud::rest_internal;

StatusOr<BigQueryHttpResponse> BigQueryHttpResponse::BuildFromRestResponse(
    std::unique_ptr<rest::RestResponse> rest_response) {
  BigQueryHttpResponse response;
  if (rest_response == nullptr) {
    return internal::InvalidArgumentError(
        "RestResponse argument passed in is null", GCP_ERROR_INFO());
  }
  if (rest::IsHttpError(*rest_response)) {
    return rest::AsStatus(std::move(*rest_response));
  }
  response.http_status_code = rest_response->StatusCode();
  response.http_headers = rest_response->Headers();

  auto payload = rest::ReadAll(std::move(*rest_response).ExtractPayload());
  if (!payload) return std::move(payload).status();

  response.payload = std::move(*payload);
  return response;
}

std::string BigQueryHttpResponse::DebugString(absl::string_view name,
                                              TracingOptions const& options,
                                              int indent) const {
  // Payload is not logged as it might contain user sensitive data like
  // ldap/emails.
  return internal::DebugFormatter(name, options, indent)
      .Field("status_code", http_status_code)
      .Field("http_headers", http_headers)
      .Field("payload", "REDACTED")
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
