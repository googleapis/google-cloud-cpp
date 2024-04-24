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
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string JobReference::DebugString(absl::string_view name,
                                      TracingOptions const& options,
                                      int indent) const {
  auto f = internal::DebugFormatter(name, options, indent);
  if (project_id) f.StringField("project_id", *project_id);
  if (job_id) f.StringField("job_id", *job_id);
  if (location) f.StringField("location", *location);
  return f.Build();
}

std::string Job::DebugString(absl::string_view name,
                             TracingOptions const& options, int indent) const {
  auto f = internal::DebugFormatter(name, options, indent);
  if (kind) f.StringField("kind", *kind);
  if (etag) f.StringField("etag", *etag);
  if (self_link) f.StringField("self_link", *self_link);
  if (id) f.StringField("id", *id);
  if (configuration) f.SubMessage("configuration", *configuration);
  if (job_reference) f.SubMessage("reference", *job_reference);
  if (status) f.SubMessage("status", *status);
  if (statistics) f.SubMessage("statistics", *statistics);
  return f.Build();
}

std::string ListFormatJob::DebugString(absl::string_view name,
                                       TracingOptions const& options,
                                       int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("id", id)
      .StringField("kind", kind)
      .StringField("state", state)
      .SubMessage("configuration", configuration)
      .SubMessage("reference", job_reference)
      .SubMessage("status", status)
      .SubMessage("statistics", statistics)
      .SubMessage("error_result", error_result)
      .Build();
}

std::string JobStatus::DebugString(absl::string_view name,
                                   TracingOptions const& options,
                                   int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("errors", errors)
      .StringField("state", state)
      .SubMessage("error_result", error_result)
      .Build();
}

void to_json(nlohmann::json& j, JobStatus const& jb) {
  j = nlohmann::json{{"errorResult", jb.error_result},
                     {"errors", jb.errors},
                     {"state", jb.state}};
}
void from_json(nlohmann::json const& j, JobStatus& jb) {
  SafeGetTo(jb.error_result, j, "errorResult");
  SafeGetTo(jb.errors, j, "errors");
  SafeGetTo(jb.state, j, "state");
}

void to_json(nlohmann::json& j, JobReference const& jb) {
  if (jb.project_id) j["projectId"] = *jb.project_id;
  if (jb.job_id) j["jobId"] = *jb.job_id;
  if (jb.location) j["location"] = *jb.location;
}
void from_json(nlohmann::json const& j, JobReference& jb) {
  SafeGetTo(jb.project_id, j, "projectId");
  SafeGetTo(jb.job_id, j, "jobId");
  SafeGetTo(jb.location, j, "location");
}

void to_json(nlohmann::json& j, Job const& jb) {
  if (jb.kind) j["kind"] = *jb.kind;
  if (jb.etag) j["etag"] = *jb.etag;
  if (jb.id) j["id"] = *jb.id;
  if (jb.self_link) j["selfLink"] = *jb.self_link;
  if (jb.user_email) j["user_email"] = *jb.user_email;
  if (jb.status) j["status"] = *jb.status;
  if (jb.job_reference) j["jobReference"] = *jb.job_reference;
  if (jb.configuration) j["configuration"] = *jb.configuration;
  if (jb.statistics) j["statistics"] = *jb.statistics;
}
void from_json(nlohmann::json const& j, Job& jb) {
  SafeGetTo(jb.kind, j, "kind");
  SafeGetTo(jb.etag, j, "etag");
  SafeGetTo(jb.id, j, "id");
  SafeGetTo(jb.self_link, j, "selfLink");
  SafeGetTo(jb.user_email, j, "user_email");
  SafeGetTo(jb.status, j, "status");
  SafeGetTo(jb.job_reference, j, "jobReference");
  SafeGetTo(jb.configuration, j, "configuration");
  SafeGetTo(jb.statistics, j, "statistics");
}

void to_json(nlohmann::json& j, ListFormatJob const& l) {
  j = nlohmann::json{{"id", l.id},
                     {"kind", l.kind},
                     {"user_email", l.user_email},
                     {"state", l.state},
                     {"principal_subject", l.principal_subject},
                     {"jobReference", l.job_reference},
                     {"configuration", l.configuration},
                     {"status", l.status},
                     {"statistics", l.statistics},
                     {"errorResult", l.error_result}};
}
void from_json(nlohmann::json const& j, ListFormatJob& l) {
  SafeGetTo(l.id, j, "id");
  SafeGetTo(l.kind, j, "kind");
  SafeGetTo(l.user_email, j, "user_email");
  SafeGetTo(l.state, j, "state");
  SafeGetTo(l.principal_subject, j, "principal_subject");
  SafeGetTo(l.job_reference, j, "jobReference");
  SafeGetTo(l.configuration, j, "configuration");
  SafeGetTo(l.status, j, "status");
  SafeGetTo(l.statistics, j, "statistics");
  SafeGetTo(l.error_result, j, "errorResult");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
