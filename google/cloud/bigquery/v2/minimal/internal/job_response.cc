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

#include "google/cloud/bigquery/v2/minimal/internal/job_response.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

bool valid_job(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("id") &&
          j.contains("status") && j.contains("jobReference") &&
          j.contains("configuration"));
}

bool valid_list_format_job(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("state") && j.contains("id") &&
          j.contains("jobReference"));
}

bool valid_jobs_list(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("jobs"));
}

StatusOr<nlohmann::json> parse_json(std::string const& payload) {
  if (payload.empty()) {
    return internal::InternalError("Empty payload in HTTP response",
                                   GCP_ERROR_INFO());
  }
  // Build the job response object from Http response.
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError("Error parsing Json from response payload",
                                   GCP_ERROR_INFO());
  }

  return json;
}

}  // namespace

StatusOr<GetJobResponse> GetJobResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_job(*json)) {
    return internal::InternalError("Not a valid Json Job object",
                                   GCP_ERROR_INFO());
  }

  GetJobResponse result;
  result.job = json->get<Job>();
  result.http_response = http_response;

  return result;
}

std::string GetJobResponse::DebugString(absl::string_view name,
                                        TracingOptions const& options,
                                        int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("http_response", http_response)
      .SubMessage("job", job)
      .Build();
}

StatusOr<ListJobsResponse> ListJobsResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_jobs_list(*json)) {
    return internal::InternalError("Not a valid Json JobList object",
                                   GCP_ERROR_INFO());
  }

  ListJobsResponse result;
  result.http_response = http_response;

  result.kind = json->value("kind", "");
  result.etag = json->value("etag", "");
  result.next_page_token = json->value("nextPageToken", "");

  for (auto const& kv : json->at("jobs").items()) {
    auto const& json_list_format_job_obj = kv.value();
    if (!valid_list_format_job(json_list_format_job_obj)) {
      return internal::InternalError("Not a valid Json ListFormatJob object",
                                     GCP_ERROR_INFO());
    }
    auto const& list_format_job = json_list_format_job_obj.get<ListFormatJob>();
    result.jobs.push_back(list_format_job);
  }

  return result;
}

std::string ListJobsResponse::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("jobs", jobs)
      .StringField("next_page_token", next_page_token)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .SubMessage("http_response", http_response)
      .Build();
}

StatusOr<InsertJobResponse> InsertJobResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_job(*json)) {
    return internal::InternalError("Not a valid Json Job object",
                                   GCP_ERROR_INFO());
  }

  InsertJobResponse result;
  result.job = json->get<Job>();
  result.http_response = http_response;

  return result;
}

StatusOr<CancelJobResponse> CancelJobResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!json->contains("job")) {
    return internal::InternalError("Not a valid CancelJobResponse object",
                                   GCP_ERROR_INFO());
  }

  auto const& json_job_obj = json->at("job");
  if (!valid_job(json_job_obj)) {
    return internal::InternalError("Not a valid Json Job object",
                                   GCP_ERROR_INFO());
  }

  CancelJobResponse result;
  result.kind = json->value("kind", "");
  result.http_response = http_response;
  result.job = json_job_obj.get<Job>();

  return result;
}

std::string InsertJobResponse::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("http_response", http_response)
      .SubMessage("job", job)
      .Build();
}

std::string CancelJobResponse::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .SubMessage("http_response", http_response)
      .SubMessage("job", job)
      .Build();
}

std::string PostQueryResults::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("page_token", page_token)
      .Field("total_rows", total_rows)
      .Field("total_bytes_processed", total_bytes_processed)
      .Field("num_dml_affected_rows", num_dml_affected_rows)
      .Field("job_complete", job_complete)
      .Field("cache_hit", cache_hit)
      .Field("rows", rows)
      .Field("errors", errors)
      .SubMessage("schema", schema)
      .SubMessage("job_reference", job_reference)
      .SubMessage("session_info", session_info)
      .SubMessage("dml_stats", dml_stats)
      .Build();
}

std::string QueryResponse::DebugString(absl::string_view name,
                                       TracingOptions const& options,
                                       int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("http_response", http_response)
      .SubMessage("query_results", post_query_results)
      .Build();
}

StatusOr<QueryResponse> QueryResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  PostQueryResults query_results;
  query_results.kind = json->value("kind", "");
  query_results.page_token = json->value("pageToken", "");
  // May not be present in certain query scenarios (e.g in dry-run mode).
  if (json->contains("totalRows")) {
    query_results.total_rows =
        static_cast<std::uint64_t>(GetNumberFromJson(*json, "totalRows"));
  }
  query_results.total_bytes_processed =
      GetNumberFromJson(*json, "totalBytesProcessed");
  query_results.num_dml_affected_rows =
      GetNumberFromJson(*json, "numDmlAffectedRows");

  SafeGetTo(query_results.job_complete, *json, "jobComplete");
  SafeGetTo(query_results.cache_hit, *json, "cacheHit");
  SafeGetTo(query_results.schema, *json, "schema");
  SafeGetTo(query_results.job_reference, *json, "jobReference");

  if (json->contains("rows")) {
    for (auto const& kv : json->at("rows").items()) {
      auto const& json_struct_obj = kv.value();
      auto const& row = json_struct_obj.get<Struct>();
      query_results.rows.push_back(row);
    }
  }

  if (json->contains("errors")) {
    for (auto const& kv : json->at("errors").items()) {
      auto const& json_error_proto_obj = kv.value();
      auto const& error = json_error_proto_obj.get<ErrorProto>();
      query_results.errors.push_back(error);
    }
  }

  SafeGetTo(query_results.session_info, *json, "sessionInfo");
  SafeGetTo(query_results.dml_stats, *json, "dmlStats");

  QueryResponse response;
  response.http_response = http_response;
  response.post_query_results = query_results;

  return response;
}

std::string SessionInfo::DebugString(absl::string_view name,
                                     TracingOptions const& options,
                                     int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("session_id", session_id)
      .Build();
}

void to_json(nlohmann::json& j, SessionInfo const& s) {
  j = nlohmann::json{{"sessionId", s.session_id}};
}
void from_json(nlohmann::json const& j, SessionInfo& s) {
  SafeGetTo(s.session_id, j, "sessionId");
}

void to_json(nlohmann::json& j, PostQueryResults const& q) {
  j = nlohmann::json{{"kind", q.kind},
                     {"pageToken", q.page_token},
                     {"totalRows", q.total_rows},
                     {"totalBytesProcessed", q.total_bytes_processed},
                     {"numDmlAffectedRows", q.num_dml_affected_rows},
                     {"jobComplete", q.job_complete},
                     {"cacheHit", q.cache_hit},
                     {"schema", q.schema},
                     {"jobReference", q.job_reference},
                     {"rows", q.rows},
                     {"errors", q.errors},
                     {"sessionInfo", q.session_info},
                     {"dmlStats", q.dml_stats}};
}

void from_json(nlohmann::json const& j, PostQueryResults& q) {
  SafeGetTo(q.kind, j, "kind");
  SafeGetTo(q.page_token, j, "pageToken");
  SafeGetTo(q.total_rows, j, "totalRows");
  SafeGetTo(q.total_bytes_processed, j, "totalBytesProcessed");
  SafeGetTo(q.num_dml_affected_rows, j, "numDmlAffectedRows");
  SafeGetTo(q.job_complete, j, "jobComplete");
  SafeGetTo(q.cache_hit, j, "cacheHit");
  SafeGetTo(q.schema, j, "schema");
  SafeGetTo(q.job_reference, j, "jobReference");
  SafeGetTo(q.rows, j, "rows");
  SafeGetTo(q.errors, j, "errors");
  SafeGetTo(q.session_info, j, "sessionInfo");
  SafeGetTo(q.dml_stats, j, "dmlStats");
}

void to_json(nlohmann::json& j, GetQueryResults const& q) {
  j = nlohmann::json{{"kind", q.kind},
                     {"etag", q.etag},
                     {"pageToken", q.page_token},
                     {"totalRows", q.total_rows},
                     {"totalBytesProcessed", q.total_bytes_processed},
                     {"numDmlAffectedRows", q.num_dml_affected_rows},
                     {"jobComplete", q.job_complete},
                     {"cacheHit", q.cache_hit},
                     {"schema", q.schema},
                     {"jobReference", q.job_reference},
                     {"rows", q.rows},
                     {"errors", q.errors}};
}
void from_json(nlohmann::json const& j, GetQueryResults& q) {
  SafeGetTo(q.kind, j, "kind");
  SafeGetTo(q.etag, j, "etag");
  SafeGetTo(q.page_token, j, "pageToken");
  SafeGetTo(q.total_rows, j, "totalRows");
  SafeGetTo(q.total_bytes_processed, j, "totalBytesProcessed");
  SafeGetTo(q.num_dml_affected_rows, j, "numDmlAffectedRows");
  SafeGetTo(q.job_complete, j, "jobComplete");
  SafeGetTo(q.cache_hit, j, "cacheHit");
  SafeGetTo(q.schema, j, "schema");
  SafeGetTo(q.job_reference, j, "jobReference");
  SafeGetTo(q.rows, j, "rows");
  SafeGetTo(q.errors, j, "errors");
}

std::string GetQueryResults::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .StringField("page_token", page_token)
      .Field("total_rows", total_rows)
      .Field("total_bytes_processed", total_bytes_processed)
      .Field("num_dml_affected_rows", num_dml_affected_rows)
      .Field("job_complete", job_complete)
      .Field("cache_hit", cache_hit)
      .Field("rows", rows)
      .Field("errors", errors)
      .SubMessage("schema", schema)
      .SubMessage("job_reference", job_reference)
      .Build();
}

std::string GetQueryResultsResponse::DebugString(absl::string_view name,
                                                 TracingOptions const& options,
                                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("http_response", http_response)
      .SubMessage("get_query_results", get_query_results)
      .Build();
}

StatusOr<GetQueryResultsResponse>
GetQueryResultsResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  GetQueryResults get_query_results;
  get_query_results.kind = json->value("kind", "");
  get_query_results.etag = json->value("etag", "");
  get_query_results.page_token = json->value("pageToken", "");
  if (json->contains("totalRows")) {
    get_query_results.total_rows =
        static_cast<std::uint64_t>(GetNumberFromJson(*json, "totalRows"));
  }
  get_query_results.total_bytes_processed =
      GetNumberFromJson(*json, "totalBytesProcessed");
  get_query_results.num_dml_affected_rows =
      GetNumberFromJson(*json, "numDmlAffectedRows");

  SafeGetTo(get_query_results.job_complete, *json, "jobComplete");
  SafeGetTo(get_query_results.cache_hit, *json, "cacheHit");
  SafeGetTo(get_query_results.schema, *json, "schema");
  SafeGetTo(get_query_results.job_reference, *json, "jobReference");

  if (json->contains("rows")) {
    for (auto const& kv : json->at("rows").items()) {
      auto const& json_struct_obj = kv.value();
      auto const& row = json_struct_obj.get<Struct>();
      get_query_results.rows.push_back(row);
    }
  }
  if (json->contains("errors")) {
    for (auto const& kv : json->at("errors").items()) {
      auto const& json_error_proto_obj = kv.value();
      auto const& error = json_error_proto_obj.get<ErrorProto>();
      get_query_results.errors.push_back(error);
    }
  }

  GetQueryResultsResponse response;
  response.http_response = http_response;
  response.get_query_results = get_query_results;

  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
