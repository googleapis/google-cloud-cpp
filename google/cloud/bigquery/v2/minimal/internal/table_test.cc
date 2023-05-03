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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::testing::IsEmpty;
using ::testing::Not;

// Helper functions.
namespace {

ListFormatTable MakeListFormatTable() {
  std::map<std::string, std::string> labels;
  labels.insert({"l1", "v1"});
  labels.insert({"l2", "v2"});

  ListFormatTable expected;
  expected.kind = "t-kind";
  expected.id = "t-id";
  expected.friendly_name = "t-friendlyname";
  expected.type = "t-type";
  expected.labels = labels;

  expected.creation_time = std::chrono::milliseconds(1);
  expected.expiration_time = std::chrono::milliseconds(1);

  expected.table_reference.dataset_id = "t-123";
  expected.table_reference.project_id = "t-123";
  expected.table_reference.table_id = "t-123";

  expected.time_partitioning.field = "time-partition-field";
  expected.time_partitioning.expiration_time = std::chrono::milliseconds(123);
  expected.range_partitioning.field = "range-partition-field";

  std::vector<std::string> cfields;
  cfields.emplace_back("c-field-1");
  expected.clustering.fields = cfields;

  expected.hive_partitioning_options.mode = "h-mode";
  expected.hive_partitioning_options.require_partition_filter = true;

  std::vector<std::string> hfields;
  hfields.emplace_back("h-field-1");
  expected.hive_partitioning_options.fields = hfields;

  expected.view.use_legacy_sql = true;

  return expected;
}

Table MakeTable() {
  std::map<std::string, std::string> labels;
  labels.insert({"l1", "v1"});
  labels.insert({"l2", "v2"});

  Table expected;
  expected.kind = "t-kind";
  expected.etag = "t-etag";
  expected.id = "t-id";
  expected.self_link = "t-selflink";
  expected.friendly_name = "t-friendlyname";
  expected.description = "t-description";
  expected.type = "t-type";
  expected.location = "t-location";
  expected.default_collation = "t-defaultcollation";
  expected.max_staleness = "stale";
  expected.case_insensitive = true;
  expected.require_partition_filter = true;
  expected.labels = labels;
  expected.num_time_travel_physical_bytes = 1;
  expected.num_total_logical_bytes = 1;
  expected.num_active_logical_bytes = 1;
  expected.num_long_term_logical_bytes = 1;
  expected.num_total_physical_bytes = 1;
  expected.num_active_physical_bytes = 1;
  expected.num_long_term_physical_bytes = 1;
  expected.num_partitions = 1;
  expected.num_bytes = 1;
  expected.num_physical_bytes = 1;
  expected.num_long_term_bytes = 1;
  expected.num_rows = 1;

  expected.creation_time =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1));
  expected.expiration_time =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1));
  expected.last_modified_time =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(1));

  expected.table_reference.dataset_id = "t-123";
  expected.table_reference.project_id = "t-123";
  expected.table_reference.table_id = "t-123";

  TableFieldSchema f;
  f.name = "fname-1";
  f.mode = "fmode";
  f.is_measure = true;

  expected.schema.fields.emplace_back(f);

  expected.default_rounding_mode = RoundingMode::RoundHalfEven();

  expected.time_partitioning.field = "time-partition-field";
  expected.time_partitioning.expiration_time = std::chrono::milliseconds(123);
  expected.range_partitioning.field = "range-partition-field";
  std::vector<std::string> cfields;
  cfields.emplace_back("c-field-1");
  expected.clustering.fields = cfields;

  expected.clone_definition.base_table_reference.dataset_id = "t-123";
  expected.clone_definition.base_table_reference.project_id = "t-123";
  expected.clone_definition.base_table_reference.table_id = "t-123";

  expected.table_constraints.primary_key.columns.emplace_back("pcol-1");
  ForeignKey fk;
  fk.key_name = "fkey-1";
  expected.table_constraints.foreign_keys.emplace_back(fk);

  expected.view.query = "select 1;";
  expected.view.use_explicit_column_names = true;

  expected.materialized_view.query = "select 1;";
  expected.materialized_view.enable_refresh = true;
  expected.materialized_view.refresh_interval_time =
      std::chrono::milliseconds(0);

  expected.materialized_view_status.refresh_watermark =
      std::chrono::system_clock::time_point(std::chrono::milliseconds(123));

  return expected;
}

void AssertEquals(Table const& lhs, Table const& rhs) {
  EXPECT_EQ(lhs.kind, rhs.kind);
  EXPECT_EQ(lhs.etag, rhs.etag);
  EXPECT_EQ(lhs.id, rhs.id);
  EXPECT_EQ(lhs.self_link, rhs.self_link);
  EXPECT_EQ(lhs.friendly_name, rhs.friendly_name);
  EXPECT_EQ(lhs.description, rhs.description);
  EXPECT_EQ(lhs.type, rhs.type);
  EXPECT_EQ(lhs.location, rhs.location);
  EXPECT_EQ(lhs.default_collation, rhs.default_collation);
  EXPECT_EQ(lhs.max_staleness, rhs.max_staleness);
  EXPECT_EQ(lhs.require_partition_filter, rhs.require_partition_filter);
  EXPECT_EQ(lhs.require_partition_filter, rhs.require_partition_filter);

  EXPECT_EQ(lhs.creation_time, rhs.creation_time);
  EXPECT_EQ(lhs.expiration_time, rhs.expiration_time);
  EXPECT_EQ(lhs.last_modified_time, rhs.last_modified_time);

  EXPECT_EQ(lhs.num_time_travel_physical_bytes,
            rhs.num_time_travel_physical_bytes);
  EXPECT_EQ(lhs.num_total_logical_bytes, rhs.num_total_logical_bytes);
  EXPECT_EQ(lhs.num_active_logical_bytes, rhs.num_active_logical_bytes);
  EXPECT_EQ(lhs.num_long_term_logical_bytes, rhs.num_long_term_logical_bytes);
  EXPECT_EQ(lhs.num_total_physical_bytes, rhs.num_total_physical_bytes);
  EXPECT_EQ(lhs.num_active_physical_bytes, rhs.num_active_physical_bytes);
  EXPECT_EQ(lhs.num_long_term_physical_bytes, rhs.num_long_term_physical_bytes);
  EXPECT_EQ(lhs.num_partitions, rhs.num_partitions);
  EXPECT_EQ(lhs.num_bytes, rhs.num_bytes);
  EXPECT_EQ(lhs.num_physical_bytes, rhs.num_physical_bytes);
  EXPECT_EQ(lhs.num_long_term_bytes, rhs.num_long_term_bytes);
  EXPECT_EQ(lhs.num_rows, rhs.num_rows);

  EXPECT_EQ(lhs.default_rounding_mode.value, rhs.default_rounding_mode.value);

  ASSERT_THAT(lhs.schema.fields, Not(IsEmpty()));
  ASSERT_THAT(rhs.schema.fields, Not(IsEmpty()));
  EXPECT_EQ(lhs.schema.fields.size(), rhs.schema.fields.size());

  ASSERT_THAT(lhs.labels, Not(IsEmpty()));
  ASSERT_THAT(rhs.labels, Not(IsEmpty()));
  EXPECT_EQ(lhs.labels.size(), rhs.labels.size());
  EXPECT_EQ(lhs.labels.find("l1")->second, rhs.labels.find("l1")->second);
  EXPECT_EQ(lhs.labels.find("l2")->second, rhs.labels.find("l2")->second);

  EXPECT_EQ(lhs.table_reference.dataset_id, rhs.table_reference.dataset_id);
  EXPECT_EQ(lhs.table_reference.project_id, rhs.table_reference.project_id);
  EXPECT_EQ(lhs.table_reference.table_id, rhs.table_reference.table_id);

  EXPECT_EQ(lhs.time_partitioning.field, rhs.time_partitioning.field);
  EXPECT_EQ(lhs.range_partitioning.field, rhs.range_partitioning.field);

  EXPECT_EQ(lhs.time_partitioning.field, rhs.time_partitioning.field);
  EXPECT_EQ(lhs.range_partitioning.field, rhs.range_partitioning.field);

  ASSERT_THAT(lhs.clustering.fields, Not(IsEmpty()));
  ASSERT_THAT(rhs.clustering.fields, Not(IsEmpty()));
  EXPECT_EQ(lhs.clustering.fields.size(), rhs.clustering.fields.size());

  EXPECT_EQ(lhs.clone_definition.clone_time, rhs.clone_definition.clone_time);
  EXPECT_EQ(lhs.clone_definition.base_table_reference.dataset_id,
            rhs.clone_definition.base_table_reference.dataset_id);
  EXPECT_EQ(lhs.clone_definition.base_table_reference.project_id,
            rhs.clone_definition.base_table_reference.project_id);
  EXPECT_EQ(lhs.clone_definition.base_table_reference.table_id,
            rhs.clone_definition.base_table_reference.table_id);

  ASSERT_THAT(lhs.table_constraints.primary_key.columns, Not(IsEmpty()));
  ASSERT_THAT(rhs.table_constraints.primary_key.columns, Not(IsEmpty()));
  EXPECT_EQ(lhs.table_constraints.primary_key.columns[0],
            rhs.table_constraints.primary_key.columns[0]);

  ASSERT_THAT(lhs.table_constraints.foreign_keys, Not(IsEmpty()));
  ASSERT_THAT(rhs.table_constraints.foreign_keys, Not(IsEmpty()));
  EXPECT_EQ(lhs.table_constraints.foreign_keys[0].key_name,
            rhs.table_constraints.foreign_keys[0].key_name);

  EXPECT_EQ(lhs.view.query, rhs.view.query);
  EXPECT_EQ(lhs.view.use_explicit_column_names,
            rhs.view.use_explicit_column_names);

  EXPECT_EQ(lhs.materialized_view.query, rhs.materialized_view.query);
  EXPECT_EQ(lhs.materialized_view.enable_refresh,
            rhs.materialized_view.enable_refresh);

  EXPECT_EQ(lhs.materialized_view_status.refresh_watermark,
            rhs.materialized_view_status.refresh_watermark);
}

void AssertEquals(ListFormatTable const& lhs, ListFormatTable const& rhs) {
  EXPECT_EQ(lhs.kind, rhs.kind);
  EXPECT_EQ(lhs.id, rhs.id);
  EXPECT_EQ(lhs.friendly_name, rhs.friendly_name);
  EXPECT_EQ(lhs.type, rhs.type);
  EXPECT_EQ(lhs.creation_time, rhs.creation_time);
  EXPECT_EQ(lhs.expiration_time, rhs.expiration_time);

  ASSERT_THAT(lhs.labels, Not(IsEmpty()));
  ASSERT_THAT(rhs.labels, Not(IsEmpty()));
  EXPECT_EQ(lhs.labels.size(), rhs.labels.size());
  EXPECT_EQ(lhs.labels.find("l1")->second, rhs.labels.find("l1")->second);
  EXPECT_EQ(lhs.labels.find("l2")->second, rhs.labels.find("l2")->second);

  EXPECT_EQ(lhs.table_reference.dataset_id, rhs.table_reference.dataset_id);
  EXPECT_EQ(lhs.table_reference.project_id, rhs.table_reference.project_id);
  EXPECT_EQ(lhs.table_reference.table_id, rhs.table_reference.table_id);

  EXPECT_EQ(lhs.time_partitioning.field, rhs.time_partitioning.field);
  EXPECT_EQ(lhs.range_partitioning.field, rhs.range_partitioning.field);

  EXPECT_EQ(lhs.time_partitioning.field, rhs.time_partitioning.field);
  EXPECT_EQ(lhs.range_partitioning.field, rhs.range_partitioning.field);

  ASSERT_THAT(lhs.clustering.fields, Not(IsEmpty()));
  ASSERT_THAT(rhs.clustering.fields, Not(IsEmpty()));
  EXPECT_EQ(lhs.clustering.fields.size(), rhs.clustering.fields.size());

  EXPECT_EQ(lhs.view.use_legacy_sql, rhs.view.use_legacy_sql);

  EXPECT_EQ(lhs.hive_partitioning_options.mode,
            rhs.hive_partitioning_options.mode);
  EXPECT_EQ(lhs.hive_partitioning_options.require_partition_filter,
            rhs.hive_partitioning_options.require_partition_filter);
  EXPECT_EQ(lhs.hive_partitioning_options.source_uri_prefix,
            rhs.hive_partitioning_options.source_uri_prefix);

  ASSERT_THAT(lhs.hive_partitioning_options.fields, Not(IsEmpty()));
  ASSERT_THAT(rhs.hive_partitioning_options.fields, Not(IsEmpty()));
  EXPECT_EQ(lhs.hive_partitioning_options.fields.size(),
            rhs.hive_partitioning_options.fields.size());
  EXPECT_EQ(lhs.hive_partitioning_options.fields[0],
            rhs.hive_partitioning_options.fields[0]);
}

std::string MakeTableJsonText() {
  return R"({"case_insensitive":true,"clone_definition":{)"
         R"("base_table_reference":{"dataset_id":"t-123","project_id":"t-123")"
         R"(,"table_id":"t-123"})"
         R"(,"clone_time":0},"clustering":{"fields":["c-field-1"]})"
         R"(,"creation_time":1,"default_collation":"t-defaultcollation")"
         R"(,"default_rounding_mode":{"value":"ROUND_HALF_EVEN"})"
         R"(,"description":"t-description","etag":"t-etag")"
         R"(,"expiration_time":1,"friendly_name":"t-friendlyname")"
         R"(,"id":"t-id","kind":"t-kind","labels":{"l1":"v1","l2":"v2"})"
         R"(,"last_modified_time":1,"location":"t-location","materialized_view":{)"
         R"("allow_non_incremental_definition":false)"
         R"(,"enable_refresh":true,"last_refresh_time":0,"max_staleness":"")"
         R"(,"query":"select 1;","refresh_interval":0})"
         R"(,"materialized_view_status":{"last_refresh_status":{)"
         R"("location":"","message":"","reason":""},"refresh_watermark":123})"
         R"(,"max_staleness":"stale","num_active_logical_bytes":1)"
         R"(,"num_active_physical_bytes":1,"num_bytes":1)"
         R"(,"num_long_term_bytes":1,"num_long_term_logical_bytes":1)"
         R"(,"num_long_term_physical_bytes":1,"num_partitions":1)"
         R"(,"num_physical_bytes":1,"num_rows":1)"
         R"(,"num_time_travel_physical_bytes":1,"num_total_logical_bytes":1)"
         R"(,"num_total_physical_bytes":1,"range_partitioning":{)"
         R"("field":"range-partition-field","range":{"end":"")"
         R"(,"interval":"","start":""}},"require_partition_filter":true)"
         R"(,"schema":{"fields":[{"categories":{"names":[]})"
         R"(,"collation":"","data_classification_tags":{"names":[]})"
         R"(,"default_value_expression":"","description":"")"
         R"(,"fields":{"fields":[]},"is_measure":true,"max_length":0)"
         R"(,"mode":"fmode","name":"fname-1")"
         R"(,"policy_tags":{"names":[]},"precision":0,"range_element_type":{)"
         R"("type":""},"rounding_mode":{"value":""})"
         R"(,"scale":0,"type":""}]},"self_link":"t-selflink")"
         R"(,"table_constraints":{"foreign_keys":[{"column_references":[])"
         R"(,"key_name":"fkey-1","referenced_table":{"dataset_id":"")"
         R"(,"project_id":"","table_id":""}}])"
         R"(,"primary_key":{"columns":["pcol-1"]}},"table_reference":{)"
         R"("dataset_id":"t-123","project_id":"t-123")"
         R"(,"table_id":"t-123"},"time_partitioning":{)"
         R"("expiration_time":123,"field":"time-partition-field","type":""})"
         R"(,"type":"t-type","view":{"query":"select 1;")"
         R"(,"use_explicit_column_names":true)"
         R"(,"use_legacy_sql":false,"user_defined_function_resources":[]}})";
}

std::string MakeListFormatTableJsonText() {
  return R"({"clustering":{"fields":["c-field-1"]},"creation_time":1)"
         R"(,"expiration_time":1,"friendly_name":"t-friendlyname")"
         R"(,"hive_partitioning_options":{"fields":["h-field-1"],"mode":"h-mode")"
         R"(,"require_partition_filter":true,"source_uri_prefix":""})"
         R"(,"id":"t-id","kind":"t-kind","labels":{"l1":"v1","l2":"v2"})"
         R"(,"range_partitioning":{"field":"range-partition-field")"
         R"(,"range":{"end":"","interval":"","start":""}},"table_reference":{)"
         R"("dataset_id":"t-123","project_id":"t-123","table_id":"t-123"})"
         R"(,"time_partitioning":{"expiration_time":123,"field":"time-partition-field")"
         R"(,"type":""},"type":"t-type","view":{"use_legacy_sql":true}})";
}
}  // namespace

TEST(TableTest, TableToJson) {
  auto const text = MakeTableJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input = MakeTable();

  nlohmann::json actual_json;
  to_json(actual_json, input);

  EXPECT_EQ(expected_json.dump(), actual_json.dump());
}

TEST(TableTest, TableFromJson) {
  auto const text = MakeTableJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  Table expected = MakeTable();

  Table actual;
  from_json(json, actual);

  AssertEquals(expected, actual);
}

TEST(TableTest, ListFormatTableToJson) {
  auto const text = MakeListFormatTableJsonText();
  auto expected_json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto input = MakeListFormatTable();

  nlohmann::json actual_json;
  to_json(actual_json, input);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(TableTest, ListFormatTableFromJson) {
  auto const text = MakeListFormatTableJsonText();
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  ListFormatTable expected = MakeListFormatTable();

  ListFormatTable actual;
  from_json(json, actual);

  AssertEquals(expected, actual);
}

TEST(TableTest, TableDebugString) {
  Table table = MakeTable();

  EXPECT_EQ(
      table.DebugString("Table", TracingOptions{}),
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
      R"( case_insensitive: true)"
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
      R"( is_measure: true)"
      R"( categories { })"
      R"( policy_tags { })"
      R"( data_classification_tags { })"
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
      R"( view { query: "select 1;" use_legacy_sql: false use_explicit_column_names: true })"
      R"( materialized_view {)"
      R"( query: "select 1;")"
      R"( max_staleness: "")"
      R"( allow_non_incremental_definition: false)"
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
      R"( case_insensitive: true)"
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
      R"( is_measure: true)"
      R"( categories { })"
      R"( policy_tags { })"
      R"( data_classification_tags { })"
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
      R"( view { query: "select ...<truncated>..." use_legacy_sql: false use_explicit_column_names: true })"
      R"( materialized_view {)"
      R"( query: "select ...<truncated>...")"
      R"( max_staleness: "")"
      R"( allow_non_incremental_definition: false)"
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
  case_insensitive: true
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
      is_measure: true
      categories {
      }
      policy_tags {
      }
      data_classification_tags {
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
    use_explicit_column_names: true
  }
  materialized_view {
    query: "select 1;"
    max_staleness: ""
    allow_non_incremental_definition: false
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
  auto table = MakeListFormatTable();

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
