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

#include "google/cloud/bigquery/v2/minimal/internal/table_response.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(GetTableResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload = bigquery_v2_minimal_testing::MakeTableJsonText();
  auto const response = GetTableResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->http_response.payload.empty());

  auto expected = bigquery_v2_minimal_testing::MakeTable();
  bigquery_v2_minimal_testing::AssertEquals(expected, response->table);
}

TEST(GetTableResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response = GetTableResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(GetTableResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const response = GetTableResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(GetTableResponseTest, InvalidTable) {
  BigQueryHttpResponse http_response;
  http_response.payload = R"({"kind":"tkind"})";
  auto const response = GetTableResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json Table object")));
}

TEST(ListTablesResponseTest, SuccessMultiplePages) {
  BigQueryHttpResponse http_response;
  auto tables_json_txt =
      bigquery_v2_minimal_testing::MakeListFormatTableJsonText();
  http_response.payload =
      bigquery_v2_minimal_testing::MakeListTablesResponseJsonText();

  auto const list_tables_response =
      ListTablesResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_tables_response);

  auto expected = bigquery_v2_minimal_testing::MakeListFormatTable();

  EXPECT_FALSE(list_tables_response->http_response.payload.empty());
  EXPECT_EQ(list_tables_response->kind, "kind-1");
  EXPECT_EQ(list_tables_response->etag, "tag-1");
  EXPECT_EQ(list_tables_response->total_items, 1);
  EXPECT_THAT(list_tables_response->next_page_token, "npt-123");

  auto const tables = list_tables_response->tables;
  ASSERT_EQ(tables.size(), 1);

  bigquery_v2_minimal_testing::AssertEquals(expected, tables[0]);
}

TEST(ListTablesResponseTest, SuccessSinglePage) {
  BigQueryHttpResponse http_response;
  auto tables_json_txt =
      bigquery_v2_minimal_testing::MakeListFormatTableJsonText();
  http_response.payload =
      bigquery_v2_minimal_testing::MakeListTablesResponseNoPageTokenJsonText();

  auto const list_tables_response =
      ListTablesResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_tables_response);

  auto expected = bigquery_v2_minimal_testing::MakeListFormatTable();

  EXPECT_FALSE(list_tables_response->http_response.payload.empty());
  EXPECT_EQ(list_tables_response->kind, "kind-1");
  EXPECT_EQ(list_tables_response->etag, "tag-1");
  EXPECT_EQ(list_tables_response->total_items, 1);
  EXPECT_THAT(list_tables_response->next_page_token, IsEmpty());

  auto const tables = list_tables_response->tables;
  ASSERT_EQ(tables.size(), 1);

  bigquery_v2_minimal_testing::AssertEquals(expected, tables[0]);
}

TEST(ListTablesResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response =
      ListTablesResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListTablesResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Invalid";
  auto const response =
      ListTablesResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListTablesResponseTest, InvalidTableList) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "dkind",
          "etag": "dtag"})";
  auto const response =
      ListTablesResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Not a valid Json TableList object")));
}

TEST(ListTablesResponseTest, InvalidListFormatTable) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "totalItems": 1,
          "tables": [
              {
                "id": "1",
                "kind": "kind-2"
              }
  ]})";
  auto const response =
      ListTablesResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Not a valid Json ListFormatTable object")));
}

TEST(GetTableResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload = bigquery_v2_minimal_testing::MakeTableJsonText();
  auto response = GetTableResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("GetTableResponse", TracingOptions{}),
      R"(GetTableResponse { table { kind: "t-kind" etag: "t-etag" id: "t-id")"
      R"( self_link: "t-selflink" friendly_name: "t-friendlyname")"
      R"( description: "t-description")"
      R"( type: "t-type" location: "t-location")"
      R"( default_collation: "t-defaultcollation")"
      R"( max_staleness: "stale" require_partition_filter: true)"
      R"( creation_time { "1970-01-01T00:00:00.001Z" })"
      R"( expiration_time { "1970-01-01T00:00:00.001Z" })"
      R"( last_modified_time { "1970-01-01T00:00:00.001Z" })"
      R"( num_time_travel_physical_bytes: 1)"
      R"( num_total_logical_bytes: 1 num_active_logical_bytes: 1)"
      R"( num_long_term_logical_bytes: 1)"
      R"( num_total_physical_bytes: 1 num_active_physical_bytes: 1)"
      R"( num_long_term_physical_bytes: 1)"
      R"( num_partitions: 1 num_bytes: 1 num_physical_bytes: 1)"
      R"( num_long_term_bytes: 1)"
      R"( labels { key: "l1" value: "v1" } labels { key: "l2")"
      R"( value: "v2" })"
      R"( table_reference { project_id: "t-123" dataset_id: "t-123")"
      R"( table_id: "t-123" })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode")"
      R"( description: "" collation: "")"
      R"( default_value_expression: "" max_length: 0 precision: 0 scale: 0)"
      R"( categories { } policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( default_rounding_mode { value: "ROUND_HALF_EVEN" })"
      R"( time_partitioning { type: "" expiration_time { "123ms" })"
      R"( field: "time-partition-field" })"
      R"( range_partitioning { field: "range-partition-field")"
      R"( range { start: "" end: "" interval: "" } })"
      R"( clustering { fields: "c-field-1" })"
      R"( clone_definition { base_table_reference { project_id: "t-123" dataset_id: "t-123")"
      R"( table_id: "t-123" } clone_time { "1970-01-01T00:00:00Z" } })"
      R"( table_constraints { primary_key { columns: "pcol-1" })"
      R"( foreign_keys { key_name: "fkey-1")"
      R"( referenced_table { project_id: "" dataset_id: "" table_id: "" } } })"
      R"( view { query: "select 1;" use_legacy_sql: false })"
      R"( materialized_view { query: "select 1;")"
      R"( enable_refresh: true refresh_interval_time { "0" })"
      R"( last_refresh_time { "1970-01-01T00:00:00Z" } })"
      R"( materialized_view_status {)"
      R"( last_refresh_status { reason: "" location: "" message: "" })"
      R"( refresh_watermark { "1970-01-01T00:00:00.123Z" } } })"
      R"( http_response { status_code: 200)"
      R"( http_headers { key: "header1" value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString(
          "GetTableResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(GetTableResponse { table { kind: "t-kind")"
      R"( etag: "t-etag" id: "t-id")"
      R"( self_link: "t-selfl...<truncated>...")"
      R"( friendly_name: "t-frien...<truncated>...")"
      R"( description: "t-descr...<truncated>..." type: "t-type")"
      R"( location: "t-locat...<truncated>...")"
      R"( default_collation: "t-defau...<truncated>...")"
      R"( max_staleness: "stale" require_partition_filter: true)"
      R"( creation_time { "1970-01-01T00:00:00.001Z" })"
      R"( expiration_time { "1970-01-01T00:00:00.001Z" })"
      R"( last_modified_time { "1970-01-01T00:00:00.001Z" })"
      R"( num_time_travel_physical_bytes: 1 num_total_logical_bytes: 1)"
      R"( num_active_logical_bytes: 1 num_long_term_logical_bytes: 1)"
      R"( num_total_physical_bytes: 1 num_active_physical_bytes: 1)"
      R"( num_long_term_physical_bytes: 1 num_partitions: 1 num_bytes: 1)"
      R"( num_physical_bytes: 1 num_long_term_bytes: 1)"
      R"( labels { key: "l1" value: "v1" } labels { key: "l2" value: "v2" })"
      R"( table_reference { project_id: "t-123" dataset_id: "t-123" table_id: "t-123" })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode")"
      R"( description: "" collation: "" default_value_expression: "")"
      R"( max_length: 0 precision: 0 scale: 0 categories { } policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( default_rounding_mode { value: "ROUND_H...<truncated>..." })"
      R"( time_partitioning { type: "" expiration_time { "123ms" })"
      R"( field: "time-pa...<truncated>..." })"
      R"( range_partitioning { field: "range-p...<truncated>...")"
      R"( range { start: "" end: "" interval: "" } })"
      R"( clustering { fields: "c-field...<truncated>..." })"
      R"( clone_definition { base_table_reference {)"
      R"( project_id: "t-123" dataset_id: "t-123" table_id: "t-123" })"
      R"( clone_time { "1970-01-01T00:00:00Z" } })"
      R"( table_constraints { primary_key { columns: "pcol-1" })"
      R"( foreign_keys { key_name: "fkey-1")"
      R"( referenced_table { project_id: "" dataset_id: "" table_id: "" } } })"
      R"( view { query: "select ...<truncated>..." use_legacy_sql: false })"
      R"( materialized_view { query: "select ...<truncated>...")"
      R"( enable_refresh: true refresh_interval_time { "0" })"
      R"( last_refresh_time { "1970-01-01T00:00:00Z" } })"
      R"( materialized_view_status { last_refresh_status { reason: "" location: "" message: "" })"
      R"( refresh_watermark { "1970-01-01T00:00:00.123Z" } } })"
      R"( http_response { status_code: 200)"
      R"( http_headers { key: "header1" value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString("GetTableResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(GetTableResponse {
  table {
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
  }
  http_response {
    status_code: 200
    http_headers {
      key: "header1"
      value: "value1"
    }
    payload: REDACTED
  }
})");
}

TEST(ListTablesResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  auto tables_json_txt =
      bigquery_v2_minimal_testing::MakeListFormatTableJsonText();
  http_response.payload = R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "totalItems": 1,
          "tables": [)" + tables_json_txt +
                          R"(]})";

  auto response = ListTablesResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("ListTablesResponse", TracingOptions{}),
      R"(ListTablesResponse { kind: "kind-1" etag: "tag-1")"
      R"( next_page_token: "npt-123" total_items: 1 tables {)"
      R"( kind: "t-kind" id: "t-id" friendly_name: "t-friendlyname")"
      R"( type: "t-type" table_reference { project_id: "t-123")"
      R"( dataset_id: "t-123" table_id: "t-123" })"
      R"( time_partitioning { type: "" expiration_time { "123ms" })"
      R"( field: "time-partition-field" } range_partitioning {)"
      R"( field: "range-partition-field" range {)"
      R"( start: "" end: "" interval: "" } })"
      R"( clustering { fields: "c-field-1" })"
      R"( hive_partitioning_options {)"
      R"( mode: "h-mode" source_uri_prefix: "")"
      R"( require_partition_filter: true)"
      R"( fields: "h-field-1" } view { use_legacy_sql: true })"
      R"( labels { key: "l1" value: "v1" })"
      R"( labels { key: "l2" value: "v2" })"
      R"( creation_time { "1ms" })"
      R"( expiration_time { "1ms" } })"
      R"( http_response { status_code: 200)"
      R"( http_headers { key: "header1" value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString(
          "ListTablesResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(ListTablesResponse { kind: "kind-1" etag: "tag-1")"
      R"( next_page_token: "npt-123" total_items: 1 tables {)"
      R"( kind: "t-kind" id: "t-id" friendly_name: "t-frien...<truncated>...")"
      R"( type: "t-type" table_reference {)"
      R"( project_id: "t-123" dataset_id: "t-123" table_id: "t-123" })"
      R"( time_partitioning { type: "" expiration_time { "123ms" })"
      R"( field: "time-pa...<truncated>..." })"
      R"( range_partitioning { field: "range-p...<truncated>...")"
      R"( range { start: "" end: "" interval: "" } })"
      R"( clustering { fields: "c-field...<truncated>..." })"
      R"( hive_partitioning_options { mode: "h-mode" source_uri_prefix: "")"
      R"( require_partition_filter: true fields: "h-field...<truncated>..." })"
      R"( view { use_legacy_sql: true } labels { key: "l1" value: "v1" })"
      R"( labels { key: "l2" value: "v2" } creation_time { "1ms" })"
      R"( expiration_time { "1ms" } } http_response {)"
      R"( status_code: 200 http_headers { key: "header1" value: "value1" })"
      R"( payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString("ListTablesResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListTablesResponse {
  kind: "kind-1"
  etag: "tag-1"
  next_page_token: "npt-123"
  total_items: 1
  tables {
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
  }
  http_response {
    status_code: 200
    http_headers {
      key: "header1"
      value: "value1"
    }
    payload: REDACTED
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
