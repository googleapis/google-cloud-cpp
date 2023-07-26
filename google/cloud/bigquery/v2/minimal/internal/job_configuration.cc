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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string JobConfiguration::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("job_type", job_type)
      .Field("dry_run", dry_run)
      .Field("job_timeout_ms", job_timeout_ms)
      .Field("labels", labels)
      .SubMessage("query_config", query)
      .Build();
}

void to_json(nlohmann::json& j, JobConfiguration const& c) {
  j = nlohmann::json{{"jobType", c.job_type},
                     {"query", c.query},
                     {"dryRun", c.dry_run},
                     {"jobTimeoutMs", c.job_timeout_ms},
                     {"labels", c.labels}};
}
void from_json(nlohmann::json const& j, JobConfiguration& c) {
  if (j.contains("jobType")) j.at("jobType").get_to(c.job_type);
  if (j.contains("query")) j.at("query").get_to(c.query);
  if (j.contains("dryRun")) j.at("dryRun").get_to(c.dry_run);
  if (j.contains("jobTimeoutMs")) j.at("jobTimeoutMs").get_to(c.job_timeout_ms);
  if (j.contains("labels")) j.at("labels").get_to(c.labels);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
