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
        "Invalid GetJobRequest: Project Id is empty", GCP_ERROR_INFO());
  }
  if (r.job_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Job Id is empty", GCP_ERROR_INFO());
  }
  // Builds CancelJob request path based on endpoint provided.
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

std::string CancelJobRequest::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .StringField("job_id", job_id_)
      .StringField("location", location_)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
