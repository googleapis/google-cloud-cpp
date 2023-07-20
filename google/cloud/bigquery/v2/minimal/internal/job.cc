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
      .StringField("project_id", project_id)
      .StringField("job_id", job_id)
      .StringField("location", location)
      .Build();
}

std::string Job::DebugString(absl::string_view name,
                             TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("etag", etag)
      .StringField("kind", kind)
      .StringField("self_link", self_link)
      .StringField("id", id)
      .SubMessage("configuration", configuration)
      .SubMessage("reference", job_reference)
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
  if (j.contains("errorResult")) j.at("errorResult").get_to(jb.error_result);
  if (j.contains("errors")) j.at("errors").get_to(jb.errors);
  if (j.contains("state")) j.at("state").get_to(jb.state);
}

void to_json(nlohmann::json& j, JobReference const& jb) {
  j = nlohmann::json{{"projectId", jb.project_id},
                     {"jobId", jb.job_id},
                     {"location", jb.location}};
}
void from_json(nlohmann::json const& j, JobReference& jb) {
  if (j.contains("projectId")) j.at("projectId").get_to(jb.project_id);
  if (j.contains("jobId")) j.at("jobId").get_to(jb.job_id);
  if (j.contains("location")) j.at("location").get_to(jb.location);
}

void to_json(nlohmann::json& j, Job const& jb) {
  j = nlohmann::json{{"kind", jb.kind},
                     {"etag", jb.etag},
                     {"id", jb.id},
                     {"selfLink", jb.self_link},
                     {"user_email", jb.user_email},
                     {"status", jb.status},
                     {"jobReference", jb.job_reference},
                     {"configuration", jb.configuration},
                     {"statistics", jb.statistics}};
}
void from_json(nlohmann::json const& j, Job& jb) {
  if (j.contains("kind")) j.at("kind").get_to(jb.kind);
  if (j.contains("etag")) j.at("etag").get_to(jb.etag);
  if (j.contains("id")) j.at("id").get_to(jb.id);
  if (j.contains("selfLink")) j.at("selfLink").get_to(jb.self_link);
  if (j.contains("user_email")) j.at("user_email").get_to(jb.user_email);
  if (j.contains("status")) j.at("status").get_to(jb.status);
  if (j.contains("jobReference")) j.at("jobReference").get_to(jb.job_reference);
  if (j.contains("configuration"))
    j.at("configuration").get_to(jb.configuration);
  if (j.contains("statistics")) j.at("statistics").get_to(jb.statistics);
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
  if (j.contains("id")) j.at("id").get_to(l.id);
  if (j.contains("kind")) j.at("kind").get_to(l.kind);
  if (j.contains("user_email")) j.at("user_email").get_to(l.user_email);
  if (j.contains("state")) j.at("state").get_to(l.state);
  if (j.contains("principal_subject"))
    j.at("principal_subject").get_to(l.principal_subject);
  if (j.contains("jobReference")) j.at("jobReference").get_to(l.job_reference);
  if (j.contains("configuration"))
    j.at("configuration").get_to(l.configuration);
  if (j.contains("status")) j.at("status").get_to(l.status);
  if (j.contains("statistics")) j.at("statistics").get_to(l.statistics);
  if (j.contains("errorResult")) j.at("errorResult").get_to(l.error_result);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
