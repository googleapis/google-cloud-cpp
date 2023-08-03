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

#include "google/cloud/bigquery/v2/minimal/internal/job_configuration_query.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MakeJobConfigurationQuery;

TEST(JobConfigurationQueryTest, DebugString) {
  auto job_query_config = MakeJobConfigurationQuery();

  EXPECT_EQ(
      job_query_config.DebugString("JobConfigurationQuery", TracingOptions{}),
      R"(JobConfigurationQuery { query: "select 1;")"
      R"( create_disposition: "job-create-disposition")"
      R"( write_disposition: "job-write-disposition")"
      R"( priority: "job-priority" parameter_mode: "job-param-mode")"
      R"( preserve_nulls: true allow_large_results: true)"
      R"( use_query_cache: true flatten_results: true)"
      R"( use_legacy_sql: true create_session: true)"
      R"( maximum_bytes_billed: 0 schema_update_options: "job-update-options")"
      R"( connection_properties { key: "conn-prop-key")"
      R"( value: "conn-prop-val" } query_parameters {)"
      R"( name: "query-parameter-name" parameter_type {)"
      R"( type: "query-parameter-type" struct_types {)"
      R"( name: "qp-struct-name" description: "qp-struct-description" } })"
      R"( parameter_value { value: "query-parameter-value" } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( time_partitioning { type: "tp-field-type" expiration_time { "0" })"
      R"( field: "tp-field-1" } range_partitioning {)"
      R"( field: "rp-field-1" range { start: "range-start")"
      R"( end: "range-end" interval: "range-interval" } })"
      R"( clustering { fields: "clustering-field-1")"
      R"( fields: "clustering-field-2" })"
      R"( destination_encryption_configuration {)"
      R"( kms_key_name: "encryption-key-name" })"
      R"( script_options { statement_timeout { "10ms" })"
      R"( statement_byte_budget: 10 key_result_statement {)"
      R"( value: "FIRST_SELECT" } } system_variables {)"
      R"( types { key: "sql-struct-type-key-1" value {)"
      R"( type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value {)"
      R"( type_kind { value: "STRING" } } } types {)"
      R"( key: "sql-struct-type-key-3" value { type_kind {)"
      R"( value: "STRING" } } } values { fields {)"
      R"( key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } } } })");

  EXPECT_EQ(
      job_query_config.DebugString(
          "JobConfigurationQuery",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=10")),
      R"(JobConfigurationQuery { query: "select 1;")"
      R"( create_disposition: "job-create...<truncated>...")"
      R"( write_disposition: "job-write-...<truncated>...")"
      R"( priority: "job-priori...<truncated>...")"
      R"( parameter_mode: "job-param-...<truncated>...")"
      R"( preserve_nulls: true allow_large_results: true)"
      R"( use_query_cache: true flatten_results: true use_legacy_sql: true)"
      R"( create_session: true maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update...<truncated>...")"
      R"( connection_properties { key: "conn-prop-...<truncated>...")"
      R"( value: "conn-prop-...<truncated>..." })"
      R"( query_parameters { name: "query-para...<truncated>...")"
      R"( parameter_type { type: "query-para...<truncated>...")"
      R"( struct_types { name: "qp-struct-...<truncated>...")"
      R"( description: "qp-struct-...<truncated>..." } })"
      R"( parameter_value { value: "query-para...<truncated>..." } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } time_partitioning { type: "tp-field-t...<truncated>...")"
      R"( expiration_time { "0" } field: "tp-field-1" })"
      R"( range_partitioning { field: "rp-field-1")"
      R"( range { start: "range-star...<truncated>...")"
      R"( end: "range-end" interval: "range-inte...<truncated>..." } })"
      R"( clustering { fields: "clustering...<truncated>...")"
      R"( fields: "clustering...<truncated>..." })"
      R"( destination_encryption_configuration {)"
      R"( kms_key_name: "encryption...<truncated>..." })"
      R"( script_options { statement_timeout { "10ms" })"
      R"( statement_byte_budget: 10 key_result_statement {)"
      R"( value: "FIRST_SELE...<truncated>..." } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } })"
      R"( types { key: "sql-struct-type-key-2")"
      R"( value { type_kind { value: "STRING" } } })"
      R"( types { key: "sql-struct-type-key-3")"
      R"( value { type_kind { value: "STRING" } } })"
      R"( values { fields { key: "bool-key")"
      R"( value { value_kind: true } } fields {)"
      R"( key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } } } })");

  EXPECT_EQ(job_query_config.DebugString(
                "JobConfigurationQuery",
                TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(JobConfigurationQuery {
  query: "select 1;"
  create_disposition: "job-create-disposition"
  write_disposition: "job-write-disposition"
  priority: "job-priority"
  parameter_mode: "job-param-mode"
  preserve_nulls: true
  allow_large_results: true
  use_query_cache: true
  flatten_results: true
  use_legacy_sql: true
  create_session: true
  maximum_bytes_billed: 0
  schema_update_options: "job-update-options"
  connection_properties {
    key: "conn-prop-key"
    value: "conn-prop-val"
  }
  query_parameters {
    name: "query-parameter-name"
    parameter_type {
      type: "query-parameter-type"
      struct_types {
        name: "qp-struct-name"
        description: "qp-struct-description"
      }
    }
    parameter_value {
      value: "query-parameter-value"
    }
  }
  default_dataset {
    project_id: "2"
    dataset_id: "1"
  }
  destination_table {
    project_id: "2"
    dataset_id: "1"
    table_id: "3"
  }
  time_partitioning {
    type: "tp-field-type"
    expiration_time {
      "0"
    }
    field: "tp-field-1"
  }
  range_partitioning {
    field: "rp-field-1"
    range {
      start: "range-start"
      end: "range-end"
      interval: "range-interval"
    }
  }
  clustering {
    fields: "clustering-field-1"
    fields: "clustering-field-2"
  }
  destination_encryption_configuration {
    kms_key_name: "encryption-key-name"
  }
  script_options {
    statement_timeout {
      "10ms"
    }
    statement_byte_budget: 10
    key_result_statement {
      value: "FIRST_SELECT"
    }
  }
  system_variables {
    types {
      key: "sql-struct-type-key-1"
      value {
        type_kind {
          value: "INT64"
        }
      }
    }
    types {
      key: "sql-struct-type-key-2"
      value {
        type_kind {
          value: "STRING"
        }
      }
    }
    types {
      key: "sql-struct-type-key-3"
      value {
        type_kind {
          value: "STRING"
        }
      }
    }
    values {
      fields {
        key: "bool-key"
        value {
          value_kind: true
        }
      }
      fields {
        key: "double-key"
        value {
          value_kind: 3.4
        }
      }
      fields {
        key: "string-key"
        value {
          value_kind: "val3"
        }
      }
    }
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
