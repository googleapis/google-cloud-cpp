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
#include "google/cloud/version.h"
#include <iosfwd>
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

  // Builds RestRequest from GetJobRequest.
  static StatusOr<rest_internal::RestRequest> BuildRestRequest(
      Options opts, GetJobRequest& r);

 private:
  std::string project_id_;
  std::string job_id_;
  std::string location_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_REQUEST_H
