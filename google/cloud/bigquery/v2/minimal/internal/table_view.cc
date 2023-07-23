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

#include "google/cloud/bigquery/v2/minimal/internal/table_view.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TableMetadataView TableMetadataView::UnSpecified() {
  return TableMetadataView{"TABLE_METADATA_VIEW_UNSPECIFIED"};
}

TableMetadataView TableMetadataView::Basic() {
  return TableMetadataView{"BASIC"};
}

TableMetadataView TableMetadataView::StorageStats() {
  return TableMetadataView{"STORAGE_STATS"};
}

TableMetadataView TableMetadataView::Full() {
  return TableMetadataView{"FULL"};
}

std::string TableMetadataView::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string UserDefinedFunctionResource::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("resource_uri", resource_uri)
      .StringField("inline_code", inline_code)
      .Build();
}

std::string ViewDefinition::DebugString(absl::string_view name,
                                        TracingOptions const& options,
                                        int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("query", query)
      .Field("use_legacy_sql", use_legacy_sql)
      .Field("user_defined_function_resources", user_defined_function_resources)
      .Build();
}

std::string MaterializedViewDefinition::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("query", query)
      .Field("enable_refresh", enable_refresh)
      .Field("refresh_interval_time", refresh_interval_time)
      .Field("last_refresh_time", last_refresh_time)
      .Build();
}

std::string MaterializedViewStatus::DebugString(absl::string_view name,
                                                TracingOptions const& options,
                                                int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("last_refresh_status", last_refresh_status)
      .Field("refresh_watermark", refresh_watermark)
      .Build();
}

void to_json(nlohmann::json& j, MaterializedViewDefinition const& m) {
  j = nlohmann::json{{"query", m.query}, {"enableRefresh", m.enable_refresh}};

  ToJson(m.refresh_interval_time, j, "refreshIntervalMs");
  ToJson(m.last_refresh_time, j, "lastRefreshTime");
}
void from_json(nlohmann::json const& j, MaterializedViewDefinition& m) {
  if (j.contains("query")) j.at("query").get_to(m.query);
  if (j.contains("enableRefresh"))
    j.at("enableRefresh").get_to(m.enable_refresh);

  FromJson(m.refresh_interval_time, j, "refreshIntervalMs");
  FromJson(m.last_refresh_time, j, "lastRefreshTime");
}

void to_json(nlohmann::json& j, MaterializedViewStatus const& m) {
  j = nlohmann::json{{"lastRefreshStatus", m.last_refresh_status}};
  ToJson(m.refresh_watermark, j, "refreshWatermark");
}
void from_json(nlohmann::json const& j, MaterializedViewStatus& m) {
  if (j.contains("lastRefreshStatus"))
    j.at("lastRefreshStatus").get_to(m.last_refresh_status);
  FromJson(m.refresh_watermark, j, "refreshWatermark");
}

void to_json(nlohmann::json& j, ViewDefinition const& v) {
  j = nlohmann::json{
      {"query", v.query},
      {"useLegacySql", v.use_legacy_sql},
      {"userDefinedFunctionResources", v.user_defined_function_resources}};
}
void from_json(nlohmann::json const& j, ViewDefinition& v) {
  // TODO(#12188): Implement SafeGetTo(...) for potential performance
  // improvement.
  if (j.contains("query")) j.at("query").get_to(v.query);
  if (j.contains("useLegacySql")) j.at("useLegacySql").get_to(v.use_legacy_sql);
  if (j.contains("userDefinedFunctionResources")) {
    j.at("userDefinedFunctionResources")
        .get_to(v.user_defined_function_resources);
  }
}

void to_json(nlohmann::json& j, UserDefinedFunctionResource const& u) {
  j = nlohmann::json{{"resourceUri", u.resource_uri},
                     {"inlineCode", u.inline_code}};
}
void from_json(nlohmann::json const& j, UserDefinedFunctionResource& u) {
  // TODO(#12188): Implement SafeGetTo(...) for potential performance
  // improvement.
  if (j.contains("resourceUri")) j.at("resourceUri").get_to(u.resource_uri);
  if (j.contains("inlineCode")) j.at("inlineCode").get_to(u.inline_code);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
