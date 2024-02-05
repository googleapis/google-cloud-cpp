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

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/bigquery/v2/minimal/internal/rest_stub_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Projection Projection::Full() {
  Projection projection;
  projection.value = "FULL";
  return projection;
}

Projection Projection::Minimal() {
  Projection projection;
  projection.value = "MINIMAL";
  return projection;
}

StateFilter StateFilter::Running() {
  StateFilter state_filter;
  state_filter.value = "RUNNING";
  return state_filter;
}

StateFilter StateFilter::Pending() {
  StateFilter state_filter;
  state_filter.value = "PENDING";
  return state_filter;
}

StateFilter StateFilter::Done() {
  StateFilter state_filter;
  state_filter.value = "DONE";
  return state_filter;
}

std::string GetJobRequest::DebugString(absl::string_view name,
                                       TracingOptions const& options,
                                       int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .StringField("job_id", job_id_)
      .StringField("location", location_)
      .Build();
}

std::string ListJobsRequest::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .Field("all_users", all_users_)
      .Field("max_results", max_results_)
      .Field("min_creation_time", min_creation_time_)
      .Field("max_creation_time", max_creation_time_)
      .StringField("page_token", page_token_)
      .SubMessage("projection", projection_)
      .SubMessage("state_filter", state_filter_)
      .StringField("parent_job_id", parent_job_id_)
      .Build();
}

std::string InsertJobRequest::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .SubMessage("job", job_)
      .Build();
}

std::string Projection::DebugString(absl::string_view name,
                                    TracingOptions const& options,
                                    int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string StateFilter::DebugString(absl::string_view name,
                                     TracingOptions const& options,
                                     int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

ListJobsRequest::ListJobsRequest(std::string project_id)
    : project_id_(std::move(project_id)), all_users_(false), max_results_(0) {}

StatusOr<rest_internal::RestRequest> BuildRestRequest(GetJobRequest const& r) {
  auto const& opts = internal::CurrentOptions();

  rest_internal::RestRequest request;
  // Builds GetJob request path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);

  std::string path = absl::StrCat(endpoint, "/projects/", r.project_id(),
                                  "/jobs/", r.job_id());
  request.SetPath(std::move(path));

  // Add query params.
  if (!r.location().empty()) {
    request.AddQueryParameter("location", r.location());
  }
  return request;
}

auto TimePointToUnixMilliseconds(std::chrono::system_clock::time_point tp) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             tp - std::chrono::system_clock::from_time_t(0))
      .count();
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    ListJobsRequest const& r) {
  rest_internal::RestRequest request;
  auto const& opts = internal::CurrentOptions();
  // Builds GetJob request path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);

  std::string path =
      absl::StrCat(endpoint, "/projects/", r.project_id(), "/jobs");
  request.SetPath(std::move(path));

  // Add query params.
  if (r.all_users()) {
    request.AddQueryParameter("allUsers", "true");
  }
  if (r.max_results() > 0) {
    request.AddQueryParameter("maxResults", std::to_string(r.max_results()));
  }
  if (r.min_creation_time()) {
    request.AddQueryParameter(
        "minCreationTime",
        std::to_string(TimePointToUnixMilliseconds(*r.min_creation_time())));
  }
  if (r.max_creation_time()) {
    request.AddQueryParameter(
        "maxCreationTime",
        std::to_string(TimePointToUnixMilliseconds(*r.max_creation_time())));
  }

  auto if_not_empty_add = [&](char const* key, auto const& v) {
    if (v.empty()) return;
    request.AddQueryParameter(key, v);
  };
  if_not_empty_add("pageToken", r.page_token());
  if_not_empty_add("projection", r.projection().value);
  if_not_empty_add("stateFilter", r.state_filter().value);
  if_not_empty_add("parentJobId", r.parent_job_id());

  return request;
}

std::ostream& operator<<(std::ostream& os, GetJobRequest const& request) {
  os << "GetJobRequest{project_id=" << request.project_id()
     << ", job_id=" << request.job_id() << ", location=" << request.location();
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, ListJobsRequest const& request) {
  std::string all_users = request.all_users() ? "true" : "false";
  os << "ListJobsRequest{project_id=" << request.project_id()
     << ", all_users=" << all_users << ", max_results=" << request.max_results()
     << ", page_token=" << request.page_token()
     << ", projection=" << request.projection().value
     << ", state_filter=" << request.state_filter().value
     << ", parent_job_id=" << request.parent_job_id();
  return os << "}";
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    InsertJobRequest const& r) {
  auto const& opts = internal::CurrentOptions();

  rest_internal::RestRequest request;
  // Builds InsertJob request path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);
  std::string path =
      absl::StrCat(endpoint, "/projects/", r.project_id(), "/jobs");
  request.SetPath(std::move(path));

  return request;
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    CancelJobRequest const& r) {
  auto const& opts = internal::CurrentOptions();

  rest_internal::RestRequest request;
  // Builds CancelJob request path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);
  std::string path = absl::StrCat(endpoint, "/projects/", r.project_id(),
                                  "/jobs/", r.job_id(), "/cancel");
  request.SetPath(std::move(path));

  // Add query params.
  if (!r.location().empty()) {
    request.AddQueryParameter("location", r.location());
  }
  return request;
}

std::string CancelJobRequest::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .StringField("job_id", job_id_)
      .StringField("location", location_)
      .Build();
}

void to_json(nlohmann::json& j, PostQueryRequest const& q) {
  j = nlohmann::json{{"projectId", q.project_id()},
                     {"queryRequest", q.query_request()}};
}

void to_json(nlohmann::json& j, QueryRequest const& q) {
  j = nlohmann::json{
      {"query", q.query()},
      {"kind", q.kind()},
      {"parameterMode", q.parameter_mode()},
      {"location", q.location()},
      {"requestId", q.request_id()},
      {"dryRun", q.dry_run()},
      {"preserveNulls", q.preserve_nulls()},
      {"useQueryCache", q.use_query_cache()},
      {"useLegacySql", q.use_legacy_sql()},
      {"createSession", q.create_session()},
      {"maxResults", q.max_results()},
      {"maximumBytesBilled", std::to_string(q.maximum_bytes_billed())},
      {"connectionProperties", q.connection_properties()},
      {"queryParameters", q.query_parameters()},
      {"defaultDataset", q.default_dataset()},
      {"formatOptions", q.format_options()},
      {"labels", q.labels()}};

  ToIntJson(q.timeout(), j,
            "timeoutMs");  // timeoutMs value is a number for this request type.
}

void from_json(nlohmann::json const& j, PostQueryRequest& q) {
  SafeGetTo(j, "project_id", &PostQueryRequest::set_project_id, q);
  SafeGetTo(j, "queryRequest", &PostQueryRequest::set_query_request, q);
}

void from_json(nlohmann::json const& j, QueryRequest& q) {
  SafeGetTo(j, "query", &QueryRequest::set_query, q);
  SafeGetTo(j, "kind", &QueryRequest::set_kind, q);
  SafeGetTo(j, "parameterMode", &QueryRequest::set_parameter_mode, q);
  SafeGetTo(j, "location", &QueryRequest::set_location, q);
  SafeGetTo(j, "requestId", &QueryRequest::set_request_id, q);
  SafeGetTo(j, "dryRun", &QueryRequest::set_dry_run, q);
  SafeGetTo(j, "preserveNulls", &QueryRequest::set_preserve_nulls, q);
  SafeGetTo(j, "useQueryCache", &QueryRequest::set_use_query_cache, q);
  SafeGetTo(j, "useLegacySql", &QueryRequest::set_use_legacy_sql, q);
  SafeGetTo(j, "createSession", &QueryRequest::set_create_session, q);
  SafeGetTo(j, "maxResults", &QueryRequest::set_max_results, q);
  q.set_maximum_bytes_billed(GetNumberFromJson(j, "maximumBytesBilled"));
  SafeGetTo(j, "connectionProperties", &QueryRequest::set_connection_properties,
            q);
  SafeGetTo(j, "queryParameters", &QueryRequest::set_query_parameters, q);
  SafeGetTo(j, "defaultDataset", &QueryRequest::set_default_dataset, q);
  SafeGetTo(j, "formatOptions", &QueryRequest::set_format_options, q);
  SafeGetTo(j, "labels", &QueryRequest::set_labels, q);

  std::chrono::milliseconds timeout;
  FromJson(timeout, j, "timeoutMs");
  q.set_timeout(timeout);
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    PostQueryRequest const& r) {
  auto const& opts = internal::CurrentOptions();
  rest_internal::RestRequest request;

  // Builds PostQueryRequest path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);
  std::string path =
      absl::StrCat(endpoint, "/projects/", r.project_id(), "/queries");
  request.SetPath(std::move(path));

  return request;
}

std::string PostQueryRequest::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id())
      .SubMessage("query_request", query_request())
      .Build();
}

std::string DataFormatOptions::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("use_int64_timestamp", use_int64_timestamp)
      .Build();
}

std::string QueryRequest::DebugString(absl::string_view name,
                                      TracingOptions const& options,
                                      int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("query", query())
      .StringField("kind", kind())
      .StringField("parameter_mode", parameter_mode())
      .StringField("location", location())
      .StringField("request_id", request_id())
      .Field("dry_run", dry_run())
      .Field("preserver_nulls", preserve_nulls())
      .Field("use_query_cache", use_query_cache())
      .Field("use_legacy_sql", use_legacy_sql())
      .Field("create_session", create_session())
      .Field("max_results", max_results())
      .Field("maximum_bytes_billed", maximum_bytes_billed())
      .Field("timeout", timeout())
      .Field("connection_properties", connection_properties())
      .Field("query_parameters", query_parameters())
      .Field("labels", labels())
      .SubMessage("default_dataset", default_dataset())
      .SubMessage("format_options", format_options())
      .Build();
}

void to_json(nlohmann::json& j, GetQueryResultsRequest const& q) {
  j = nlohmann::json{
      {"projectId", q.project_id()},   {"jobId", q.job_id()},
      {"pageToken", q.page_token()},   {"location", q.location()},
      {"startIndex", q.start_index()}, {"maxResults", q.max_results()}};

  ToJson(q.timeout(), j, "timeoutMs");
}

void from_json(nlohmann::json const& j, GetQueryResultsRequest& q) {
  SafeGetTo(j, "projectId", &GetQueryResultsRequest::set_project_id, q);
  SafeGetTo(j, "jobId", &GetQueryResultsRequest::set_job_id, q);
  SafeGetTo(j, "pageToken", &GetQueryResultsRequest::set_page_token, q);
  SafeGetTo(j, "location", &GetQueryResultsRequest::set_location, q);
  SafeGetTo(j, "startIndex", &GetQueryResultsRequest::set_start_index, q);
  SafeGetTo(j, "maxResults", &GetQueryResultsRequest::set_max_results, q);

  std::chrono::milliseconds timeout;
  FromJson(timeout, j, "timeoutMs");
  q.set_timeout(timeout);
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    GetQueryResultsRequest const& r) {
  auto const& opts = internal::CurrentOptions();
  rest_internal::RestRequest request;

  // Builds GetQueryResultsRequest path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);
  std::string path = absl::StrCat(endpoint, "/projects/", r.project_id(),
                                  "/queries/", r.job_id());
  request.SetPath(std::move(path));

  // Add query params.
  // https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/getQueryResults#query-parameters
  auto if_not_empty_add = [&](char const* key, auto const& v) {
    if (v.empty()) return;
    request.AddQueryParameter(key, v);
  };
  if_not_empty_add("pageToken", r.page_token());
  if_not_empty_add("location", r.location());

  request.AddQueryParameter("startIndex", std::to_string(r.start_index()));
  if (r.max_results() > 0) {
    request.AddQueryParameter("maxResults", std::to_string(r.max_results()));
  }
  if (r.timeout() > std::chrono::milliseconds(0)) {
    request.AddQueryParameter("timeoutMs", std::to_string(r.timeout().count()));
  }

  return request;
}

std::string GetQueryResultsRequest::DebugString(absl::string_view name,
                                                TracingOptions const& options,
                                                int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id())
      .StringField("job_id", job_id())
      .StringField("page_token", page_token())
      .StringField("location", location())
      .Field("start_index", start_index())
      .Field("max_results", max_results())
      .Field("timeout", timeout())
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
