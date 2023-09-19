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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_constraints.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_partition.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_schema.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_view.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct CloneDefinition {
  TableReference base_table_reference;
  std::chrono::system_clock::time_point clone_time;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, CloneDefinition const& c);
void from_json(nlohmann::json const& j, CloneDefinition& c);

struct Table {
  std::string kind;
  std::string etag;
  std::string id;
  std::string self_link;
  std::string friendly_name;
  std::string description;
  std::string type;
  std::string location;
  std::string default_collation;
  std::string max_staleness;

  std::int64_t num_time_travel_physical_bytes;
  std::int64_t num_total_logical_bytes;
  std::int64_t num_active_logical_bytes;
  std::int64_t num_long_term_logical_bytes;
  std::int64_t num_total_physical_bytes;
  std::int64_t num_active_physical_bytes;
  std::int64_t num_long_term_physical_bytes;
  std::int64_t num_partitions;
  std::int64_t num_bytes;
  std::int64_t num_physical_bytes;
  std::int64_t num_long_term_bytes;
  std::uint64_t num_rows;

  bool require_partition_filter = false;

  std::chrono::system_clock::time_point creation_time;
  std::chrono::system_clock::time_point expiration_time;
  std::chrono::system_clock::time_point last_modified_time;

  std::map<std::string, std::string> labels;

  TableReference table_reference;
  TableSchema schema;

  RoundingMode default_rounding_mode;
  TimePartitioning time_partitioning;
  RangePartitioning range_partitioning;
  Clustering clustering;
  CloneDefinition clone_definition;
  TableConstraints table_constraints;

  ViewDefinition view;
  MaterializedViewDefinition materialized_view;
  MaterializedViewStatus materialized_view_status;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, Table const& t);
void from_json(nlohmann::json const& j, Table& t);

struct ListFormatView {
  bool use_legacy_sql = false;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ListFormatView const& v);
void from_json(nlohmann::json const& j, ListFormatView& v);

struct HivePartitioningOptions {
  std::string mode;
  std::string source_uri_prefix;

  bool require_partition_filter = false;

  std::vector<std::string> fields;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, HivePartitioningOptions const& h);
void from_json(nlohmann::json const& j, HivePartitioningOptions& h);

struct ListFormatTable {
  std::string kind;
  std::string id;
  std::string friendly_name;
  std::string type;

  TableReference table_reference;
  TimePartitioning time_partitioning;
  RangePartitioning range_partitioning;
  Clustering clustering;
  HivePartitioningOptions hive_partitioning_options;
  ListFormatView view;

  std::map<std::string, std::string> labels;
  std::chrono::milliseconds creation_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds expiration_time = std::chrono::milliseconds(0);

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ListFormatTable const& t);
void from_json(nlohmann::json const& j, ListFormatTable& t);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_H
