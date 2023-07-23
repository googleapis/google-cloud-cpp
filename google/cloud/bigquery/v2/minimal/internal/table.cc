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
  if (j.contains("baseTableReference")) {
    j.at("baseTableReference").get_to(c.base_table_reference);
  }
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
  if (j.contains("kind")) j.at("kind").get_to(t.kind);
  if (j.contains("id")) j.at("id").get_to(t.id);
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("friendlyName")) j.at("friendlyName").get_to(t.friendly_name);
  if (j.contains("tableReference")) {
    j.at("tableReference").get_to(t.table_reference);
  }
  if (j.contains("timePartitioning")) {
    j.at("timePartitioning").get_to(t.time_partitioning);
  }
  if (j.contains("rangePartitioning")) {
    j.at("rangePartitioning").get_to(t.range_partitioning);
  }
  if (j.contains("clustering")) j.at("clustering").get_to(t.clustering);
  if (j.contains("hivePartitioningOptions")) {
    j.at("hivePartitioningOptions").get_to(t.hive_partitioning_options);
  }
  if (j.contains("view")) j.at("view").get_to(t.view);
  if (j.contains("labels")) j.at("labels").get_to(t.labels);

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
      {"numTimeTravelPhysicalBytes", t.num_time_travel_physical_bytes},
      {"numTotalLogicalBytes", t.num_total_logical_bytes},
      {"numActiveLogicalBytes", t.num_active_logical_bytes},
      {"numLongTermLogicalBytes", t.num_long_term_logical_bytes},
      {"numTotalPhysicalBytes", t.num_total_physical_bytes},
      {"numActivePhysicalBytes", t.num_active_physical_bytes},
      {"numLongTermPhysicalBytes", t.num_long_term_physical_bytes},
      {"numPartitions", t.num_partitions},
      {"numBytes", t.num_bytes},
      {"numPhysicalBytes", t.num_physical_bytes},
      {"numLongTermBytes", t.num_long_term_bytes},
      {"numRows", t.num_rows},
      {"labels", t.labels},
      {"tableReference", t.table_reference},
      {"schema", t.schema},
      {"defaultRoundingMode", t.default_rounding_mode},
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
  if (j.contains("kind")) j.at("kind").get_to(t.kind);
  if (j.contains("etag")) j.at("etag").get_to(t.etag);
  if (j.contains("id")) j.at("id").get_to(t.id);
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("selfLink")) j.at("selfLink").get_to(t.self_link);
  if (j.contains("friendlyName")) j.at("friendlyName").get_to(t.friendly_name);
  if (j.contains("description")) j.at("description").get_to(t.description);
  if (j.contains("location")) j.at("location").get_to(t.location);
  if (j.contains("defaultCollation")) {
    j.at("defaultCollation").get_to(t.default_collation);
  }
  if (j.contains("requirePartitionFilter")) {
    j.at("requirePartitionFilter").get_to(t.require_partition_filter);
  }
  if (j.contains("maxStaleness")) j.at("maxStaleness").get_to(t.max_staleness);
  if (j.contains("require_partition_filter")) {
    j.at("require_partition_filter").get_to(t.require_partition_filter);
  }
  if (j.contains("numTimeTravelPhysicalBytes")) {
    j.at("numTimeTravelPhysicalBytes").get_to(t.num_time_travel_physical_bytes);
  }
  if (j.contains("numTotalLogicalBytes")) {
    j.at("numTotalLogicalBytes").get_to(t.num_total_logical_bytes);
  }
  if (j.contains("numActiveLogicalBytes")) {
    j.at("numActiveLogicalBytes").get_to(t.num_active_logical_bytes);
  }
  if (j.contains("numLongTermLogicalBytes")) {
    j.at("numLongTermLogicalBytes").get_to(t.num_long_term_logical_bytes);
  }
  if (j.contains("numTotalPhysicalBytes")) {
    j.at("numTotalPhysicalBytes").get_to(t.num_total_physical_bytes);
  }
  if (j.contains("numActivePhysicalBytes")) {
    j.at("numActivePhysicalBytes").get_to(t.num_active_physical_bytes);
  }
  if (j.contains("numLongTermPhysicalBytes")) {
    j.at("numLongTermPhysicalBytes").get_to(t.num_long_term_physical_bytes);
  }
  if (j.contains("numPartitions")) {
    j.at("numPartitions").get_to(t.num_partitions);
  }
  if (j.contains("numBytes")) j.at("numBytes").get_to(t.num_bytes);
  if (j.contains("numPhysicalBytes")) {
    j.at("numPhysicalBytes").get_to(t.num_physical_bytes);
  }
  if (j.contains("numLongTermBytes")) {
    j.at("numLongTermBytes").get_to(t.num_long_term_bytes);
  }
  if (j.contains("numRows")) j.at("numRows").get_to(t.num_rows);
  if (j.contains("labels")) j.at("labels").get_to(t.labels);
  if (j.contains("tableReference")) {
    j.at("tableReference").get_to(t.table_reference);
  }
  if (j.contains("schema")) j.at("schema").get_to(t.schema);
  if (j.contains("defaultRoundingMode")) {
    j.at("defaultRoundingMode").get_to(t.default_rounding_mode);
  }
  if (j.contains("timePartitioning")) {
    j.at("timePartitioning").get_to(t.time_partitioning);
  }
  if (j.contains("rangePartitioning")) {
    j.at("rangePartitioning").get_to(t.range_partitioning);
  }
  if (j.contains("clustering")) j.at("clustering").get_to(t.clustering);
  if (j.contains("cloneDefinition")) {
    j.at("cloneDefinition").get_to(t.clone_definition);
  }
  if (j.contains("tableConstraints")) {
    j.at("tableConstraints").get_to(t.table_constraints);
  }
  if (j.contains("view")) j.at("view").get_to(t.view);
  if (j.contains("materializedView")) {
    j.at("materializedView").get_to(t.materialized_view);
  }
  if (j.contains("materializedViewStatus")) {
    j.at("materializedViewStatus").get_to(t.materialized_view_status);
  }

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
  // TODO(#12188): Implement SafeGetTo(...) for potential performance
  // improvement.
  if (j.contains("mode")) j.at("mode").get_to(h.mode);
  if (j.contains("sourceUriPrefix")) {
    j.at("sourceUriPrefix").get_to(h.source_uri_prefix);
  }
  if (j.contains("requirePartitionFilter")) {
    j.at("requirePartitionFilter").get_to(h.require_partition_filter);
  }
  if (j.contains("fields")) j.at("fields").get_to(h.fields);
}

void to_json(nlohmann::json& j, ListFormatView const& v) {
  j = nlohmann::json{{"useLegacySql", v.use_legacy_sql}};
}
void from_json(nlohmann::json const& j, ListFormatView& v) {
  // TODO(#12188): Implement SafeGetTo(...) for potential performance
  // improvement.
  if (j.contains("useLegacySql")) j.at("useLegacySql").get_to(v.use_legacy_sql);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
