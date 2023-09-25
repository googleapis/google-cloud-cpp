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

#include "google/cloud/bigquery/v2/minimal/internal/table.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string CloneDefinition::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("base_table_reference", base_table_reference)
      .Field("clone_time", clone_time)
      .Build();
}

std::string Table::DebugString(absl::string_view name,
                               TracingOptions const& options,
                               int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .StringField("id", id)
      .StringField("self_link", self_link)
      .StringField("friendly_name", friendly_name)
      .StringField("description", description)
      .StringField("type", type)
      .StringField("location", location)
      .StringField("default_collation", default_collation)
      .StringField("max_staleness", max_staleness)
      .Field("require_partition_filter", require_partition_filter)
      .Field("creation_time", creation_time)
      .Field("expiration_time", expiration_time)
      .Field("last_modified_time", last_modified_time)
      .Field("num_time_travel_physical_bytes", num_time_travel_physical_bytes)
      .Field("num_total_logical_bytes", num_total_logical_bytes)
      .Field("num_active_logical_bytes", num_active_logical_bytes)
      .Field("num_long_term_logical_bytes", num_long_term_logical_bytes)
      .Field("num_total_physical_bytes", num_total_physical_bytes)
      .Field("num_active_physical_bytes", num_active_physical_bytes)
      .Field("num_long_term_physical_bytes", num_long_term_physical_bytes)
      .Field("num_partitions", num_partitions)
      .Field("num_bytes", num_bytes)
      .Field("num_physical_bytes", num_physical_bytes)
      .Field("num_long_term_bytes", num_long_term_bytes)
      .Field("labels", labels)
      .SubMessage("table_reference", table_reference)
      .SubMessage("schema", schema)
      .SubMessage("default_rounding_mode", default_rounding_mode)
      .SubMessage("time_partitioning", time_partitioning)
      .SubMessage("range_partitioning", range_partitioning)
      .SubMessage("clustering", clustering)
      .SubMessage("clone_definition", clone_definition)
      .SubMessage("table_constraints", table_constraints)
      .SubMessage("view", view)
      .SubMessage("materialized_view", materialized_view)
      .SubMessage("materialized_view_status", materialized_view_status)
      .Build();
}

std::string ListFormatView::DebugString(absl::string_view name,
                                        TracingOptions const& options,
                                        int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("use_legacy_sql", use_legacy_sql)
      .Build();
}

std::string HivePartitioningOptions::DebugString(absl::string_view name,
                                                 TracingOptions const& options,
                                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("mode", mode)
      .StringField("source_uri_prefix", source_uri_prefix)
      .Field("require_partition_filter", require_partition_filter)
      .Field("fields", fields)
      .Build();
}

std::string ListFormatTable::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("id", id)
      .StringField("friendly_name", friendly_name)
      .StringField("type", type)
      .SubMessage("table_reference", table_reference)
      .SubMessage("time_partitioning", time_partitioning)
      .SubMessage("range_partitioning", range_partitioning)
      .SubMessage("clustering", clustering)
      .SubMessage("hive_partitioning_options", hive_partitioning_options)
      .SubMessage("view", view)
      .Field("labels", labels)
      .Field("creation_time", creation_time)
      .Field("expiration_time", expiration_time)
      .Build();
}

void to_json(nlohmann::json& j, CloneDefinition const& c) {
  j = nlohmann::json{{"baseTableReference", c.base_table_reference}};
  ToJson(c.clone_time, j, "cloneTime");
}

void from_json(nlohmann::json const& j, CloneDefinition& c) {
  SafeGetTo(c.base_table_reference, j, "baseTableReference");
  FromJson(c.clone_time, j, "cloneTime");
}

void to_json(nlohmann::json& j, ListFormatTable const& t) {
  j = nlohmann::json{{"kind", t.kind},
                     {"id", t.id},
                     {"friendlyName", t.friendly_name},
                     {"type", t.type},
                     {"tableReference", t.table_reference},
                     {"timePartitioning", t.time_partitioning},
                     {"rangePartitioning", t.range_partitioning},
                     {"clustering", t.clustering},
                     {"hivePartitioningOptions", t.hive_partitioning_options},
                     {"view", t.view},
                     {"labels", t.labels}};
  ToJson(t.creation_time, j, "creationTime");
  ToJson(t.expiration_time, j, "expirationTime");
}

void from_json(nlohmann::json const& j, ListFormatTable& t) {
  SafeGetTo(t.kind, j, "kind");
  SafeGetTo(t.id, j, "id");
  SafeGetTo(t.friendly_name, j, "friendlyName");
  SafeGetTo(t.type, j, "type");
  SafeGetTo(t.table_reference, j, "tableReference");
  SafeGetTo(t.time_partitioning, j, "timePartitioning");
  SafeGetTo(t.range_partitioning, j, "rangePartitioning");
  SafeGetTo(t.clustering, j, "clustering");
  SafeGetTo(t.hive_partitioning_options, j, "hivePartitioningOptions");
  SafeGetTo(t.view, j, "view");
  SafeGetTo(t.labels, j, "labels");

  FromJson(t.creation_time, j, "creationTime");
  FromJson(t.expiration_time, j, "expirationTime");
}

void to_json(nlohmann::json& j, Table const& t) {
  j = nlohmann::json{
      {"kind", t.kind},
      {"etag", t.etag},
      {"id", t.id},
      {"selfLink", t.self_link},
      {"friendlyName", t.friendly_name},
      {"description", t.description},
      {"type", t.type},
      {"location", t.location},
      {"defaultCollation", t.default_collation},
      {"maxStaleness", t.max_staleness},
      {"requirePartitionFilter", t.require_partition_filter},
      {"numTimeTravelPhysicalBytes",
       std::to_string(t.num_time_travel_physical_bytes)},
      {"numTotalLogicalBytes", std::to_string(t.num_total_logical_bytes)},
      {"numActiveLogicalBytes", std::to_string(t.num_active_logical_bytes)},
      {"numLongTermLogicalBytes",
       std::to_string(t.num_long_term_logical_bytes)},
      {"numTotalPhysicalBytes", std::to_string(t.num_total_physical_bytes)},
      {"numActivePhysicalBytes", std::to_string(t.num_active_physical_bytes)},
      {"numLongTermPhysicalBytes",
       std::to_string(t.num_long_term_physical_bytes)},
      {"numPartitions", std::to_string(t.num_partitions)},
      {"numBytes", std::to_string(t.num_bytes)},
      {"numPhysicalBytes", std::to_string(t.num_physical_bytes)},
      {"numLongTermBytes", std::to_string(t.num_long_term_bytes)},
      {"numRows", std::to_string(t.num_rows)},
      {"labels", t.labels},
      {"tableReference", t.table_reference},
      {"schema", t.schema},
      {"defaultRoundingMode", t.default_rounding_mode.value},
      {"timePartitioning", t.time_partitioning},
      {"rangePartitioning", t.range_partitioning},
      {"clustering", t.clustering},
      {"cloneDefinition", t.clone_definition},
      {"tableConstraints", t.table_constraints},
      {"view", t.view},
      {"materializedView", t.materialized_view},
      {"materializedViewStatus", t.materialized_view_status}};

  ToJson(t.last_modified_time, j, "lastModifiedTime");
  ToJson(t.expiration_time, j, "expirationTime");
  ToJson(t.creation_time, j, "creationTime");
}

void from_json(nlohmann::json const& j, Table& t) {
  SafeGetTo(t.kind, j, "kind");
  SafeGetTo(t.etag, j, "etag");
  SafeGetTo(t.id, j, "id");
  SafeGetTo(t.self_link, j, "selfLink");
  SafeGetTo(t.friendly_name, j, "friendlyName");
  SafeGetTo(t.description, j, "description");
  SafeGetTo(t.type, j, "type");
  SafeGetTo(t.location, j, "location");
  SafeGetTo(t.default_collation, j, "defaultCollation");
  SafeGetTo(t.max_staleness, j, "maxStaleness");
  SafeGetTo(t.require_partition_filter, j, "requirePartitionFilter");
  t.num_time_travel_physical_bytes =
      GetNumberFromJson(j, "numTimeTravelPhysicalBytes");
  t.num_total_logical_bytes = GetNumberFromJson(j, "numTotalLogicalBytes");
  t.num_active_logical_bytes = GetNumberFromJson(j, "numActiveLogicalBytes");
  t.num_long_term_logical_bytes =
      GetNumberFromJson(j, "numLongTermLogicalBytes");
  t.num_total_physical_bytes = GetNumberFromJson(j, "numTotalPhysicalBytes");
  t.num_active_physical_bytes = GetNumberFromJson(j, "numActivePhysicalBytes");
  t.num_long_term_physical_bytes =
      GetNumberFromJson(j, "numLongTermPhysicalBytes");
  t.num_partitions = GetNumberFromJson(j, "numPartitions");
  t.num_bytes = GetNumberFromJson(j, "numBytes");
  t.num_physical_bytes = GetNumberFromJson(j, "numPhysicalBytes");
  t.num_long_term_bytes = GetNumberFromJson(j, "numLongTermBytes");
  t.num_rows = GetNumberFromJson(j, "numRows");
  SafeGetTo(t.labels, j, "labels");
  SafeGetTo(t.table_reference, j, "tableReference");
  SafeGetTo(t.schema, j, "schema");
  SafeGetTo(t.default_rounding_mode.value, j, "defaultRoundingMode");
  SafeGetTo(t.time_partitioning, j, "timePartitioning");
  SafeGetTo(t.range_partitioning, j, "rangePartitioning");
  SafeGetTo(t.clustering, j, "clustering");
  SafeGetTo(t.clone_definition, j, "cloneDefinition");
  SafeGetTo(t.table_constraints, j, "tableConstraints");
  SafeGetTo(t.view, j, "view");
  SafeGetTo(t.materialized_view, j, "materializedView");
  SafeGetTo(t.materialized_view_status, j, "materializedViewStatus");

  FromJson(t.last_modified_time, j, "lastModifiedTime");
  FromJson(t.expiration_time, j, "expirationTime");
  FromJson(t.creation_time, j, "creationTime");
}

void to_json(nlohmann::json& j, HivePartitioningOptions const& h) {
  j = nlohmann::json{{"mode", h.mode},
                     {"sourceUriPrefix", h.source_uri_prefix},
                     {"requirePartitionFilter", h.require_partition_filter},
                     {"fields", h.fields}};
}
void from_json(nlohmann::json const& j, HivePartitioningOptions& h) {
  SafeGetTo(h.mode, j, "mode");
  SafeGetTo(h.source_uri_prefix, j, "sourceUriPrefix");
  SafeGetTo(h.require_partition_filter, j, "requirePartitionFilter");
  SafeGetTo(h.fields, j, "fields");
}

void to_json(nlohmann::json& j, ListFormatView const& v) {
  j = nlohmann::json{{"useLegacySql", v.use_legacy_sql}};
}
void from_json(nlohmann::json const& j, ListFormatView& v) {
  SafeGetTo(v.use_legacy_sql, j, "useLegacySql");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
