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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_RESPONSE_H

#include "google/cloud/bigquery/v2/minimal/internal/bigquery_http_response.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_schema.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Parses the BigQueryHttpResponse and builds a GetJobResponse.
class GetJobResponse {
 public:
  GetJobResponse() = default;
  // Builds GetJobResponse from HttpResponse.
  static StatusOr<GetJobResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  Job job;
  BigQueryHttpResponse http_response;
};

// Parses the BigQueryHttpResponse and builds a ListJobsResponse.
class ListJobsResponse {
 public:
  ListJobsResponse() = default;
  // Builds ListJobsResponse from HttpResponse.
  static StatusOr<ListJobsResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  std::vector<ListFormatJob> jobs;
  std::string next_page_token;
  std::string kind;
  std::string etag;

  BigQueryHttpResponse http_response;
};

class InsertJobResponse {
 public:
  InsertJobResponse() = default;
  static StatusOr<InsertJobResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  Job job;
  BigQueryHttpResponse http_response;
};

class CancelJobResponse {
 public:
  CancelJobResponse() = default;
  static StatusOr<CancelJobResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  std::string kind;
  Job job;

  BigQueryHttpResponse http_response;
};

struct PostQueryResults {
  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  std::string kind;
  std::string page_token;

  std::uint64_t total_rows = 0;
  std::int64_t total_bytes_processed = 0;
  std::int64_t num_dml_affected_rows = 0;

  bool job_complete = false;
  bool cache_hit = false;

  TableSchema schema;
  JobReference job_reference;
  std::vector<Struct> rows;
  std::vector<ErrorProto> errors;
  SessionInfo session_info;
  DmlStats dml_stats;
};
void to_json(nlohmann::json& j, PostQueryResults const& q);
void from_json(nlohmann::json const& j, PostQueryResults& q);

class QueryResponse {
 public:
  QueryResponse() = default;
  static StatusOr<QueryResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  PostQueryResults post_query_results;

  BigQueryHttpResponse http_response;
};

struct GetQueryResults {
  std::string kind;
  std::string etag;
  std::string page_token;

  TableSchema schema;
  JobReference job_reference;

  std::int64_t total_bytes_processed = 0;
  std::uint64_t total_rows = 0;
  std::int64_t num_dml_affected_rows = 0;

  bool job_complete = false;
  bool cache_hit = false;

  std::vector<Struct> rows;
  std::vector<ErrorProto> errors;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, GetQueryResults const& q);
void from_json(nlohmann::json const& j, GetQueryResults& q);

class GetQueryResultsResponse {
 public:
  GetQueryResultsResponse() = default;
  static StatusOr<GetQueryResultsResponse> BuildFromHttpResponse(
      BigQueryHttpResponse const& http_response);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  GetQueryResults get_query_results;

  BigQueryHttpResponse http_response;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_RESPONSE_H
