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

#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobRestStub::~BigQueryJobRestStub() = default;

StatusOr<GetJobResponse> DefaultBigQueryJobRestStub::GetJob(
    GetJobRequest const& request) {
  GetJobResponse response;
  if (request.project_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Project Id is empty", GCP_ERROR_INFO());
  }
  if (request.job_id().empty()) {
    return internal::InvalidArgumentError(
        "Invalid GetJobRequest: Job Id is empty", GCP_ERROR_INFO());
  }
  // Not Implemented Yet: Call the rest client to get job details from the
  // server.
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
