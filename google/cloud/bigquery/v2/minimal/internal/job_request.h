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

#include "google/cloud/internal/rest_request.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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

  std::string DebugString(TracingOptions const& options) const;

 private:
  std::string project_id_;
  std::string job_id_;
  std::string location_;
};

struct Projection {
  static Projection Minimal();
  static Projection Full();

  std::string value;
};

struct StateFilter {
  static StateFilter Pending();
  static StateFilter Running();
  static StateFilter Done();

  std::string value;
};

class ListJobsRequest {
 public:
  ListJobsRequest() = default;
  explicit ListJobsRequest(std::string project_id);

  std::string const& project_id() const { return project_id_; }
  bool const& all_users() const { return all_users_; }
  std::int32_t const& max_results() const { return max_results_; }
  std::chrono::system_clock::time_point const& min_creation_time() const {
    return min_creation_time_;
  }
  std::chrono::system_clock::time_point const& max_creation_time() const {
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

  std::string DebugString(TracingOptions const& options) const;

  // Members
 private:
  std::string project_id_;
  bool all_users_;
  std::int32_t max_results_;
  std::chrono::system_clock::time_point min_creation_time_;
  std::chrono::system_clock::time_point max_creation_time_;
  std::string page_token_;
  Projection projection_;
  StateFilter state_filter_;
  std::string parent_job_id_;
};

// Builds RestRequest from GetJobRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(GetJobRequest const& r,
                                                      Options const& opts);
// Builds RestRequest from ListJobsRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(ListJobsRequest const& r,
                                                      Options const& opts);

std::ostream& operator<<(std::ostream& os, GetJobRequest const& request);
std::ostream& operator<<(std::ostream& os, ListJobsRequest const& request);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_REQUEST_H
