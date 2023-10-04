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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_REQUEST_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

// Holds request parameters necessary to make the GetJob call.
class GetJobRequest {
 public:
  GetJobRequest() = default;
  explicit GetJobRequest(std::string project_id, std::string job_id)
      : project_id_(std::move(project_id)), job_id_(std::move(job_id)) {}

  std::string const& project_id() const { return project_id_; }
  std::string const& job_id() const { return job_id_; }
  std::string const& location() const { return location_; }

  GetJobRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  GetJobRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  GetJobRequest& set_job_id(std::string job_id) & {
    job_id_ = std::move(job_id);
    return *this;
  }
  GetJobRequest&& set_job_id(std::string job_id) && {
    return std::move(set_job_id(std::move(job_id)));
  }

  GetJobRequest& set_location(std::string location) & {
    location_ = std::move(location);
    return *this;
  }
  GetJobRequest&& set_location(std::string location) && {
    return std::move(set_location(std::move(location)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  std::string job_id_;
  std::string location_;
};

struct Projection {
  static Projection Minimal();
  static Projection Full();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

struct StateFilter {
  static StateFilter Pending();
  static StateFilter Running();
  static StateFilter Done();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

class ListJobsRequest {
 public:
  ListJobsRequest() = default;
  explicit ListJobsRequest(std::string project_id);

  std::string const& project_id() const { return project_id_; }
  bool const& all_users() const { return all_users_; }
  std::int32_t const& max_results() const { return max_results_; }
  absl::optional<std::chrono::system_clock::time_point> const&
  min_creation_time() const {
    return min_creation_time_;
  }
  absl::optional<std::chrono::system_clock::time_point> const&
  max_creation_time() const {
    return max_creation_time_;
  }
  std::string const& page_token() const { return page_token_; }
  Projection const& projection() const { return projection_; }
  StateFilter const& state_filter() const { return state_filter_; }
  std::string const& parent_job_id() const { return parent_job_id_; }

  ListJobsRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  ListJobsRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  ListJobsRequest& set_all_users(bool all_users) & {
    all_users_ = all_users;
    return *this;
  }
  ListJobsRequest&& set_all_users(bool all_users) && {
    return std::move(set_all_users(all_users));
  }

  ListJobsRequest& set_max_results(std::int32_t max_results) & {
    max_results_ = max_results;
    return *this;
  }
  ListJobsRequest&& set_max_results(std::int32_t max_results) && {
    return std::move(set_max_results(max_results));
  }

  ListJobsRequest& set_min_creation_time(
      std::chrono::system_clock::time_point min_creation_time) & {
    min_creation_time_ = min_creation_time;
    return *this;
  }
  ListJobsRequest&& set_min_creation_time(
      std::chrono::system_clock::time_point min_creation_time) && {
    return std::move(set_min_creation_time(min_creation_time));
  }

  ListJobsRequest& set_max_creation_time(
      std::chrono::system_clock::time_point max_creation_time) & {
    max_creation_time_ = max_creation_time;
    return *this;
  }
  ListJobsRequest&& set_max_creation_time(
      std::chrono::system_clock::time_point max_creation_time) && {
    return std::move(set_max_creation_time(max_creation_time));
  }

  ListJobsRequest& set_page_token(std::string page_token) & {
    page_token_ = std::move(page_token);
    return *this;
  }
  ListJobsRequest&& set_page_token(std::string page_token) && {
    return std::move(set_page_token(std::move(page_token)));
  }

  ListJobsRequest& set_projection(Projection projection) & {
    projection_ = std::move(projection);
    return *this;
  }
  ListJobsRequest&& set_projection(Projection projection) && {
    return std::move(set_projection(std::move(projection)));
  }

  ListJobsRequest& set_state_filter(StateFilter state_filter) & {
    state_filter_ = std::move(state_filter);
    return *this;
  }
  ListJobsRequest&& set_state_filter(StateFilter state_filter) && {
    return std::move(set_state_filter(std::move(state_filter)));
  }

  ListJobsRequest& set_parent_job_id(std::string parent_job_id) & {
    parent_job_id_ = std::move(parent_job_id);
    return *this;
  }
  ListJobsRequest&& set_parent_job_id(std::string parent_job_id) && {
    return std::move(set_parent_job_id(std::move(parent_job_id)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  bool all_users_;
  std::int32_t max_results_;
  absl::optional<std::chrono::system_clock::time_point> min_creation_time_;
  absl::optional<std::chrono::system_clock::time_point> max_creation_time_;
  std::string page_token_;
  Projection projection_;
  StateFilter state_filter_;
  std::string parent_job_id_;
};

class InsertJobRequest {
 public:
  InsertJobRequest() = default;
  explicit InsertJobRequest(std::string project_id, Job job)
      : project_id_(std::move(project_id)), job_(std::move(job)) {}

  std::string const& project_id() const { return project_id_; }
  Job const& job() const { return job_; }
  std::vector<std::string> json_filter_keys() const {
    return json_filter_keys_;
  }

  InsertJobRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  InsertJobRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  InsertJobRequest& set_job(Job job) & {
    job_ = std::move(job);
    return *this;
  }
  InsertJobRequest&& set_job(Job job) && {
    return std::move(set_job(std::move(job)));
  }

  InsertJobRequest& set_json_filter_keys(std::vector<std::string> keys) & {
    json_filter_keys_ = std::move(keys);
    return *this;
  }
  InsertJobRequest&& set_json_filter_keys(std::vector<std::string> keys) && {
    return std::move(set_json_filter_keys(std::move(keys)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  Job job_;
  std::vector<std::string> json_filter_keys_;
};

class CancelJobRequest {
 public:
  CancelJobRequest() = default;
  explicit CancelJobRequest(std::string project_id, std::string job_id)
      : project_id_(std::move(project_id)), job_id_(std::move(job_id)) {}

  std::string const& project_id() const { return project_id_; }
  std::string const& job_id() const { return job_id_; }
  std::string const& location() const { return location_; }

  CancelJobRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  CancelJobRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  CancelJobRequest& set_job_id(std::string job_id) & {
    job_id_ = std::move(job_id);
    return *this;
  }
  CancelJobRequest&& set_job_id(std::string job_id) && {
    return std::move(set_job_id(std::move(job_id)));
  }

  CancelJobRequest& set_location(std::string location) & {
    location_ = std::move(location);
    return *this;
  }
  CancelJobRequest&& set_location(std::string location) && {
    return std::move(set_location(std::move(location)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  std::string job_id_;
  std::string location_;
};

struct DataFormatOptions {
  DataFormatOptions() = default;
  bool use_int64_timestamp = false;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DataFormatOptions,
                                                use_int64_timestamp);

class QueryRequest {
 public:
  QueryRequest() = default;
  explicit QueryRequest(std::string query) : query_(std::move(query)) {}

  std::string const& query() const { return query_; }
  std::string const& kind() const { return kind_; }
  std::string const& parameter_mode() const { return parameter_mode_; }
  std::string const& location() const { return location_; }
  std::string const& request_id() const { return request_id_; }

  bool const& dry_run() const { return dry_run_; }
  bool const& preserve_nulls() const { return preserve_nulls_; }
  bool const& use_query_cache() const { return use_query_cache_; }
  bool const& use_legacy_sql() const { return use_legacy_sql_; }
  bool const& create_session() const { return create_session_; }

  std::uint32_t const& max_results() const { return max_results_; }
  std::int64_t const& maximum_bytes_billed() const {
    return maximum_bytes_billed_;
  }
  std::chrono::milliseconds const& timeout() const { return timeout_; }

  std::vector<ConnectionProperty> const& connection_properties() const {
    return connection_properties_;
  }
  std::vector<QueryParameter> const& query_parameters() const {
    return query_parameters_;
  }
  std::map<std::string, std::string> const& labels() const { return labels_; }

  DatasetReference const& default_dataset() const { return default_dataset_; }
  DataFormatOptions const& format_options() const { return format_options_; }

  QueryRequest& set_query(std::string query) & {
    query_ = std::move(query);
    return *this;
  }
  QueryRequest&& set_query(std::string query) && {
    return std::move(set_query(std::move(query)));
  }

  QueryRequest& set_kind(std::string kind) & {
    kind_ = std::move(kind);
    return *this;
  }
  QueryRequest&& set_kind(std::string kind) && {
    return std::move(set_kind(std::move(kind)));
  }

  QueryRequest& set_parameter_mode(std::string parameter_mode) & {
    parameter_mode_ = std::move(parameter_mode);
    return *this;
  }
  QueryRequest&& set_parameter_mode(std::string parameter_mode) && {
    return std::move(set_parameter_mode(std::move(parameter_mode)));
  }

  QueryRequest& set_location(std::string location) & {
    location_ = std::move(location);
    return *this;
  }
  QueryRequest&& set_location(std::string location) && {
    return std::move(set_location(std::move(location)));
  }

  QueryRequest& set_request_id(std::string request_id) & {
    request_id_ = std::move(request_id);
    return *this;
  }
  QueryRequest&& set_request_id(std::string request_id) && {
    return std::move(set_request_id(std::move(request_id)));
  }

  QueryRequest& set_dry_run(bool dry_run) & {
    dry_run_ = std::move(dry_run);
    return *this;
  }
  QueryRequest&& set_dry_run(bool dry_run) && {
    return std::move(set_dry_run(std::move(dry_run)));
  }

  QueryRequest& set_preserve_nulls(bool preserve_nulls) & {
    preserve_nulls_ = std::move(preserve_nulls);
    return *this;
  }
  QueryRequest&& set_preserve_nulls(bool preserve_nulls) && {
    return std::move(set_preserve_nulls(std::move(preserve_nulls)));
  }

  QueryRequest& set_use_query_cache(bool use_query_cache) & {
    use_query_cache_ = std::move(use_query_cache);
    return *this;
  }
  QueryRequest&& set_use_query_cache(bool use_query_cache) && {
    return std::move(set_use_query_cache(std::move(use_query_cache)));
  }

  QueryRequest& set_use_legacy_sql(bool use_legacy_sql) & {
    use_legacy_sql_ = std::move(use_legacy_sql);
    return *this;
  }
  QueryRequest&& set_use_legacy_sql(bool use_legacy_sql) && {
    return std::move(set_use_legacy_sql(std::move(use_legacy_sql)));
  }

  QueryRequest& set_create_session(bool create_session) & {
    create_session_ = std::move(create_session);
    return *this;
  }
  QueryRequest&& set_create_session(bool create_session) && {
    return std::move(set_create_session(std::move(create_session)));
  }

  QueryRequest& set_max_results(std::uint32_t max_results) & {
    max_results_ = std::move(max_results);
    return *this;
  }
  QueryRequest&& set_max_results(std::uint32_t max_results) && {
    return std::move(set_max_results(std::move(max_results)));
  }

  QueryRequest& set_maximum_bytes_billed(std::int64_t maximum_bytes_billed) & {
    maximum_bytes_billed_ = std::move(maximum_bytes_billed);
    return *this;
  }
  QueryRequest&& set_maximum_bytes_billed(
      std::int64_t maximum_bytes_billed) && {
    return std::move(set_maximum_bytes_billed(std::move(maximum_bytes_billed)));
  }

  QueryRequest& set_timeout(std::chrono::milliseconds timeout) & {
    timeout_ = std::move(timeout);
    return *this;
  }
  QueryRequest&& set_timeout(std::chrono::milliseconds timeout) && {
    return std::move(set_timeout(std::move(timeout)));
  }

  QueryRequest& set_connection_properties(
      std::vector<ConnectionProperty> connection_properties) & {
    connection_properties_ = std::move(connection_properties);
    return *this;
  }
  QueryRequest&& set_connection_properties(
      std::vector<ConnectionProperty> connection_properties) && {
    return std::move(
        set_connection_properties(std::move(connection_properties)));
  }

  QueryRequest& set_query_parameters(
      std::vector<QueryParameter> query_parameters) & {
    query_parameters_ = std::move(query_parameters);
    return *this;
  }
  QueryRequest&& set_query_parameters(
      std::vector<QueryParameter> query_parameters) && {
    return std::move(set_query_parameters(std::move(query_parameters)));
  }

  QueryRequest& set_labels(std::map<std::string, std::string> labels) & {
    labels_ = std::move(labels);
    return *this;
  }
  QueryRequest&& set_labels(std::map<std::string, std::string> labels) && {
    return std::move(set_labels(std::move(labels)));
  }

  QueryRequest& set_default_dataset(DatasetReference default_dataset) & {
    default_dataset_ = std::move(default_dataset);
    return *this;
  }
  QueryRequest&& set_default_dataset(DatasetReference default_dataset) && {
    return std::move(set_default_dataset(std::move(default_dataset)));
  }

  QueryRequest& set_format_options(DataFormatOptions format_options) & {
    format_options_ = std::move(format_options);
    return *this;
  }
  QueryRequest&& set_format_options(DataFormatOptions format_options) && {
    return std::move(set_format_options(std::move(format_options)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string query_;
  std::string kind_;
  std::string parameter_mode_;
  std::string location_;
  std::string request_id_;

  bool dry_run_ = false;
  bool preserve_nulls_ = false;
  bool use_query_cache_ = false;
  bool use_legacy_sql_ = false;
  bool create_session_ = false;

  std::uint32_t max_results_ = 0;
  std::int64_t maximum_bytes_billed_ = 0;

  std::chrono::milliseconds timeout_ = std::chrono::milliseconds(0);

  std::vector<ConnectionProperty> connection_properties_;
  std::vector<QueryParameter> query_parameters_;

  std::map<std::string, std::string> labels_;

  DatasetReference default_dataset_;
  DataFormatOptions format_options_;
};
void to_json(nlohmann::json& j, QueryRequest const& q);
void from_json(nlohmann::json const& j, QueryRequest& q);

class PostQueryRequest {
 public:
  PostQueryRequest() = default;
  explicit PostQueryRequest(std::string project_id)
      : project_id_(std::move(project_id)) {}

  std::string const& project_id() const { return project_id_; }
  QueryRequest const& query_request() const { return query_request_; }
  std::vector<std::string> json_filter_keys() const {
    return json_filter_keys_;
  }

  PostQueryRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  PostQueryRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  PostQueryRequest& set_query_request(QueryRequest query_request) & {
    query_request_ = std::move(query_request);
    return *this;
  }
  PostQueryRequest&& set_query_request(QueryRequest query_request) && {
    return std::move(set_query_request(std::move(query_request)));
  }

  PostQueryRequest& set_json_filter_keys(std::vector<std::string> keys) & {
    json_filter_keys_ = std::move(keys);
    return *this;
  }
  PostQueryRequest&& set_json_filter_keys(std::vector<std::string> keys) && {
    return std::move(set_json_filter_keys(std::move(keys)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  QueryRequest query_request_;
  std::vector<std::string> json_filter_keys_;
};
void to_json(nlohmann::json& j, PostQueryRequest const& q);
void from_json(nlohmann::json const& j, PostQueryRequest& q);

class GetQueryResultsRequest {
 public:
  GetQueryResultsRequest() = default;
  explicit GetQueryResultsRequest(std::string project_id, std::string job_id)
      : project_id_(std::move(project_id)), job_id_(std::move(job_id)) {}

  std::string const& project_id() const { return project_id_; }
  std::string const& job_id() const { return job_id_; }
  std::string const& page_token() const { return page_token_; }
  std::string const& location() const { return location_; }

  std::uint64_t const& start_index() const { return start_index_; }
  std::uint32_t const& max_results() const { return max_results_; }

  std::chrono::milliseconds const& timeout() const { return timeout_; }

  GetQueryResultsRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  GetQueryResultsRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  GetQueryResultsRequest& set_job_id(std::string job_id) & {
    job_id_ = std::move(job_id);
    return *this;
  }
  GetQueryResultsRequest&& set_job_id(std::string job_id) && {
    return std::move(set_job_id(std::move(job_id)));
  }

  GetQueryResultsRequest& set_page_token(std::string page_token) & {
    page_token_ = std::move(page_token);
    return *this;
  }
  GetQueryResultsRequest&& set_page_token(std::string page_token) && {
    return std::move(set_page_token(std::move(page_token)));
  }

  GetQueryResultsRequest& set_location(std::string location) & {
    location_ = std::move(location);
    return *this;
  }
  GetQueryResultsRequest&& set_location(std::string location) && {
    return std::move(set_location(std::move(location)));
  }

  GetQueryResultsRequest& set_start_index(std::uint64_t start_index) & {
    start_index_ = std::move(start_index);
    return *this;
  }
  GetQueryResultsRequest&& set_start_index(std::uint64_t start_index) && {
    return std::move(set_start_index(std::move(start_index)));
  }

  GetQueryResultsRequest& set_max_results(std::uint32_t max_results) & {
    max_results_ = std::move(max_results);
    return *this;
  }
  GetQueryResultsRequest&& set_max_results(std::uint32_t max_results) && {
    return std::move(set_max_results(std::move(max_results)));
  }

  GetQueryResultsRequest& set_timeout(std::chrono::milliseconds timeout) & {
    timeout_ = std::move(timeout);
    return *this;
  }
  GetQueryResultsRequest&& set_timeout(std::chrono::milliseconds timeout) && {
    return std::move(set_timeout(std::move(timeout)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  std::string job_id_;
  std::string page_token_;
  std::string location_;

  std::uint64_t start_index_ = 0;
  std::uint32_t max_results_ = 0;

  std::chrono::milliseconds timeout_ = std::chrono::milliseconds(0);
};
void to_json(nlohmann::json& j, GetQueryResultsRequest const& q);
void from_json(nlohmann::json const& j, GetQueryResultsRequest& q);

// Builds RestRequest from GetJobRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(GetJobRequest const& r);
// Builds RestRequest from ListJobsRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(ListJobsRequest const& r);
// Builds RestRequest from InsertJobRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    InsertJobRequest const& r);
// Builds RestRequest from CancelJobRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    CancelJobRequest const& r);
// Builds RestRequest from PostQueryRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    PostQueryRequest const& r);
// Builds RestRequest from GetQueryResultsRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    GetQueryResultsRequest const& r);

std::ostream& operator<<(std::ostream& os, GetJobRequest const& request);
std::ostream& operator<<(std::ostream& os, ListJobsRequest const& request);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_REQUEST_H
