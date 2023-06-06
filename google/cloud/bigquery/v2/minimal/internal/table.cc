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
      .Field("case_insensitive", case_insensitive)
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
  j = nlohmann::json{{"base_table_reference", c.base_table_reference}};
  ToJsonTimepoint(c.clone_time, j, "clone_time");
}

void from_json(nlohmann::json const& j, CloneDefinition& c) {
  if (j.contains("base_table_reference"))
    j.at("base_table_reference").get_to(c.base_table_reference);
  FromJsonTimepoint(c.clone_time, j, "clone_time");
}

void to_json(nlohmann::json& j, ListFormatTable const& t) {
  j = nlohmann::json{{"kind", t.kind},
                     {"id", t.id},
                     {"friendly_name", t.friendly_name},
                     {"type", t.type},
                     {"table_reference", t.table_reference},
                     {"time_partitioning", t.time_partitioning},
                     {"range_partitioning", t.range_partitioning},
                     {"clustering", t.clustering},
                     {"hive_partitioning_options", t.hive_partitioning_options},
                     {"view", t.view},
                     {"labels", t.labels}};
  ToJsonMilliseconds(t.creation_time, j, "creation_time");
  ToJsonMilliseconds(t.expiration_time, j, "expiration_time");
}

void from_json(nlohmann::json const& j, ListFormatTable& t) {
  if (j.contains("kind")) j.at("kind").get_to(t.kind);
  if (j.contains("id")) j.at("id").get_to(t.id);
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("friendly_name"))
    j.at("friendly_name").get_to(t.friendly_name);
  if (j.contains("table_reference"))
    j.at("table_reference").get_to(t.table_reference);
  if (j.contains("time_partitioning"))
    j.at("time_partitioning").get_to(t.time_partitioning);
  if (j.contains("range_partitioning"))
    j.at("range_partitioning").get_to(t.range_partitioning);
  if (j.contains("clustering")) j.at("clustering").get_to(t.clustering);
  if (j.contains("hive_partitioning_options"))
    j.at("hive_partitioning_options").get_to(t.hive_partitioning_options);
  if (j.contains("view")) j.at("view").get_to(t.view);
  if (j.contains("labels")) j.at("labels").get_to(t.labels);
  if (j.contains("friendly_name"))
    j.at("friendly_name").get_to(t.friendly_name);

  FromJsonMilliseconds(t.creation_time, j, "creation_time");
  FromJsonMilliseconds(t.expiration_time, j, "expiration_time");
}

void to_json(nlohmann::json& j, Table const& t) {
  j = nlohmann::json{
      {"kind", t.kind},
      {"etag", t.etag},
      {"id", t.id},
      {"self_link", t.self_link},
      {"friendly_name", t.friendly_name},
      {"description", t.description},
      {"type", t.type},
      {"location", t.location},
      {"default_collation", t.default_collation},
      {"max_staleness", t.max_staleness},
      {"require_partition_filter", t.require_partition_filter},
      {"case_insensitive", t.case_insensitive},
      {"num_time_travel_physical_bytes", t.num_time_travel_physical_bytes},
      {"num_total_logical_bytes", t.num_total_logical_bytes},
      {"num_active_logical_bytes", t.num_active_logical_bytes},
      {"num_long_term_logical_bytes", t.num_long_term_logical_bytes},
      {"num_total_physical_bytes", t.num_total_physical_bytes},
      {"num_active_physical_bytes", t.num_active_physical_bytes},
      {"num_long_term_physical_bytes", t.num_long_term_physical_bytes},
      {"num_partitions", t.num_partitions},
      {"num_bytes", t.num_bytes},
      {"num_physical_bytes", t.num_physical_bytes},
      {"num_long_term_bytes", t.num_long_term_bytes},
      {"num_rows", t.num_rows},
      {"labels", t.labels},
      {"table_reference", t.table_reference},
      {"schema", t.schema},
      {"default_rounding_mode", t.default_rounding_mode},
      {"time_partitioning", t.time_partitioning},
      {"range_partitioning", t.range_partitioning},
      {"clustering", t.clustering},
      {"clone_definition", t.clone_definition},
      {"table_constraints", t.table_constraints},
      {"view", t.view},
      {"materialized_view", t.materialized_view},
      {"materialized_view_status", t.materialized_view_status}};

  ToJsonTimepoint(t.last_modified_time, j, "last_modified_time");
  ToJsonTimepoint(t.expiration_time, j, "expiration_time");
  ToJsonTimepoint(t.creation_time, j, "creation_time");
}

void from_json(nlohmann::json const& j, Table& t) {
  if (j.contains("kind")) j.at("kind").get_to(t.kind);
  if (j.contains("etag")) j.at("etag").get_to(t.etag);
  if (j.contains("id")) j.at("id").get_to(t.id);
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("self_link")) j.at("self_link").get_to(t.self_link);
  if (j.contains("friendly_name"))
    j.at("friendly_name").get_to(t.friendly_name);
  if (j.contains("description")) j.at("description").get_to(t.description);
  if (j.contains("location")) j.at("location").get_to(t.location);
  if (j.contains("default_collation"))
    j.at("default_collation").get_to(t.default_collation);
  if (j.contains("max_staleness"))
    j.at("max_staleness").get_to(t.max_staleness);
  if (j.contains("require_partition_filter"))
    j.at("require_partition_filter").get_to(t.require_partition_filter);
  if (j.contains("case_insensitive"))
    j.at("case_insensitive").get_to(t.case_insensitive);
  if (j.contains("num_time_travel_physical_bytes"))
    j.at("num_time_travel_physical_bytes")
        .get_to(t.num_time_travel_physical_bytes);
  if (j.contains("num_total_logical_bytes"))
    j.at("num_total_logical_bytes").get_to(t.num_total_logical_bytes);
  if (j.contains("num_active_logical_bytes"))
    j.at("num_active_logical_bytes").get_to(t.num_active_logical_bytes);
  if (j.contains("num_long_term_logical_bytes"))
    j.at("num_long_term_logical_bytes").get_to(t.num_long_term_logical_bytes);
  if (j.contains("num_total_physical_bytes"))
    j.at("num_total_physical_bytes").get_to(t.num_total_physical_bytes);
  if (j.contains("num_active_physical_bytes"))
    j.at("num_active_physical_bytes").get_to(t.num_active_physical_bytes);
  if (j.contains("num_long_term_physical_bytes"))
    j.at("num_long_term_physical_bytes").get_to(t.num_long_term_physical_bytes);
  if (j.contains("num_partitions"))
    j.at("num_partitions").get_to(t.num_partitions);
  if (j.contains("num_bytes")) j.at("num_bytes").get_to(t.num_bytes);
  if (j.contains("num_physical_bytes"))
    j.at("num_physical_bytes").get_to(t.num_physical_bytes);
  if (j.contains("num_long_term_bytes"))
    j.at("num_long_term_bytes").get_to(t.num_long_term_bytes);
  if (j.contains("num_rows")) j.at("num_rows").get_to(t.num_rows);
  if (j.contains("labels")) j.at("labels").get_to(t.labels);
  if (j.contains("table_reference"))
    j.at("table_reference").get_to(t.table_reference);
  if (j.contains("schema")) j.at("schema").get_to(t.schema);
  if (j.contains("default_rounding_mode"))
    j.at("default_rounding_mode").get_to(t.default_rounding_mode);
  if (j.contains("time_partitioning"))
    j.at("time_partitioning").get_to(t.time_partitioning);
  if (j.contains("range_partitioning"))
    j.at("range_partitioning").get_to(t.range_partitioning);
  if (j.contains("clustering")) j.at("clustering").get_to(t.clustering);
  if (j.contains("clone_definition"))
    j.at("clone_definition").get_to(t.clone_definition);
  if (j.contains("table_constraints"))
    j.at("table_constraints").get_to(t.table_constraints);
  if (j.contains("view")) j.at("view").get_to(t.view);
  if (j.contains("materialized_view"))
    j.at("materialized_view").get_to(t.materialized_view);
  if (j.contains("materialized_view_status"))
    j.at("materialized_view_status").get_to(t.materialized_view_status);

  FromJsonTimepoint(t.last_modified_time, j, "last_modified_time");
  FromJsonTimepoint(t.expiration_time, j, "expiration_time");
  FromJsonTimepoint(t.creation_time, j, "creation_time");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
