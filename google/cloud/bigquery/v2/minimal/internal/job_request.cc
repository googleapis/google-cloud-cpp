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
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"

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
  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Project Id is empty", GCP_ERROR_INFO());
  }
  if (r.job_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Job Id is empty", GCP_ERROR_INFO());
  }
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

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    ListJobsRequest const& r) {
  rest_internal::RestRequest request;
  auto const& opts = internal::CurrentOptions();

  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid ListJobsRequest: Project Id is empty", GCP_ERROR_INFO());
  }
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
    request.AddQueryParameter("minCreationTime",
                              internal::FormatRfc3339(*r.min_creation_time()));
  }
  if (r.max_creation_time()) {
    request.AddQueryParameter("maxCreationTime",
                              internal::FormatRfc3339(*r.max_creation_time()));
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
  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid InsertJobRequest: Project Id is empty", GCP_ERROR_INFO());
  }

  // Builds InsertJob request path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);
  std::string path =
      absl::StrCat(endpoint, "/projects/", r.project_id(), "/jobs");
  request.SetPath(std::move(path));

  // Validate request body is a valid json Job payload.
  nlohmann::json json_payload;
  to_json(json_payload, r.job());

  if (!json_payload.is_object()) {
    return internal::InvalidArgumentError(
        "Invalid InsertJobRequest: Invalid json payload", GCP_ERROR_INFO());
  }

  auto const& job = json_payload.get<Job>();

  if (job.configuration.job_type.empty() || job.id != r.job().id) {
    return internal::InvalidArgumentError(
        "Invalid InsertJobRequest: Invalid Job object", GCP_ERROR_INFO());
  }

  return request;
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    CancelJobRequest const& r) {
  auto const& opts = internal::CurrentOptions();

  rest_internal::RestRequest request;
  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid CancelJobRequest: Project Id is empty", GCP_ERROR_INFO());
  }
  if (r.job_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid CancelJobRequest: Job Id is empty", GCP_ERROR_INFO());
  }

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
  j = nlohmann::json{{"project_id", q.project_id()},
                     {"query_request", q.query_request()}};
}

void to_json(nlohmann::json& j, QueryRequest const& q) {
  j = nlohmann::json{{"query", q.query()},
                     {"kind", q.kind()},
                     {"parameter_mode", q.parameter_mode()},
                     {"location", q.location()},
                     {"request_id", q.request_id()},
                     {"dry_run", q.dry_run()},
                     {"preserve_nulls", q.preserve_nulls()},
                     {"use_query_cache", q.use_query_cache()},
                     {"use_legacy_sql", q.use_legacy_sql()},
                     {"create_session", q.create_session()},
                     {"max_results", q.max_results()},
                     {"maximum_bytes_billed", q.maximum_bytes_billed()},
                     {"connection_properties", q.connection_properties()},
                     {"query_parameters", q.query_parameters()},
                     {"default_dataset", q.default_dataset()},
                     {"format_options", q.format_options()},
                     {"labels", q.labels()}};

  ToJson(q.timeout(), j, "timeout");
}

void from_json(nlohmann::json const& j, PostQueryRequest& q) {
  if (j.contains("project_id")) {
    q.set_project_id(j.at("project_id").value("project_id", ""));
  }
  if (j.contains("query_request")) {
    q.set_query_request(j.at("query_request").get<QueryRequest>());
  }
}

void from_json(nlohmann::json const& j, QueryRequest& q) {
  if (j.contains("query")) q.set_query(j.at("query").get<std::string>());
  if (j.contains("kind")) q.set_kind(j.at("kind").get<std::string>());
  if (j.contains("parameter_mode")) {
    q.set_parameter_mode(j.at("parameter_mode").get<std::string>());
  }
  if (j.contains("location")) {
    q.set_location(j.at("location").get<std::string>());
  }
  if (j.contains("request_id")) {
    q.set_request_id(j.at("request_id").get<std::string>());
  }
  if (j.contains("dry_run")) q.set_dry_run(j.at("dry_run").get<bool>());
  if (j.contains("preserve_nulls")) {
    q.set_preserve_nulls(j.at("preserve_nulls").get<bool>());
  }
  if (j.contains("use_query_cache")) {
    q.set_use_query_cache(j.at("use_query_cache").get<bool>());
  }
  if (j.contains("use_legacy_sql")) {
    q.set_use_legacy_sql(j.at("use_legacy_sql").get<bool>());
  }
  if (j.contains("create_session")) {
    q.set_create_session(j.at("create_session").get<bool>());
  }
  if (j.contains("max_results")) {
    q.set_max_results(j.at("max_results").get<std::uint32_t>());
  }
  if (j.contains("maximum_bytes_billed")) {
    q.set_maximum_bytes_billed(
        j.at("maximum_bytes_billed").get<std::int64_t>());
  }
  if (j.contains("connection_properties")) {
    q.set_connection_properties(
        j.at("connection_properties").get<std::vector<ConnectionProperty>>());
  }
  if (j.contains("query_parameters")) {
    q.set_query_parameters(
        j.at("query_parameters").get<std::vector<QueryParameter>>());
  }
  if (j.contains("default_dataset")) {
    q.set_default_dataset(j.at("default_dataset").get<DatasetReference>());
  }
  if (j.contains("format_options")) {
    q.set_format_options(j.at("format_options").get<DataFormatOptions>());
  }
  if (j.contains("labels")) {
    q.set_labels(j.at("labels").get<std::map<std::string, std::string>>());
  }

  std::chrono::milliseconds timeout;
  FromJson(timeout, j, "timeout");
  q.set_timeout(timeout);
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    PostQueryRequest const& r) {
  auto const& opts = internal::CurrentOptions();
  rest_internal::RestRequest request;

  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid PostQueryRequest: Project Id is empty", GCP_ERROR_INFO());
  }

  // Builds PostQueryRequest path based on endpoint provided.
  std::string endpoint = GetBaseEndpoint(opts);
  std::string path =
      absl::StrCat(endpoint, "/projects/", r.project_id(), "/queries");
  request.SetPath(std::move(path));

  // Validate request body is a valid json QueryRequest payload.
  nlohmann::json json_payload;
  to_json(json_payload, r.query_request());

  if (!json_payload.is_object()) {
    return internal::InvalidArgumentError(
        "Invalid PostQueryRequest: Invalid json payload", GCP_ERROR_INFO());
  }

  auto const& query_request = json_payload.get<QueryRequest>();

  if (query_request.query().empty()) {
    return internal::InvalidArgumentError(
        "Invalid PostQueryRequest: Missing required query field",
        GCP_ERROR_INFO());
  }

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
      .Field("maximum_bytes_biller", maximum_bytes_billed())
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
      {"project_id", q.project_id()},        {"job_id", q.job_id()},
      {"page_token", q.page_token()},        {"location", q.location()},
      {"start_index", q.start_index()},      {"max_results", q.max_results()},
      {"format_options", q.format_options()}};

  ToJson(q.timeout(), j, "timeout");
}

void from_json(nlohmann::json const& j, GetQueryResultsRequest& q) {
  if (j.contains("project_id")) {
    q.set_project_id(j.at("project_id").value("project_id", ""));
  }
  if (j.contains("job_id")) {
    q.set_job_id(j.at("job_id").value("job_id", ""));
  }
  if (j.contains("page_token")) {
    q.set_page_token(j.at("page_token").value("page_token", ""));
  }
  if (j.contains("location")) {
    q.set_location(j.at("location").value("location", ""));
  }
  if (j.contains("start_index")) {
    q.set_start_index(j.at("start_index").get<std::uint64_t>());
  }
  if (j.contains("max_results")) {
    q.set_max_results(j.at("max_results").get<std::uint32_t>());
  }
  if (j.contains("format_options")) {
    q.set_format_options(j.at("format_options").get<DataFormatOptions>());
  }

  std::chrono::milliseconds timeout;
  FromJson(timeout, j, "timeout");
  q.set_timeout(timeout);
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    GetQueryResultsRequest const& r) {
  auto const& opts = internal::CurrentOptions();
  rest_internal::RestRequest request;

  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetQueryResultsRequest: Project Id is empty",
        GCP_ERROR_INFO());
  }
  if (r.job_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetQueryResultsRequest: Job Id is empty", GCP_ERROR_INFO());
  }

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

  std::string format_options;
  if (r.format_options().use_int64_timestamp) {
    absl::StrAppend(&format_options, "{\"useInt64Timestamp\":true}");
  } else {
    absl::StrAppend(&format_options, "{\"useInt64Timestamp\":false}");
  }
  request.AddQueryParameter("formatOptions", format_options);

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
      .SubMessage("format_options", format_options())
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
