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

#include "google/cloud/bigquery/v2/minimal/internal/project.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string ProjectReference::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id)
      .Build();
}

std::string Project::DebugString(absl::string_view name,
                                 TracingOptions const& options,
                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("id", id)
      .StringField("friendly_name", friendly_name)
      .SubMessage("project_reference", project_reference)
      .Field("numeric_id", numeric_id)
      .Build();
}

void to_json(nlohmann::json& j, ProjectReference const& p) {
  j = nlohmann::json{{"projectId", p.project_id}};
}
void from_json(nlohmann::json const& j, ProjectReference& p) {
  SafeGetTo(p.project_id, j, "projectId");
}

void to_json(nlohmann::json& j, Project const& p) {
  j = nlohmann::json{{"kind", p.kind},
                     {"id", p.id},
                     {"friendlyName", p.friendly_name},
                     {"numericId", std::to_string(p.numeric_id)},
                     {"projectReference", p.project_reference}};
}
void from_json(nlohmann::json const& j, Project& p) {
  SafeGetTo(p.kind, j, "kind");
  SafeGetTo(p.id, j, "id");
  SafeGetTo(p.friendly_name, j, "friendlyName");
  p.numeric_id = GetNumberFromJson(j, "numericId");
  SafeGetTo(p.project_reference, j, "projectReference");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
