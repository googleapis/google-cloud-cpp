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

TableMetadataView TableMetadataView::StrorageStats() {
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
      .Field("use_explicit_column_names", use_explicit_column_names)
      .Field("user_defined_function_resources", user_defined_function_resources)
      .Build();
}

std::string MaterializedViewDefinition::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("query", query)
      .StringField("max_staleness", max_staleness)
      .Field("allow_non_incremental_definition",
             allow_non_incremental_definition)
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
  j = nlohmann::json{
      {"query", m.query},
      {"max_staleness", m.max_staleness},
      {"allow_non_incremental_definition", m.allow_non_incremental_definition},
      {"enable_refresh", m.enable_refresh}};

  ToJsonMilliseconds(m.refresh_interval_time, j, "refresh_interval");
  ToJsonTimepoint(m.last_refresh_time, j, "last_refresh_time");
}
void from_json(nlohmann::json const& j, MaterializedViewDefinition& m) {
  if (j.contains("query")) j.at("query").get_to(m.query);
  if (j.contains("max_staleness"))
    j.at("max_staleness").get_to(m.max_staleness);
  if (j.contains("allow_non_incremental_definition"))
    j.at("allow_non_incremental_definition")
        .get_to(m.allow_non_incremental_definition);
  if (j.contains("enable_refresh"))
    j.at("enable_refresh").get_to(m.enable_refresh);

  FromJsonMilliseconds(m.refresh_interval_time, j, "refresh_interval");
  FromJsonTimepoint(m.last_refresh_time, j, "last_refresh_time");
}

void to_json(nlohmann::json& j, MaterializedViewStatus const& m) {
  j = nlohmann::json{{"last_refresh_status", m.last_refresh_status}};
  ToJsonTimepoint(m.refresh_watermark, j, "refresh_watermark");
}
void from_json(nlohmann::json const& j, MaterializedViewStatus& m) {
  if (j.contains("last_refresh_status"))
    j.at("last_refresh_status").get_to(m.last_refresh_status);
  FromJsonTimepoint(m.refresh_watermark, j, "refresh_watermark");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
