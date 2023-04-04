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

namespace {

auto const kDefaultTimepoint = std::chrono::system_clock::time_point{};

std::string DebugStringSeparator(bool single_line_mode, int indent) {
  if (single_line_mode) return " ";
  return absl::StrCat("\n", std::string(indent * 2, ' '));
}

std::string GetJobsEndpoint(Options const& opts) {
  std::string endpoint = opts.get<EndpointOption>();

  if (!absl::StartsWith(endpoint, "https://") &&
      !absl::StartsWith(endpoint, "http://")) {
    endpoint = absl::StrCat("https://", endpoint);
  }
  if (!absl::EndsWith(endpoint, "/")) absl::StrAppend(&endpoint, "/");
  absl::StrAppend(&endpoint, "bigquery/v2/projects/");

  return endpoint;
}

}  // namespace

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

std::string GetJobRequest::DebugString(TracingOptions const& options) const {
  auto sep = [single_line_mode = options.single_line_mode()](int indent) {
    return DebugStringSeparator(single_line_mode, indent);
  };
  auto quoted = [&options](std::string const& s) {
    return absl::StrCat("\"", internal::DebugString(s, options), "\"");
  };
  return absl::StrCat(
      "google::cloud::bigquery_v2_minimal_internal::GetJobRequest {",  //
      sep(1), "project_id: ", quoted(project_id_),                     //
      sep(1), "job_id: ", quoted(job_id_),                             //
      sep(1), "location: ", quoted(location_),                         //
      sep(0), "}");
}

std::string ListJobsRequest::DebugString(TracingOptions const& options) const {
  auto sep = [single_line_mode = options.single_line_mode()](int indent) {
    return DebugStringSeparator(single_line_mode, indent);
  };
  auto quoted = [&options](std::string const& s) {
    return absl::StrCat("\"", internal::DebugString(s, options), "\"");
  };
  return absl::StrCat(
      "google::cloud::bigquery_v2_minimal_internal::ListJobsRequest {",  //
      sep(1), "project_id: ", quoted(project_id_),                       //
      sep(1), "all_users: ", all_users_ ? "true" : "false",              //
      sep(1), "max_results: ", max_results_,                             //
      sep(1), "min_creation_time {",                                     //
      sep(2), "\"", internal::FormatRfc3339(min_creation_time_), "\"",   //
      sep(1), "}",                                                       //
      sep(1), "max_creation_time {",                                     //
      sep(2), "\"", internal::FormatRfc3339(max_creation_time_), "\"",   //
      sep(1), "}",                                                       //
      sep(1), "page_token: ", quoted(page_token_),                       //
      sep(1), "projection {",                                            //
      sep(2), "value: ", quoted(projection_.value),                      //
      sep(1), "}",                                                       //
      sep(1), "state_filter {",                                          //
      sep(2), "value: ", quoted(state_filter_.value),                    //
      sep(1), "}",                                                       //
      sep(1), "parent_job_id: ", quoted(parent_job_id_),                 //
      sep(0), "}");
}

ListJobsRequest::ListJobsRequest(std::string project_id)
    : project_id_(std::move(project_id)),
      all_users_(false),
      max_results_(0),
      min_creation_time_(kDefaultTimepoint),
      max_creation_time_(kDefaultTimepoint) {}

StatusOr<rest_internal::RestRequest> BuildRestRequest(GetJobRequest const& r,
                                                      Options const& opts) {
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
  std::string endpoint = GetJobsEndpoint(opts);

  std::string path =
      absl::StrCat(endpoint, r.project_id(), "/jobs/", r.job_id());
  request.SetPath(std::move(path));

  // Add query params.
  if (!r.location().empty()) {
    request.AddQueryParameter("location", r.location());
  }
  return request;
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(ListJobsRequest const& r,
                                                      Options const& opts) {
  rest_internal::RestRequest request;
  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid ListJobsRequest: Project Id is empty", GCP_ERROR_INFO());
  }
  // Builds GetJob request path based on endpoint provided.
  std::string endpoint = GetJobsEndpoint(opts);

  std::string path = absl::StrCat(endpoint, r.project_id(), "/jobs");
  request.SetPath(std::move(path));

  // Add query params.
  if (r.all_users()) {
    request.AddQueryParameter("allUsers", "true");
  }
  if (r.max_results() > 0) {
    request.AddQueryParameter("maxResults", std::to_string(r.max_results()));
  }
  if (r.min_creation_time() != kDefaultTimepoint) {
    request.AddQueryParameter("minCreationTime",
                              internal::FormatRfc3339(r.min_creation_time()));
  }
  if (r.max_creation_time() != kDefaultTimepoint) {
    request.AddQueryParameter("maxCreationTime",
                              internal::FormatRfc3339(r.max_creation_time()));
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
