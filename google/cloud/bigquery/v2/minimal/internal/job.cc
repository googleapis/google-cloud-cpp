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
  auto const* delim = options.single_line_mode() ? " " : "\n";
  absl::StrAppend(&out, "Job{", delim, "etag=", etag, delim, ", kind=", kind,
                  delim, ", id=", id, delim, ", job_configuration={", delim,
                  "job_type=", configuration.job_type, delim,
                  ", query=", configuration.query_config.query, delim, "}",
                  delim, ", job_reference={", delim,
                  "job_id=", reference.job_id, delim,
                  ", location=", reference.location, delim,
                  ", project_id=", reference.project_id, delim, "}", delim,
                  ", job_status=", status.state, delim,
                  ", error_result=", status.error_result.message, delim, "}");
  return out;
}

std::string ListFormatJob::DebugString(TracingOptions const& options) const {
  std::string out;
  auto const* delim = options.single_line_mode() ? " " : "\n";
  absl::StrAppend(
      &out, "ListFormatJob{", delim, "id=", id, delim, ", kind=", kind, delim,
      ", state=", state, delim, ", job_configuration={", delim,
      "job_type=", configuration.job_type, delim,
      ", query=", configuration.query_config.query, delim, "}", delim,
      ", job_reference={", delim, "job_id=", reference.job_id, delim,
      ", location=", reference.location, delim,
      ", project_id=", reference.project_id, delim, "}",
      ", job_status=", status.state, delim,
      ", error_result=", error_result.message, delim, "}");
  return out;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
