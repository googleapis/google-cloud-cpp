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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_H

#include "google/cloud/bigquery/v2/minimal/internal/job_configuration.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_stats.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct JobStatus {
  ErrorProto error_result;
  std::vector<ErrorProto> errors;
  std::string state;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, JobStatus const& jb);
void from_json(nlohmann::json const& j, JobStatus& jb);

struct JobReference {
  absl::optional<std::string> project_id;
  absl::optional<std::string> job_id;
  absl::optional<std::string> location;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, JobReference const& jb);
void from_json(nlohmann::json const& j, JobReference& jb);

struct Job {
  absl::optional<std::string> kind;
  absl::optional<std::string> etag;
  absl::optional<std::string> id;
  absl::optional<std::string> self_link;
  absl::optional<std::string> user_email;

  absl::optional<JobStatus> status;
  absl::optional<JobReference> job_reference;
  absl::optional<JobConfiguration> configuration;
  absl::optional<JobStatistics> statistics;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, Job const& jb);
void from_json(nlohmann::json const& j, Job& jb);

struct ListFormatJob {
  std::string id;
  std::string kind;
  std::string user_email;
  std::string state;
  std::string principal_subject;

  JobReference job_reference;
  JobConfiguration configuration;
  JobStatus status;
  JobStatistics statistics;

  ErrorProto error_result;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ListFormatJob const& l);
void from_json(nlohmann::json const& j, ListFormatJob& l);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_H
