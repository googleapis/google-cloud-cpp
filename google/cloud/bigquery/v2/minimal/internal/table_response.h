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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_RESPONSE_H

#include "google/cloud/bigquery/v2/minimal/internal/bigquery_http_response.h"
#include "google/cloud/bigquery/v2/minimal/internal/table.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Parses the BigQueryHttpResponse and builds a GetTableResponse.
class GetTableResponse {
 public:
  GetTableResponse() = default;
  // Builds GetTableResponse from HttpResponse.
  static StatusOr<GetTableResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  Table table;
  BigQueryHttpResponse http_response;
};

// Parses the BigQueryHttpResponse and builds a ListTablesResponse.
class ListTablesResponse {
 public:
  ListTablesResponse() = default;
  // Builds ListTablesResponse from HttpResponse.
  static StatusOr<ListTablesResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  std::vector<ListFormatTable> tables;
  std::string next_page_token;
  std::string kind;
  std::string etag;
  std::int32_t total_items;

  BigQueryHttpResponse http_response;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_RESPONSE_H
