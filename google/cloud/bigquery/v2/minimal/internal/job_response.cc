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
          j.contains("status") && j.contains("reference") &&
          j.contains("configuration"));
}

bool valid_list_format_job(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("state") && j.contains("id") &&
          j.contains("reference") && j.contains("configuration"));
}

bool valid_jobs_list(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") &&
          j.contains("next_page_token") && j.contains("jobs"));
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
  result.next_page_token = json->value("next_page_token", "");

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

std::string SessionInfo::DebugString(absl::string_view name,
                                     TracingOptions const& options,
                                     int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("session_id", session_id)
      .Build();
}

std::string QueryResults::DebugString(absl::string_view name,
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
      .SubMessage("query_results", query_results)
      .Build();
}

StatusOr<QueryResponse> QueryResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  QueryResults query_results;
  query_results.kind = json->value("kind", "");
  query_results.page_token = json->value("page_token", "");
  query_results.total_rows = json->at("total_rows").get<std::uint64_t>();
  query_results.total_bytes_processed =
      json->at("total_bytes_processed").get<std::int64_t>();
  query_results.num_dml_affected_rows =
      json->at("num_dml_affected_rows").get<std::int64_t>();

  query_results.job_complete = json->at("job_complete").get<bool>();
  query_results.cache_hit = json->at("cache_hit").get<bool>();

  query_results.schema = json->at("schema").get<TableSchema>();
  query_results.job_reference = json->at("job_reference").get<JobReference>();

  for (auto const& kv : json->at("rows").items()) {
    auto const& json_struct_obj = kv.value();
    auto const& row = json_struct_obj.get<Struct>();
    query_results.rows.push_back(row);
  }

  for (auto const& kv : json->at("errors").items()) {
    auto const& json_error_proto_obj = kv.value();
    auto const& error = json_error_proto_obj.get<ErrorProto>();
    query_results.errors.push_back(error);
  }

  query_results.session_info = json->at("session_info").get<SessionInfo>();
  query_results.dml_stats = json->at("dml_stats").get<DmlStats>();

  QueryResponse response;
  response.http_response = http_response;
  response.query_results = query_results;

  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
