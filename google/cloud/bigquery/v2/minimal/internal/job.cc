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

std::string JobReference::DebugString(absl::string_view name,
                                      TracingOptions const& options,
                                      int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", projectId)
      .StringField("job_id", jobId)
      .StringField("location", location)
      .Build();
}

std::string Job::DebugString(absl::string_view name,
                             TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("etag", etag)
      .StringField("kind", kind)
      .StringField("self_link", selfLink)
      .StringField("id", id)
      .SubMessage("configuration", configuration)
      .SubMessage("reference", jobReference)
      .SubMessage("status", status)
      .SubMessage("statistics", statistics)
      .Build();
}

std::string ListFormatJob::DebugString(absl::string_view name,
                                       TracingOptions const& options,
                                       int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("id", id)
      .StringField("kind", kind)
      .StringField("state", state)
      .SubMessage("configuration", configuration)
      .SubMessage("reference", jobReference)
      .SubMessage("status", status)
      .SubMessage("statistics", statistics)
      .SubMessage("error_result", errorResult)
      .Build();
}

std::string JobStatus::DebugString(absl::string_view name,
                                   TracingOptions const& options,
                                   int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("errors", errors)
      .StringField("state", state)
      .SubMessage("error_result", errorResult)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
