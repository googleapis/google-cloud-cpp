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

namespace {
std::string MakeJobConfiguration(absl::string_view name,
                                 TracingOptions const& options,
                                 std::string job_type, std::string query) {
  return internal::DebugFormatter(name, options)
      .StringField("job_type", std::move(job_type))
      .StringField("query", std::move(query))
      .Build();
}

std::string MakeJobReference(absl::string_view name,
                             TracingOptions const& options,
                             std::string project_id, std::string job_id,
                             std::string location) {
  return internal::DebugFormatter(name, options)
      .StringField("project_id", std::move(project_id))
      .StringField("job_id", std::move(job_id))
      .StringField("location", std::move(location))
      .Build();
}

}  // namespace

std::string Job::DebugString(absl::string_view name,
                             TracingOptions const& options, int indent) const {
  auto job_configuration =
      MakeJobConfiguration("JobConfiguration", options, configuration.job_type,
                           configuration.query_config.query);
  auto job_reference =
      MakeJobReference("JobReference", options, reference.project_id,
                       reference.job_id, reference.location);

  return internal::DebugFormatter(name, options, indent)
      .StringField("etag", etag)
      .StringField("kind", kind)
      .StringField("id", id)
      .StringField("job_configuration", job_configuration)
      .StringField("job_reference", job_reference)
      .StringField("job_status", status.state)
      .StringField("error_result", status.error_result.message)
      .Build();
}

std::string ListFormatJob::DebugString(absl::string_view name,
                                       TracingOptions const& options,
                                       int indent) const {
  auto job_configuration =
      MakeJobConfiguration("JobConfiguration", options, configuration.job_type,
                           configuration.query_config.query);
  auto job_reference =
      MakeJobReference("JobReference", options, reference.project_id,
                       reference.job_id, reference.location);

  return internal::DebugFormatter(name, options, indent)
      .StringField("id", id)
      .StringField("kind", kind)
      .StringField("state", state)
      .StringField("job_configuration", job_configuration)
      .StringField("job_reference", job_reference)
      .StringField("job_status", status.state)
      .StringField("error_result", status.error_result.message)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
