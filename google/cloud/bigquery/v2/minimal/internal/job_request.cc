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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::ostream& operator<<(std::ostream& os, GetJobRequest const& r) {
  os << "GetJobRequest={project_id=" << r.project_id()
     << ", job_id=" << r.job_id() << ", location=" << r.location();
  return os << "}";
}

StatusOr<rest_internal::RestRequest> GetJobRequest::BuildRestRequest(
    GetJobRequest& r) {
  rest_internal::RestRequest request;
  if (r.project_id().empty()) {
    GCP_LOG(DEBUG) << "Invalid request: " << r;
    return Status(StatusCode::kInvalidArgument,
                  "Invalid GetJobRequest: Project Id is empty");
  }
  if (r.job_id().empty()) {
    GCP_LOG(DEBUG) << "Invalid request: " << r;
    return Status(StatusCode::kInvalidArgument,
                  "Invalid GetJobRequest: Job Id is empty");
  }
  // Set path.
  std::string path =
      absl::StrCat("https://bigquery.googleapis.com/bigquery/v2/projects/",
                   r.project_id(), "/jobs/", r.job_id());
  request.SetPath(path);
  // Add query params.
  if (!r.location().empty()) {
    request.AddQueryParameter("location", r.location());
  }
  return request;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
