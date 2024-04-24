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

#include "google/cloud/bigquery/v2/minimal/internal/job_configuration.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string JobConfiguration::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  
  auto f = internal::DebugFormatter(name, options, indent);
  if (job_type) f.StringField("job_type", *job_type);
  if (dry_run) f.Field("dry_run", *dry_run);
  if (job_timeout) f.Field("job_timeout", *job_timeout);
  if (labels) f.Field("labels", *labels);
  if (query) f.SubMessage("query_config", *query);
  return f.Build();
}

void to_json(nlohmann::json& j, JobConfiguration const& c) {
  if (c.job_type) j["jobType"] = *c.job_type;
  if (c.query) j["query"] = *c.query;
  if (c.dry_run) j["dryRun"] = *c.dry_run;
  if (c.labels) j["labels"] = *c.labels;
  if (c.job_timeout) ToJson(c.job_timeout.value(), j, "jobTimeoutMs");
}
void from_json(nlohmann::json const& j, JobConfiguration& c) {
  SafeGetTo(c.job_type, j, "jobType");
  SafeGetTo(c.query, j, "query");
  SafeGetTo(c.dry_run, j, "dryRun");
  SafeGetTo(c.labels, j, "labels");

  FromJson(c.job_timeout, j, "jobTimeoutMs");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
