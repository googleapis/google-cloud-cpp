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
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"
#include <ostream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<rest_internal::RestRequest> BuildRestRequest(GetJobRequest& r,
                                                      Options opts) {
  rest_internal::RestRequest request;
  if (r.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Project Id is empty", GCP_ERROR_INFO());
  }
  if (r.job_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Job Id is empty", GCP_ERROR_INFO());
  }
  // Fix the endpoints prefix and suffixes.
  auto endpoint =
      opts.lookup<EndpointOption>("https://bigquery.googleapis.com/");
  if (!absl::StartsWith(endpoint, "https://") &&
      !absl::StartsWith(endpoint, "http://")) {
    endpoint = absl::StrCat("https://", endpoint);
  }
  if (!absl::EndsWith(endpoint, "/")) absl::StrAppend(&endpoint, "/");
  absl::StrAppend(&endpoint, "bigquery/v2/projects/");

  std::string path =
      absl::StrCat(endpoint, r.project_id(), "/jobs/", r.job_id());
  request.SetPath(std::move(path));

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
