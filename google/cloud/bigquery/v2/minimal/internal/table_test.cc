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

#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(TableTest, TableToJson) {
  auto const text = bigquery_v2_minimal_testing::MakeTableJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input = bigquery_v2_minimal_testing::MakeTable();

  nlohmann::json actual_json;
  to_json(actual_json, input);

  EXPECT_EQ(expected_json.dump(), actual_json.dump());
}

TEST(TableTest, TableFromJson) {
  auto const text = bigquery_v2_minimal_testing::MakeTableJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  Table expected = bigquery_v2_minimal_testing::MakeTable();

  Table actual;
  from_json(json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

TEST(TableTest, ListFormatTableToJson) {
  auto const text = bigquery_v2_minimal_testing::MakeListFormatTableJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input = bigquery_v2_minimal_testing::MakeListFormatTable();

  nlohmann::json actual_json;
  to_json(actual_json, input);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(TableTest, ListFormatTableFromJson) {
  auto const text = bigquery_v2_minimal_testing::MakeListFormatTableJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  ListFormatTable expected = bigquery_v2_minimal_testing::MakeListFormatTable();

  ListFormatTable actual;
  from_json(json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

TEST(TableTest, TableDebugString) {
  Table table = bigquery_v2_minimal_testing::MakeTable();

  EXPECT_EQ(table.DebugString("Table", TracingOptions{}),
            R"(Table {)"
            R"( kind: "t-kind")"
            R"( etag: "t-etag")"
            R"( id: "t-id")"
            R"( self_link: "t-selflink")"
            R"( friendly_name: "t-friendlyname")"
            R"( description: "t-description")"
            R"( type: "t-type")"
            R"( location: "t-location")"
            R"( default_collation: "t-defaultcollation")"
            R"( max_staleness: "stale")"
            R"( require_partition_filter: true)"
            R"( creation_time { "1970-01-01T00:00:00.001Z" })"
            R"( expiration_time { "1970-01-01T00:00:00.001Z" })"
            R"( last_modified_time { "1970-01-01T00:00:00.001Z" })"
            R"( num_time_travel_physical_bytes: 1)"
            R"( num_total_logical_bytes: 1)"
            R"( num_active_logical_bytes: 1)"
            R"( num_long_term_logical_bytes: 1)"
            R"( num_total_physical_bytes: 1)"
            R"( num_active_physical_bytes: 1)"
            R"( num_long_term_physical_bytes: 1)"
            R"( num_partitions: 1)"
            R"( num_bytes: 1)"
            R"( num_physical_bytes: 1)"
            R"( num_long_term_bytes: 1)"
            R"( labels { key: "l1" value: "v1" })"
            R"( labels { key: "l2" value: "v2" })"
            R"( table_reference {)"
            R"( project_id: "t-123")"
            R"( dataset_id: "t-123")"
            R"( table_id: "t-123")"
            R"( })"
            R"( schema {)"
            R"( fields {)"
            R"( name: "fname-1")"
            R"( type: "")"
            R"( mode: "fmode")"
            R"( description: "")"
            R"( collation: "")"
            R"( default_value_expression: "")"
            R"( max_length: 0)"
            R"( precision: 0)"
            R"( scale: 0)"
            R"( categories { })"
            R"( policy_tags { })"
            R"( rounding_mode { value: "" })"
            R"( range_element_type {)"
            R"( type: "")"
            R"( })"
            R"( })"
            R"( })"
            R"( default_rounding_mode {)"
            R"( value: "ROUND_HALF_EVEN")"
            R"( })"
            R"( time_partitioning {)"
            R"( type: "")"
            R"( expiration_time {)"
            R"( "123ms")"
            R"( })"
            R"( field: "time-partition-field")"
            R"( })"
            R"( range_partitioning {)"
            R"( field: "range-partition-field")"
            R"( range {)"
            R"( start: "" end: "" interval: "")"
            R"( })"
            R"( })"
            R"( clustering {)"
            R"( fields: "c-field-1")"
            R"( })"
            R"( clone_definition {)"
            R"( base_table_reference {)"
            R"( project_id: "t-123")"
            R"( dataset_id: "t-123")"
            R"( table_id: "t-123")"
            R"( })"
            R"( clone_time { "1970-01-01T00:00:00Z" })"
            R"( })"
            R"( table_constraints {)"
            R"( primary_key {)"
            R"( columns: "pcol-1")"
            R"( })"
            R"( foreign_keys {)"
            R"( key_name: "fkey-1")"
            R"( referenced_table {)"
            R"( project_id: "" dataset_id: "" table_id: "")"
            R"( })"
            R"( })"
            R"( })"
            R"( view { query: "select 1;" use_legacy_sql: false })"
            R"( materialized_view {)"
            R"( query: "select 1;")"
            R"( enable_refresh: true)"
            R"( refresh_interval_time { "0" })"
            R"( last_refresh_time { "1970-01-01T00:00:00Z" })"
            R"( })"
            R"( materialized_view_status {)"
            R"( last_refresh_status { reason: "" location: "" message: "" })"
            R"( refresh_watermark { "1970-01-01T00:00:00.123Z" })"
            R"( })"
            R"( })");

  EXPECT_EQ(
      table.DebugString("Table", TracingOptions{}.SetOptions(
                                     "truncate_string_field_longer_than=7")),
      R"(Table {)"
      R"( kind: "t-kind")"
      R"( etag: "t-etag")"
      R"( id: "t-id")"
      R"( self_link: "t-selfl...<truncated>...")"
      R"( friendly_name: "t-frien...<truncated>...")"
      R"( description: "t-descr...<truncated>...")"
      R"( type: "t-type")"
      R"( location: "t-locat...<truncated>...")"
      R"( default_collation: "t-defau...<truncated>...")"
      R"( max_staleness: "stale")"
      R"( require_partition_filter: true)"
      R"( creation_time { "1970-01-01T00:00:00.001Z" })"
      R"( expiration_time { "1970-01-01T00:00:00.001Z" })"
      R"( last_modified_time { "1970-01-01T00:00:00.001Z" })"
      R"( num_time_travel_physical_bytes: 1)"
      R"( num_total_logical_bytes: 1)"
      R"( num_active_logical_bytes: 1)"
      R"( num_long_term_logical_bytes: 1)"
      R"( num_total_physical_bytes: 1)"
      R"( num_active_physical_bytes: 1)"
      R"( num_long_term_physical_bytes: 1)"
      R"( num_partitions: 1)"
      R"( num_bytes: 1)"
      R"( num_physical_bytes: 1)"
      R"( num_long_term_bytes: 1)"
      R"( labels { key: "l1" value: "v1" })"
      R"( labels { key: "l2" value: "v2" })"
      R"( table_reference {)"
      R"( project_id: "t-123")"
      R"( dataset_id: "t-123")"
      R"( table_id: "t-123")"
      R"( })"
      R"( schema {)"
      R"( fields {)"
      R"( name: "fname-1")"
      R"( type: "")"
      R"( mode: "fmode")"
      R"( description: "")"
      R"( collation: "")"
      R"( default_value_expression: "")"
      R"( max_length: 0)"
      R"( precision: 0)"
      R"( scale: 0)"
      R"( categories { })"
      R"( policy_tags { })"
      R"( rounding_mode { value: "" })"
      R"( range_element_type {)"
      R"( type: "")"
      R"( })"
      R"( })"
      R"( })"
      R"( default_rounding_mode {)"
      R"( value: "ROUND_H...<truncated>...")"
      R"( })"
      R"( time_partitioning {)"
      R"( type: "")"
      R"( expiration_time {)"
      R"( "123ms")"
      R"( })"
      R"( field: "time-pa...<truncated>...")"
      R"( })"
      R"( range_partitioning {)"
      R"( field: "range-p...<truncated>...")"
      R"( range {)"
      R"( start: "" end: "" interval: "")"
      R"( })"
      R"( })"
      R"( clustering {)"
      R"( fields: "c-field...<truncated>...")"
      R"( })"
      R"( clone_definition {)"
      R"( base_table_reference {)"
      R"( project_id: "t-123")"
      R"( dataset_id: "t-123")"
      R"( table_id: "t-123")"
      R"( })"
      R"( clone_time { "1970-01-01T00:00:00Z" })"
      R"( })"
      R"( table_constraints {)"
      R"( primary_key {)"
      R"( columns: "pcol-1")"
      R"( })"
      R"( foreign_keys {)"
      R"( key_name: "fkey-1")"
      R"( referenced_table {)"
      R"( project_id: "" dataset_id: "" table_id: "")"
      R"( })"
      R"( })"
      R"( })"
      R"( view { query: "select ...<truncated>..." use_legacy_sql: false })"
      R"( materialized_view {)"
      R"( query: "select ...<truncated>...")"
      R"( enable_refresh: true)"
      R"( refresh_interval_time { "0" })"
      R"( last_refresh_time { "1970-01-01T00:00:00Z" })"
      R"( })"
      R"( materialized_view_status {)"
      R"( last_refresh_status { reason: "" location: "" message: "" })"
      R"( refresh_watermark { "1970-01-01T00:00:00.123Z" })"
      R"( })"
      R"( })");

  EXPECT_EQ(table.DebugString(
                "Table", TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(Table {
  kind: "t-kind"
  etag: "t-etag"
  id: "t-id"
  self_link: "t-selflink"
  friendly_name: "t-friendlyname"
  description: "t-description"
  type: "t-type"
  location: "t-location"
  default_collation: "t-defaultcollation"
  max_staleness: "stale"
  require_partition_filter: true
  creation_time {
    "1970-01-01T00:00:00.001Z"
  }
  expiration_time {
    "1970-01-01T00:00:00.001Z"
  }
  last_modified_time {
    "1970-01-01T00:00:00.001Z"
  }
  num_time_travel_physical_bytes: 1
  num_total_logical_bytes: 1
  num_active_logical_bytes: 1
  num_long_term_logical_bytes: 1
  num_total_physical_bytes: 1
  num_active_physical_bytes: 1
  num_long_term_physical_bytes: 1
  num_partitions: 1
  num_bytes: 1
  num_physical_bytes: 1
  num_long_term_bytes: 1
  labels {
    key: "l1"
    value: "v1"
  }
  labels {
    key: "l2"
    value: "v2"
  }
  table_reference {
    project_id: "t-123"
    dataset_id: "t-123"
    table_id: "t-123"
  }
  schema {
    fields {
      name: "fname-1"
      type: ""
      mode: "fmode"
      description: ""
      collation: ""
      default_value_expression: ""
      max_length: 0
      precision: 0
      scale: 0
      categories {
      }
      policy_tags {
      }
      rounding_mode {
        value: ""
      }
      range_element_type {
        type: ""
      }
    }
  }
  default_rounding_mode {
    value: "ROUND_HALF_EVEN"
  }
  time_partitioning {
    type: ""
    expiration_time {
      "123ms"
    }
    field: "time-partition-field"
  }
  range_partitioning {
    field: "range-partition-field"
    range {
      start: ""
      end: ""
      interval: ""
    }
  }
  clustering {
    fields: "c-field-1"
  }
  clone_definition {
    base_table_reference {
      project_id: "t-123"
      dataset_id: "t-123"
      table_id: "t-123"
    }
    clone_time {
      "1970-01-01T00:00:00Z"
    }
  }
  table_constraints {
    primary_key {
      columns: "pcol-1"
    }
    foreign_keys {
      key_name: "fkey-1"
      referenced_table {
        project_id: ""
        dataset_id: ""
        table_id: ""
      }
    }
  }
  view {
    query: "select 1;"
    use_legacy_sql: false
  }
  materialized_view {
    query: "select 1;"
    enable_refresh: true
    refresh_interval_time {
      "0"
    }
    last_refresh_time {
      "1970-01-01T00:00:00Z"
    }
  }
  materialized_view_status {
    last_refresh_status {
      reason: ""
      location: ""
      message: ""
    }
    refresh_watermark {
      "1970-01-01T00:00:00.123Z"
    }
  }
})");
}

TEST(DatasetTest, ListFormatTableDebugString) {
  auto table = bigquery_v2_minimal_testing::MakeListFormatTable();

  EXPECT_EQ(table.DebugString("Table", TracingOptions{}),
            R"(Table {)"
            R"( kind: "t-kind")"
            R"( id: "t-id")"
            R"( friendly_name: "t-friendlyname")"
            R"( type: "t-type")"
            R"( table_reference {)"
            R"( project_id: "t-123")"
            R"( dataset_id: "t-123")"
            R"( table_id: "t-123")"
            R"( })"
            R"( time_partitioning {)"
            R"( type: "")"
            R"( expiration_time { "123ms" })"
            R"( field: "time-partition-field")"
            R"( })"
            R"( range_partitioning {)"
            R"( field: "range-partition-field")"
            R"( range { start: "" end: "" interval: "" })"
            R"( })"
            R"( clustering {)"
            R"( fields: "c-field-1")"
            R"( })"
            R"( hive_partitioning_options {)"
            R"( mode: "h-mode")"
            R"( source_uri_prefix: "")"
            R"( require_partition_filter: true)"
            R"( fields: "h-field-1")"
            R"( } )"
            R"(view {)"
            R"( use_legacy_sql: true)"
            R"( })"
            R"( labels { key: "l1" value: "v1" })"
            R"( labels { key: "l2" value: "v2" })"
            R"( creation_time { "1ms" })"
            R"( expiration_time { "1ms" })"
            R"( })");

  EXPECT_EQ(
      table.DebugString("Table", TracingOptions{}.SetOptions(
                                     "truncate_string_field_longer_than=7")),
      R"(Table {)"
      R"( kind: "t-kind")"
      R"( id: "t-id")"
      R"( friendly_name: "t-frien...<truncated>...")"
      R"( type: "t-type")"
      R"( table_reference {)"
      R"( project_id: "t-123")"
      R"( dataset_id: "t-123")"
      R"( table_id: "t-123")"
      R"( })"
      R"( time_partitioning {)"
      R"( type: "")"
      R"( expiration_time { "123ms" })"
      R"( field: "time-pa...<truncated>...")"
      R"( })"
      R"( range_partitioning {)"
      R"( field: "range-p...<truncated>...")"
      R"( range { start: "" end: "" interval: "" })"
      R"( })"
      R"( clustering {)"
      R"( fields: "c-field...<truncated>...")"
      R"( })"
      R"( hive_partitioning_options {)"
      R"( mode: "h-mode")"
      R"( source_uri_prefix: "")"
      R"( require_partition_filter: true)"
      R"( fields: "h-field...<truncated>...")"
      R"( } )"
      R"(view {)"
      R"( use_legacy_sql: true)"
      R"( })"
      R"( labels { key: "l1" value: "v1" })"
      R"( labels { key: "l2" value: "v2" })"
      R"( creation_time { "1ms" })"
      R"( expiration_time { "1ms" })"
      R"( })");

  EXPECT_EQ(table.DebugString(
                "Table", TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(Table {
  kind: "t-kind"
  id: "t-id"
  friendly_name: "t-friendlyname"
  type: "t-type"
  table_reference {
    project_id: "t-123"
    dataset_id: "t-123"
    table_id: "t-123"
  }
  time_partitioning {
    type: ""
    expiration_time {
      "123ms"
    }
    field: "time-partition-field"
  }
  range_partitioning {
    field: "range-partition-field"
    range {
      start: ""
      end: ""
      interval: ""
    }
  }
  clustering {
    fields: "c-field-1"
  }
  hive_partitioning_options {
    mode: "h-mode"
    source_uri_prefix: ""
    require_partition_filter: true
    fields: "h-field-1"
  }
  view {
    use_legacy_sql: true
  }
  labels {
    key: "l1"
    value: "v1"
  }
  labels {
    key: "l2"
    value: "v2"
  }
  creation_time {
    "1ms"
  }
  expiration_time {
    "1ms"
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
