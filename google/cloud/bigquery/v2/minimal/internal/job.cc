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

#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string Job::DebugString(TracingOptions const& options) const {
  std::string out;
  absl::StrAppend(
      &out, "Job{etag=", etag, ", kind=", kind, ", id=", id,
      ", job_configuration={job_type=", configuration.job_type,
      ", query=", configuration.query_config.query, "}", ", job_reference={",
      "job_id=", reference.job_id, ", location=", reference.location,
      ", project_id=", reference.project_id, "}", ", job_status=", status.state,
      ", error_result=", status.error_result.message, "}");
  return internal::DebugString(out, options);
}

std::string ListFormatJob::DebugString(TracingOptions const& options) const {
  std::string out;
  absl::StrAppend(
      &out, "ListFormatJob{id=", id, ", kind=", kind, ", state=", state,
      ", job_configuration={job_type=", configuration.job_type,
      ", query=", configuration.query_config.query, "}", ", job_reference={",
      "job_id=", reference.job_id, ", location=", reference.location,
      ", project_id=", reference.project_id, "}", ", job_status=", status.state,
      ", error_result=", error_result.message, "}");
  return internal::DebugString(out, options);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
