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

#include "google/cloud/bigquery/v2/minimal/internal/job.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MakeJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakeListFormatJob;

TEST(JobTest, JobDebugString) {
  auto job = bigquery_v2_minimal_testing::MakeJob();

  EXPECT_EQ(
      job.DebugString("Job", TracingOptions{}),
      R"(Job { etag: "etag" kind: "Job" self_link: "self-link" id: "1")"
      R"( configuration { job_type: "QUERY" dry_run: true job_timeout { "10ms" })"
      R"( labels { key: "label-key1" value: "label-val1" })"
      R"( query_config { query: "select 1;")"
      R"( create_disposition: "job-create-disposition")"
      R"( write_disposition: "job-write-disposition")"
      R"( priority: "job-priority" parameter_mode: "job-param-mode")"
      R"( preserve_nulls: true allow_large_results: true use_query_cache: true)"
      R"( flatten_results: true use_legacy_sql: true create_session: true)"
      R"( maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update-options" connection_properties {)"
      R"( key: "conn-prop-key" value: "conn-prop-val" } query_parameters {)"
      R"( name: "query-parameter-name" parameter_type {)"
      R"( type: "query-parameter-type" struct_types {)"
      R"( name: "qp-struct-name" description: "qp-struct-description" } })"
      R"( parameter_value { value: "query-parameter-value" } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( time_partitioning { type: "tp-field-type" expiration_time { "0" })"
      R"( field: "tp-field-1" } range_partitioning {)"
      R"( field: "rp-field-1" range { start: "range-start" end: "range-end")"
      R"( interval: "range-interval" } } clustering { fields: "clustering-field-1")"
      R"( fields: "clustering-field-2" } destination_encryption_configuration {)"
      R"( kms_key_name: "encryption-key-name" } script_options {)"
      R"( statement_timeout { "10ms" } statement_byte_budget: 10)"
      R"( key_result_statement { value: "FIRST_SELECT" } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value { type_kind {)"
      R"( value: "STRING" } } } types { key: "sql-struct-type-key-3")"
      R"( value { type_kind { value: "STRING" } } } values {)"
      R"( fields { key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } } } } })"
      R"( reference { project_id: "1" job_id: "2" location: "us-east" })"
      R"( status { state: "DONE" error_result { reason: "" location: "")"
      R"( message: "" } } statistics { creation_time { "10ms" })"
      R"( start_time { "10ms" } end_time { "10ms" } total_slot_time {)"
      R"( "10ms" } final_execution_duration { "10ms" } total_bytes_processed: 1234)"
      R"( num_child_jobs: 1234)"
      R"( row_level_security_applied: true data_masking_applied: true)"
      R"( completion_ratio: 1234.12 quota_deferments: "quota-defer-1")"
      R"( parent_job_id: "parent-job-123" session_id: "session-id-123")"
      R"( transaction_id: "transaction-id-123" reservation_id: "reservation-id-123")"
      R"( script_statistics { stack_frames { start_line: 1234 start_column: 1234)"
      R"( end_line: 1234 end_column: 1234 procedure_id: "proc-id")"
      R"( text: "stack-frame-text" } evaluation_kind { value: "STATEMENT" } })"
      R"( job_query_stats { estimated_bytes_processed: 1234)"
      R"( total_partitions_processed: 1234 total_bytes_processed: 1234)"
      R"( total_bytes_billed: 1234 billing_tier: 1234 num_dml_affected_rows: 1234)"
      R"( ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_bytes_processed_accuracy")"
      R"( statement_type: "statement_type")"
      R"( ddl_operation_performed: "ddl_operation_performed" total_slot_time { "10ms" })"
      R"( cache_hit: true query_plan { name: "test-explain")"
      R"( status: "explain-status" id: 1234 shuffle_output_bytes: 1234)"
      R"( shuffle_output_bytes_spilled: 1234 records_read: 1234)"
      R"( records_written: 1234 parallel_inputs: 1234)"
      R"( completed_parallel_inputs: 1234 start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" } wait_avg_time_spent { "10ms" })"
      R"( wait_max_time_spent { "10ms" } read_avg_time_spent { "10ms" })"
      R"( read_max_time_spent { "10ms" } write_avg_time_spent { "10ms" })"
      R"( write_max_time_spent { "10ms" } compute_avg_time_spent { "10ms" })"
      R"( compute_max_time_spent { "10ms" } wait_ratio_avg: 1234.12)"
      R"( wait_ratio_max: 1234.12 read_ratio_avg: 1234.12 read_ratio_max: 1234.12)"
      R"( compute_ratio_avg: 1234.12 compute_ratio_max: 1234.12)"
      R"( write_ratio_avg: 1234.12 write_ratio_max: 1234.12 steps {)"
      R"( kind: "sub-step-kind" substeps: "sub-step-1" } compute_mode {)"
      R"( value: "BIGQUERY" } } timeline { elapsed_time { "10ms" })"
      R"( total_slot_time { "10ms" } pending_units: 1234)"
      R"( completed_units: 1234 active_units: 1234 estimated_runnable_units: 1234 })"
      R"( referenced_tables { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( referenced_routines { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode" description: "")"
      R"( collation: "" default_value_expression: "" max_length: 0 precision: 0)"
      R"( scale: 0 categories { } policy_tags { })"
      R"( rounding_mode { value: "" })"
      R"( range_element_type { type: "" } } } dml_stats {)"
      R"( inserted_row_count: 1234 deleted_row_count: 1234 updated_row_count: 1234 })"
      R"( ddl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( ddl_target_row_access_policy { project_id: "1234" dataset_id: "1")"
      R"( table_id: "2" policy_id: "3" } ddl_target_routine {)"
      R"( project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( dcl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_view { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( search_statistics { index_unused_reasons { message: "")"
      R"( index_name: "test-index" base_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } code { value: "BASE_TABLE_TOO_SMALL" } })"
      R"( index_usage_mode { value: "PARTIALLY_USED" } })"
      R"( performance_insights { avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234)"
      R"( slot_contention: true insufficient_shuffle_quota: true })"
      R"( stage_performance_change_insights { stage_id: 1234 input_data_change {)"
      R"( records_read_diff_percentage: 12.12 } } } materialized_view_statistics {)"
      R"( materialized_view { chosen: true estimated_bytes_saved: 1234 rejected_reason {)"
      R"( value: "BASE_TABLE_DATA_CHANGE" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table-metadata" unused_reason {)"
      R"( value: "EXCEEDED_MAX_STALENESS" })"
      R"( table_reference { project_id: "2" dataset_id: "1" table_id: "3" } } } } } })");

  EXPECT_EQ(
      job.DebugString("Job", TracingOptions{}.SetOptions(
                                 "truncate_string_field_longer_than=1024")),
      R"(Job { etag: "etag" kind: "Job" self_link: "self-link" id: "1")"
      R"( configuration { job_type: "QUERY" dry_run: true job_timeout { "10ms" })"
      R"( labels { key: "label-key1" value: "label-val1" })"
      R"( query_config { query: "select 1;")"
      R"( create_disposition: "job-create-disposition")"
      R"( write_disposition: "job-write-disposition")"
      R"( priority: "job-priority" parameter_mode: "job-param-mode")"
      R"( preserve_nulls: true allow_large_results: true use_query_cache: true)"
      R"( flatten_results: true use_legacy_sql: true create_session: true)"
      R"( maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update-options" connection_properties {)"
      R"( key: "conn-prop-key" value: "conn-prop-val" } query_parameters {)"
      R"( name: "query-parameter-name" parameter_type {)"
      R"( type: "query-parameter-type" struct_types {)"
      R"( name: "qp-struct-name" description: "qp-struct-description" } })"
      R"( parameter_value { value: "query-parameter-value" } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( time_partitioning { type: "tp-field-type" expiration_time { "0" })"
      R"( field: "tp-field-1" } range_partitioning {)"
      R"( field: "rp-field-1" range { start: "range-start" end: "range-end")"
      R"( interval: "range-interval" } } clustering { fields: "clustering-field-1")"
      R"( fields: "clustering-field-2" } destination_encryption_configuration {)"
      R"( kms_key_name: "encryption-key-name" } script_options {)"
      R"( statement_timeout { "10ms" } statement_byte_budget: 10)"
      R"( key_result_statement { value: "FIRST_SELECT" } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value { type_kind {)"
      R"( value: "STRING" } } } types { key: "sql-struct-type-key-3")"
      R"( value { type_kind { value: "STRING" } } } values {)"
      R"( fields { key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } } } } })"
      R"( reference { project_id: "1" job_id: "2" location: "us-east" })"
      R"( status { state: "DONE" error_result { reason: "" location: "")"
      R"( message: "" } } statistics { creation_time { "10ms" })"
      R"( start_time { "10ms" } end_time { "10ms" } total_slot_time {)"
      R"( "10ms" } final_execution_duration { "10ms" } total_bytes_processed: 1234)"
      R"( num_child_jobs: 1234)"
      R"( row_level_security_applied: true data_masking_applied: true)"
      R"( completion_ratio: 1234.12 quota_deferments: "quota-defer-1")"
      R"( parent_job_id: "parent-job-123" session_id: "session-id-123")"
      R"( transaction_id: "transaction-id-123" reservation_id: "reservation-id-123")"
      R"( script_statistics { stack_frames { start_line: 1234 start_column: 1234)"
      R"( end_line: 1234 end_column: 1234 procedure_id: "proc-id")"
      R"( text: "stack-frame-text" } evaluation_kind { value: "STATEMENT" } })"
      R"( job_query_stats { estimated_bytes_processed: 1234)"
      R"( total_partitions_processed: 1234 total_bytes_processed: 1234)"
      R"( total_bytes_billed: 1234 billing_tier: 1234 num_dml_affected_rows: 1234)"
      R"( ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_bytes_processed_accuracy")"
      R"( statement_type: "statement_type")"
      R"( ddl_operation_performed: "ddl_operation_performed" total_slot_time { "10ms" })"
      R"( cache_hit: true query_plan { name: "test-explain")"
      R"( status: "explain-status" id: 1234 shuffle_output_bytes: 1234)"
      R"( shuffle_output_bytes_spilled: 1234 records_read: 1234)"
      R"( records_written: 1234 parallel_inputs: 1234)"
      R"( completed_parallel_inputs: 1234 start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" } wait_avg_time_spent { "10ms" })"
      R"( wait_max_time_spent { "10ms" } read_avg_time_spent { "10ms" })"
      R"( read_max_time_spent { "10ms" } write_avg_time_spent { "10ms" })"
      R"( write_max_time_spent { "10ms" } compute_avg_time_spent { "10ms" })"
      R"( compute_max_time_spent { "10ms" } wait_ratio_avg: 1234.12)"
      R"( wait_ratio_max: 1234.12 read_ratio_avg: 1234.12 read_ratio_max: 1234.12)"
      R"( compute_ratio_avg: 1234.12 compute_ratio_max: 1234.12)"
      R"( write_ratio_avg: 1234.12 write_ratio_max: 1234.12 steps {)"
      R"( kind: "sub-step-kind" substeps: "sub-step-1" } compute_mode {)"
      R"( value: "BIGQUERY" } } timeline { elapsed_time { "10ms" })"
      R"( total_slot_time { "10ms" } pending_units: 1234)"
      R"( completed_units: 1234 active_units: 1234 estimated_runnable_units: 1234 })"
      R"( referenced_tables { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( referenced_routines { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode" description: "")"
      R"( collation: "" default_value_expression: "" max_length: 0 precision: 0)"
      R"( scale: 0 categories { } policy_tags { })"
      R"( rounding_mode { value: "" })"
      R"( range_element_type { type: "" } } } dml_stats {)"
      R"( inserted_row_count: 1234 deleted_row_count: 1234 updated_row_count: 1234 })"
      R"( ddl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( ddl_target_row_access_policy { project_id: "1234" dataset_id: "1")"
      R"( table_id: "2" policy_id: "3" } ddl_target_routine {)"
      R"( project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( dcl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_view { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( search_statistics { index_unused_reasons { message: "")"
      R"( index_name: "test-index" base_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } code { value: "BASE_TABLE_TOO_SMALL" } })"
      R"( index_usage_mode { value: "PARTIALLY_USED" } })"
      R"( performance_insights { avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234)"
      R"( slot_contention: true insufficient_shuffle_quota: true })"
      R"( stage_performance_change_insights { stage_id: 1234 input_data_change {)"
      R"( records_read_diff_percentage: 12.12 } } } materialized_view_statistics {)"
      R"( materialized_view { chosen: true estimated_bytes_saved: 1234 rejected_reason {)"
      R"( value: "BASE_TABLE_DATA_CHANGE" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table-metadata" unused_reason {)"
      R"( value: "EXCEEDED_MAX_STALENESS" })"
      R"( table_reference { project_id: "2" dataset_id: "1" table_id: "3" } } } } } })");

  EXPECT_EQ(
      job.DebugString("Job", TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(Job {
  etag: "etag"
  kind: "Job"
  self_link: "self-link"
  id: "1"
  configuration {
    job_type: "QUERY"
    dry_run: true
    job_timeout {
      "10ms"
    }
    labels {
      key: "label-key1"
      value: "label-val1"
    }
    query_config {
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
    }
  }
  reference {
    project_id: "1"
    job_id: "2"
    location: "us-east"
  }
  status {
    state: "DONE"
    error_result {
      reason: ""
      location: ""
      message: ""
    }
  }
  statistics {
    creation_time {
      "10ms"
    }
    start_time {
      "10ms"
    }
    end_time {
      "10ms"
    }
    total_slot_time {
      "10ms"
    }
    final_execution_duration {
      "10ms"
    }
    total_bytes_processed: 1234
    num_child_jobs: 1234
    row_level_security_applied: true
    data_masking_applied: true
    completion_ratio: 1234.12
    quota_deferments: "quota-defer-1"
    parent_job_id: "parent-job-123"
    session_id: "session-id-123"
    transaction_id: "transaction-id-123"
    reservation_id: "reservation-id-123"
    script_statistics {
      stack_frames {
        start_line: 1234
        start_column: 1234
        end_line: 1234
        end_column: 1234
        procedure_id: "proc-id"
        text: "stack-frame-text"
      }
      evaluation_kind {
        value: "STATEMENT"
      }
    }
    job_query_stats {
      estimated_bytes_processed: 1234
      total_partitions_processed: 1234
      total_bytes_processed: 1234
      total_bytes_billed: 1234
      billing_tier: 1234
      num_dml_affected_rows: 1234
      ddl_affected_row_access_policy_count: 1234
      total_bytes_processed_accuracy: "total_bytes_processed_accuracy"
      statement_type: "statement_type"
      ddl_operation_performed: "ddl_operation_performed"
      total_slot_time {
        "10ms"
      }
      cache_hit: true
      query_plan {
        name: "test-explain"
        status: "explain-status"
        id: 1234
        shuffle_output_bytes: 1234
        shuffle_output_bytes_spilled: 1234
        records_read: 1234
        records_written: 1234
        parallel_inputs: 1234
        completed_parallel_inputs: 1234
        start_time {
          "10ms"
        }
        end_time {
          "10ms"
        }
        slot_time {
          "10ms"
        }
        wait_avg_time_spent {
          "10ms"
        }
        wait_max_time_spent {
          "10ms"
        }
        read_avg_time_spent {
          "10ms"
        }
        read_max_time_spent {
          "10ms"
        }
        write_avg_time_spent {
          "10ms"
        }
        write_max_time_spent {
          "10ms"
        }
        compute_avg_time_spent {
          "10ms"
        }
        compute_max_time_spent {
          "10ms"
        }
        wait_ratio_avg: 1234.12
        wait_ratio_max: 1234.12
        read_ratio_avg: 1234.12
        read_ratio_max: 1234.12
        compute_ratio_avg: 1234.12
        compute_ratio_max: 1234.12
        write_ratio_avg: 1234.12
        write_ratio_max: 1234.12
        steps {
          kind: "sub-step-kind"
          substeps: "sub-step-1"
        }
        compute_mode {
          value: "BIGQUERY"
        }
      }
      timeline {
        elapsed_time {
          "10ms"
        }
        total_slot_time {
          "10ms"
        }
        pending_units: 1234
        completed_units: 1234
        active_units: 1234
        estimated_runnable_units: 1234
      }
      referenced_tables {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      referenced_routines {
        project_id: "2"
        dataset_id: "1"
        routine_id: "3"
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
      dml_stats {
        inserted_row_count: 1234
        deleted_row_count: 1234
        updated_row_count: 1234
      }
      ddl_target_table {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      ddl_target_row_access_policy {
        project_id: "1234"
        dataset_id: "1"
        table_id: "2"
        policy_id: "3"
      }
      ddl_target_routine {
        project_id: "2"
        dataset_id: "1"
        routine_id: "3"
      }
      ddl_target_dataset {
        project_id: "2"
        dataset_id: "1"
      }
      dcl_target_table {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      dcl_target_view {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      dcl_target_dataset {
        project_id: "2"
        dataset_id: "1"
      }
      search_statistics {
        index_unused_reasons {
          message: ""
          index_name: "test-index"
          base_table {
            project_id: "2"
            dataset_id: "1"
            table_id: "3"
          }
          code {
            value: "BASE_TABLE_TOO_SMALL"
          }
        }
        index_usage_mode {
          value: "PARTIALLY_USED"
        }
      }
      performance_insights {
        avg_previous_execution_time {
          "10ms"
        }
        stage_performance_standalone_insights {
          stage_id: 1234
          slot_contention: true
          insufficient_shuffle_quota: true
        }
        stage_performance_change_insights {
          stage_id: 1234
          input_data_change {
            records_read_diff_percentage: 12.12
          }
        }
      }
      materialized_view_statistics {
        materialized_view {
          chosen: true
          estimated_bytes_saved: 1234
          rejected_reason {
            value: "BASE_TABLE_DATA_CHANGE"
          }
          table_reference {
            project_id: "2"
            dataset_id: "1"
            table_id: "3"
          }
        }
      }
      metadata_cache_statistics {
        table_metadata_cache_usage {
          explanation: "test-table-metadata"
          unused_reason {
            value: "EXCEEDED_MAX_STALENESS"
          }
          table_reference {
            project_id: "2"
            dataset_id: "1"
            table_id: "3"
          }
        }
      }
    }
  }
})");
}

TEST(JobTest, ListFormatJobDebugString) {
  auto job = bigquery_v2_minimal_testing::MakeListFormatJob();

  EXPECT_EQ(
      job.DebugString("ListFormatJob", TracingOptions{}),
      R"(ListFormatJob { id: "1" kind: "Job" state: "DONE")"
      R"( configuration { job_type: "QUERY" dry_run: true)"
      R"( job_timeout { "10ms" } labels { key: "label-key1")"
      R"( value: "label-val1" } query_config { query: "select 1;")"
      R"( create_disposition: "job-create-disposition")"
      R"( write_disposition: "job-write-disposition")"
      R"( priority: "job-priority" parameter_mode: "job-param-mode")"
      R"( preserve_nulls: true allow_large_results: true)"
      R"( use_query_cache: true flatten_results: true)"
      R"( use_legacy_sql: true create_session: true)"
      R"( maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update-options")"
      R"( connection_properties { key: "conn-prop-key")"
      R"( value: "conn-prop-val" } query_parameters {)"
      R"( name: "query-parameter-name" parameter_type {)"
      R"( type: "query-parameter-type" struct_types {)"
      R"( name: "qp-struct-name" description: "qp-struct-description" } })"
      R"( parameter_value { value: "query-parameter-value" } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } time_partitioning { type: "tp-field-type")"
      R"( expiration_time { "0" } field: "tp-field-1" })"
      R"( range_partitioning { field: "rp-field-1" range {)"
      R"( start: "range-start" end: "range-end" interval: "range-interval" } })"
      R"( clustering { fields: "clustering-field-1")"
      R"( fields: "clustering-field-2" } destination_encryption_configuration {)"
      R"( kms_key_name: "encryption-key-name" } script_options {)"
      R"( statement_timeout { "10ms" } statement_byte_budget: 10)"
      R"( key_result_statement { value: "FIRST_SELECT" } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value { type_kind {)"
      R"( value: "STRING" } } } types { key: "sql-struct-type-key-3")"
      R"( value { type_kind { value: "STRING" } } } values {)"
      R"( fields { key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } } } } })"
      R"( reference { project_id: "1" job_id: "2" location: "us-east" })"
      R"( status { state: "DONE" error_result { reason: "" location: "")"
      R"( message: "" } } statistics { creation_time { "10ms" })"
      R"( start_time { "10ms" } end_time { "10ms" } total_slot_time { "10ms" })"
      R"( final_execution_duration { "10ms" } total_bytes_processed: 1234)"
      R"( num_child_jobs: 1234)"
      R"( row_level_security_applied: true data_masking_applied: true)"
      R"( completion_ratio: 1234.12 quota_deferments: "quota-defer-1")"
      R"( parent_job_id: "parent-job-123" session_id: "session-id-123")"
      R"( transaction_id: "transaction-id-123" reservation_id: "reservation-id-123")"
      R"( script_statistics { stack_frames { start_line: 1234)"
      R"( start_column: 1234 end_line: 1234 end_column: 1234)"
      R"( procedure_id: "proc-id" text: "stack-frame-text" } evaluation_kind {)"
      R"( value: "STATEMENT" } } job_query_stats { estimated_bytes_processed: 1234)"
      R"( total_partitions_processed: 1234 total_bytes_processed: 1234)"
      R"( total_bytes_billed: 1234 billing_tier: 1234 num_dml_affected_rows: 1234)"
      R"( ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_bytes_processed_accuracy")"
      R"( statement_type: "statement_type")"
      R"( ddl_operation_performed: "ddl_operation_performed")"
      R"( total_slot_time { "10ms" } cache_hit: true query_plan {)"
      R"( name: "test-explain" status: "explain-status" id: 1234)"
      R"( shuffle_output_bytes: 1234 shuffle_output_bytes_spilled: 1234)"
      R"( records_read: 1234 records_written: 1234 parallel_inputs: 1234)"
      R"( completed_parallel_inputs: 1234 start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" })"
      R"( wait_avg_time_spent { "10ms" } wait_max_time_spent { "10ms" })"
      R"( read_avg_time_spent { "10ms" } read_max_time_spent { "10ms" })"
      R"( write_avg_time_spent { "10ms" } write_max_time_spent { "10ms" })"
      R"( compute_avg_time_spent { "10ms" } compute_max_time_spent { "10ms" })"
      R"( wait_ratio_avg: 1234.12 wait_ratio_max: 1234.12 read_ratio_avg: 1234.12)"
      R"( read_ratio_max: 1234.12 compute_ratio_avg: 1234.12)"
      R"( compute_ratio_max: 1234.12 write_ratio_avg: 1234.12)"
      R"( write_ratio_max: 1234.12 steps { kind: "sub-step-kind")"
      R"( substeps: "sub-step-1" } compute_mode { value: "BIGQUERY" } })"
      R"( timeline { elapsed_time { "10ms" } total_slot_time { "10ms" })"
      R"( pending_units: 1234 completed_units: 1234 active_units: 1234)"
      R"( estimated_runnable_units: 1234 } referenced_tables { project_id: "2")"
      R"( dataset_id: "1" table_id: "3" } referenced_routines { project_id: "2")"
      R"( dataset_id: "1" routine_id: "3" } schema { fields { name: "fname-1")"
      R"( type: "" mode: "fmode" description: "" collation: "")"
      R"( default_value_expression: "" max_length: 0 precision: 0 scale: 0)"
      R"( categories { } policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( dml_stats { inserted_row_count: 1234 deleted_row_count: 1234)"
      R"( updated_row_count: 1234 } ddl_target_table { project_id: "2")"
      R"( dataset_id: "1" table_id: "3" } ddl_target_row_access_policy {)"
      R"( project_id: "1234" dataset_id: "1" table_id: "2" policy_id: "3" })"
      R"( ddl_target_routine { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( dcl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_view { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( search_statistics { index_unused_reasons {)"
      R"( message: "" index_name: "test-index" base_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } code {)"
      R"( value: "BASE_TABLE_TOO_SMALL" } } index_usage_mode {)"
      R"( value: "PARTIALLY_USED" } } performance_insights {)"
      R"( avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234)"
      R"( slot_contention: true insufficient_shuffle_quota: true })"
      R"( stage_performance_change_insights { stage_id: 1234 input_data_change {)"
      R"( records_read_diff_percentage: 12.12 } } } materialized_view_statistics {)"
      R"( materialized_view { chosen: true estimated_bytes_saved: 1234)"
      R"( rejected_reason { value: "BASE_TABLE_DATA_CHANGE" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table-metadata" unused_reason {)"
      R"( value: "EXCEEDED_MAX_STALENESS" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } } } })"
      R"( error_result { reason: "" location: "" message: "" } })");

  EXPECT_EQ(
      job.DebugString("ListFormatJob",
                      TracingOptions{}.SetOptions(
                          "truncate_string_field_longer_than=1024")),
      R"(ListFormatJob { id: "1" kind: "Job" state: "DONE")"
      R"( configuration { job_type: "QUERY" dry_run: true)"
      R"( job_timeout { "10ms" } labels { key: "label-key1")"
      R"( value: "label-val1" } query_config { query: "select 1;")"
      R"( create_disposition: "job-create-disposition")"
      R"( write_disposition: "job-write-disposition")"
      R"( priority: "job-priority" parameter_mode: "job-param-mode")"
      R"( preserve_nulls: true allow_large_results: true)"
      R"( use_query_cache: true flatten_results: true)"
      R"( use_legacy_sql: true create_session: true)"
      R"( maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update-options")"
      R"( connection_properties { key: "conn-prop-key")"
      R"( value: "conn-prop-val" } query_parameters {)"
      R"( name: "query-parameter-name" parameter_type {)"
      R"( type: "query-parameter-type" struct_types {)"
      R"( name: "qp-struct-name" description: "qp-struct-description" } })"
      R"( parameter_value { value: "query-parameter-value" } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } time_partitioning { type: "tp-field-type")"
      R"( expiration_time { "0" } field: "tp-field-1" })"
      R"( range_partitioning { field: "rp-field-1" range {)"
      R"( start: "range-start" end: "range-end" interval: "range-interval" } })"
      R"( clustering { fields: "clustering-field-1")"
      R"( fields: "clustering-field-2" } destination_encryption_configuration {)"
      R"( kms_key_name: "encryption-key-name" } script_options {)"
      R"( statement_timeout { "10ms" } statement_byte_budget: 10)"
      R"( key_result_statement { value: "FIRST_SELECT" } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value { type_kind {)"
      R"( value: "STRING" } } } types { key: "sql-struct-type-key-3")"
      R"( value { type_kind { value: "STRING" } } } values {)"
      R"( fields { key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } } } } })"
      R"( reference { project_id: "1" job_id: "2" location: "us-east" })"
      R"( status { state: "DONE" error_result { reason: "" location: "")"
      R"( message: "" } } statistics { creation_time { "10ms" })"
      R"( start_time { "10ms" } end_time { "10ms" } total_slot_time { "10ms" })"
      R"( final_execution_duration { "10ms" } total_bytes_processed: 1234)"
      R"( num_child_jobs: 1234)"
      R"( row_level_security_applied: true data_masking_applied: true)"
      R"( completion_ratio: 1234.12 quota_deferments: "quota-defer-1")"
      R"( parent_job_id: "parent-job-123" session_id: "session-id-123")"
      R"( transaction_id: "transaction-id-123" reservation_id: "reservation-id-123")"
      R"( script_statistics { stack_frames { start_line: 1234)"
      R"( start_column: 1234 end_line: 1234 end_column: 1234)"
      R"( procedure_id: "proc-id" text: "stack-frame-text" } evaluation_kind {)"
      R"( value: "STATEMENT" } } job_query_stats { estimated_bytes_processed: 1234)"
      R"( total_partitions_processed: 1234 total_bytes_processed: 1234)"
      R"( total_bytes_billed: 1234 billing_tier: 1234 num_dml_affected_rows: 1234)"
      R"( ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_bytes_processed_accuracy")"
      R"( statement_type: "statement_type")"
      R"( ddl_operation_performed: "ddl_operation_performed")"
      R"( total_slot_time { "10ms" } cache_hit: true query_plan {)"
      R"( name: "test-explain" status: "explain-status" id: 1234)"
      R"( shuffle_output_bytes: 1234 shuffle_output_bytes_spilled: 1234)"
      R"( records_read: 1234 records_written: 1234 parallel_inputs: 1234)"
      R"( completed_parallel_inputs: 1234 start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" })"
      R"( wait_avg_time_spent { "10ms" } wait_max_time_spent { "10ms" })"
      R"( read_avg_time_spent { "10ms" } read_max_time_spent { "10ms" })"
      R"( write_avg_time_spent { "10ms" } write_max_time_spent { "10ms" })"
      R"( compute_avg_time_spent { "10ms" } compute_max_time_spent { "10ms" })"
      R"( wait_ratio_avg: 1234.12 wait_ratio_max: 1234.12 read_ratio_avg: 1234.12)"
      R"( read_ratio_max: 1234.12 compute_ratio_avg: 1234.12)"
      R"( compute_ratio_max: 1234.12 write_ratio_avg: 1234.12)"
      R"( write_ratio_max: 1234.12 steps { kind: "sub-step-kind")"
      R"( substeps: "sub-step-1" } compute_mode { value: "BIGQUERY" } })"
      R"( timeline { elapsed_time { "10ms" } total_slot_time { "10ms" })"
      R"( pending_units: 1234 completed_units: 1234 active_units: 1234)"
      R"( estimated_runnable_units: 1234 } referenced_tables { project_id: "2")"
      R"( dataset_id: "1" table_id: "3" } referenced_routines { project_id: "2")"
      R"( dataset_id: "1" routine_id: "3" } schema { fields { name: "fname-1")"
      R"( type: "" mode: "fmode" description: "" collation: "")"
      R"( default_value_expression: "" max_length: 0 precision: 0 scale: 0)"
      R"( categories { } policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( dml_stats { inserted_row_count: 1234 deleted_row_count: 1234)"
      R"( updated_row_count: 1234 } ddl_target_table { project_id: "2")"
      R"( dataset_id: "1" table_id: "3" } ddl_target_row_access_policy {)"
      R"( project_id: "1234" dataset_id: "1" table_id: "2" policy_id: "3" })"
      R"( ddl_target_routine { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( dcl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_view { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( search_statistics { index_unused_reasons {)"
      R"( message: "" index_name: "test-index" base_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } code {)"
      R"( value: "BASE_TABLE_TOO_SMALL" } } index_usage_mode {)"
      R"( value: "PARTIALLY_USED" } } performance_insights {)"
      R"( avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234)"
      R"( slot_contention: true insufficient_shuffle_quota: true })"
      R"( stage_performance_change_insights { stage_id: 1234 input_data_change {)"
      R"( records_read_diff_percentage: 12.12 } } } materialized_view_statistics {)"
      R"( materialized_view { chosen: true estimated_bytes_saved: 1234)"
      R"( rejected_reason { value: "BASE_TABLE_DATA_CHANGE" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table-metadata" unused_reason {)"
      R"( value: "EXCEEDED_MAX_STALENESS" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } } } })"
      R"( error_result { reason: "" location: "" message: "" } })");

  EXPECT_EQ(job.DebugString("ListFormatJob",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
            R"(ListFormatJob {
  id: "1"
  kind: "Job"
  state: "DONE"
  configuration {
    job_type: "QUERY"
    dry_run: true
    job_timeout {
      "10ms"
    }
    labels {
      key: "label-key1"
      value: "label-val1"
    }
    query_config {
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
    }
  }
  reference {
    project_id: "1"
    job_id: "2"
    location: "us-east"
  }
  status {
    state: "DONE"
    error_result {
      reason: ""
      location: ""
      message: ""
    }
  }
  statistics {
    creation_time {
      "10ms"
    }
    start_time {
      "10ms"
    }
    end_time {
      "10ms"
    }
    total_slot_time {
      "10ms"
    }
    final_execution_duration {
      "10ms"
    }
    total_bytes_processed: 1234
    num_child_jobs: 1234
    row_level_security_applied: true
    data_masking_applied: true
    completion_ratio: 1234.12
    quota_deferments: "quota-defer-1"
    parent_job_id: "parent-job-123"
    session_id: "session-id-123"
    transaction_id: "transaction-id-123"
    reservation_id: "reservation-id-123"
    script_statistics {
      stack_frames {
        start_line: 1234
        start_column: 1234
        end_line: 1234
        end_column: 1234
        procedure_id: "proc-id"
        text: "stack-frame-text"
      }
      evaluation_kind {
        value: "STATEMENT"
      }
    }
    job_query_stats {
      estimated_bytes_processed: 1234
      total_partitions_processed: 1234
      total_bytes_processed: 1234
      total_bytes_billed: 1234
      billing_tier: 1234
      num_dml_affected_rows: 1234
      ddl_affected_row_access_policy_count: 1234
      total_bytes_processed_accuracy: "total_bytes_processed_accuracy"
      statement_type: "statement_type"
      ddl_operation_performed: "ddl_operation_performed"
      total_slot_time {
        "10ms"
      }
      cache_hit: true
      query_plan {
        name: "test-explain"
        status: "explain-status"
        id: 1234
        shuffle_output_bytes: 1234
        shuffle_output_bytes_spilled: 1234
        records_read: 1234
        records_written: 1234
        parallel_inputs: 1234
        completed_parallel_inputs: 1234
        start_time {
          "10ms"
        }
        end_time {
          "10ms"
        }
        slot_time {
          "10ms"
        }
        wait_avg_time_spent {
          "10ms"
        }
        wait_max_time_spent {
          "10ms"
        }
        read_avg_time_spent {
          "10ms"
        }
        read_max_time_spent {
          "10ms"
        }
        write_avg_time_spent {
          "10ms"
        }
        write_max_time_spent {
          "10ms"
        }
        compute_avg_time_spent {
          "10ms"
        }
        compute_max_time_spent {
          "10ms"
        }
        wait_ratio_avg: 1234.12
        wait_ratio_max: 1234.12
        read_ratio_avg: 1234.12
        read_ratio_max: 1234.12
        compute_ratio_avg: 1234.12
        compute_ratio_max: 1234.12
        write_ratio_avg: 1234.12
        write_ratio_max: 1234.12
        steps {
          kind: "sub-step-kind"
          substeps: "sub-step-1"
        }
        compute_mode {
          value: "BIGQUERY"
        }
      }
      timeline {
        elapsed_time {
          "10ms"
        }
        total_slot_time {
          "10ms"
        }
        pending_units: 1234
        completed_units: 1234
        active_units: 1234
        estimated_runnable_units: 1234
      }
      referenced_tables {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      referenced_routines {
        project_id: "2"
        dataset_id: "1"
        routine_id: "3"
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
      dml_stats {
        inserted_row_count: 1234
        deleted_row_count: 1234
        updated_row_count: 1234
      }
      ddl_target_table {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      ddl_target_row_access_policy {
        project_id: "1234"
        dataset_id: "1"
        table_id: "2"
        policy_id: "3"
      }
      ddl_target_routine {
        project_id: "2"
        dataset_id: "1"
        routine_id: "3"
      }
      ddl_target_dataset {
        project_id: "2"
        dataset_id: "1"
      }
      dcl_target_table {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      dcl_target_view {
        project_id: "2"
        dataset_id: "1"
        table_id: "3"
      }
      dcl_target_dataset {
        project_id: "2"
        dataset_id: "1"
      }
      search_statistics {
        index_unused_reasons {
          message: ""
          index_name: "test-index"
          base_table {
            project_id: "2"
            dataset_id: "1"
            table_id: "3"
          }
          code {
            value: "BASE_TABLE_TOO_SMALL"
          }
        }
        index_usage_mode {
          value: "PARTIALLY_USED"
        }
      }
      performance_insights {
        avg_previous_execution_time {
          "10ms"
        }
        stage_performance_standalone_insights {
          stage_id: 1234
          slot_contention: true
          insufficient_shuffle_quota: true
        }
        stage_performance_change_insights {
          stage_id: 1234
          input_data_change {
            records_read_diff_percentage: 12.12
          }
        }
      }
      materialized_view_statistics {
        materialized_view {
          chosen: true
          estimated_bytes_saved: 1234
          rejected_reason {
            value: "BASE_TABLE_DATA_CHANGE"
          }
          table_reference {
            project_id: "2"
            dataset_id: "1"
            table_id: "3"
          }
        }
      }
      metadata_cache_statistics {
        table_metadata_cache_usage {
          explanation: "test-table-metadata"
          unused_reason {
            value: "EXCEEDED_MAX_STALENESS"
          }
          table_reference {
            project_id: "2"
            dataset_id: "1"
            table_id: "3"
          }
        }
      }
    }
  }
  error_result {
    reason: ""
    location: ""
    message: ""
  }
})");
}

TEST(JobTest, JobToFromJson) {
  auto const* const expected_text =
      R"({"configuration":{"dryRun":true,"jobTimeoutMs":"10","jobType":"QUERY")"
      R"(,"labels":{"label-key1":"label-val1"},"query":{"allowLargeResults":true)"
      R"(,"clustering":{"fields":["clustering-field-1","clustering-field-2"]})"
      R"(,"connectionProperties":[{"key":"conn-prop-key","value":"conn-prop-val"}])"
      R"(,"createDisposition":"job-create-disposition","createSession":true)"
      R"(,"defaultDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"destinationEncryptionConfiguration":{"kmsKeyName":"encryption-key-name"})"
      R"(,"destinationTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"flattenResults":true,"maximumBytesBilled":"0","parameterMode":"job-param-mode")"
      R"(,"preserveNulls":true,"priority":"job-priority","query":"select 1;")"
      R"(,"queryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"structTypes":[{"description":"qp-struct-description","name":"qp-struct-name")"
      R"(,"type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{"arrayValues":[{"arrayValues":[])"
      R"(,"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}])"
      R"(,"structValues":{"qp-map-key":{"arrayValues":[],"structValues":{})"
      R"(,"value":"qp-map-value"}},"value":"query-parameter-value"}}])"
      R"(,"rangePartitioning":{"field":"rp-field-1","range":{"end":"range-end")"
      R"(,"interval":"range-interval","start":"range-start"}})"
      R"(,"schemaUpdateOptions":["job-update-options"])"
      R"(,"scriptOptions":{"keyResultStatement":"FIRST_SELECT")"
      R"(,"statementByteBudget":"10","statementTimeoutMs":"10"})"
      R"(,"systemVariables":{"types":{"sql-struct-type-key-1":{)"
      R"("structType":{"fields":[{"name":"f1-sql-struct-type-int64"}]})"
      R"(,"sub_type_index":2,"typeKind":"INT64"})"
      R"(,"sql-struct-type-key-2":{"structType":{"fields":[{"name":"f2-sql-struct-type-string"}]})"
      R"(,"sub_type_index":2,"typeKind":"STRING"})"
      R"(,"sql-struct-type-key-3":{"arrayElementType":{"structType":{)"
      R"("fields":[{"name":"f2-sql-struct-type-string"}]},"sub_type_index":2)"
      R"(,"typeKind":"STRING"},"sub_type_index":1,"typeKind":"STRING"}})"
      R"(,"values":{"fields":{"bool-key":{"kind_index":3,"valueKind":true})"
      R"(,"double-key":{"kind_index":1,"valueKind":3.4},"string-key":{"kind_index":2,"valueKind":"val3"}}}})"
      R"(,"timePartitioning":{"expirationTime":"0","field":"tp-field-1","type":"tp-field-type"})"
      R"(,"useLegacySql":true,"useQueryCache":true,"writeDisposition":"job-write-disposition"}})"
      R"(,"etag":"etag","id":"1","jobReference":{"jobId":"2","location":"us-east","projectId":"1"})"
      R"(,"kind":"Job","selfLink":"self-link","statistics":{"completionRatio":1234.1234)"
      R"(,"creationTime":"10","dataMaskingStatistics":{"dataMaskingApplied":true},"endTime":"10")"
      R"(,"finalExecutionDurationMs":"10","numChildJobs":"1234","parentJobId":"parent-job-123")"
      R"(,"query":{"billingTier":1234,"cacheHit":true,"dclTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"dclTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dclTargetView":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"ddlAffectedRowAccessPolicyCount":"1234","ddlOperationPerformed":"ddl_operation_performed")"
      R"(,"ddlTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"ddlTargetRoutine":{"datasetId":"1","projectId":"2","routineId":"3"})"
      R"(,"ddlTargetRowAccessPolicy":{"datasetId":"1","policyId":"3","projectId":"1234","tableId":"2"})"
      R"(,"ddlTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dmlStats":{"deletedRowCount":"1234","insertedRowCount":"1234","updatedRowCount":"1234"})"
      R"(,"estimatedBytesProcessed":"1234","materializedViewStatistics":{"materializedView":[{"chosen":true)"
      R"(,"estimatedBytesSaved":"1234","rejectedReason":"BASE_TABLE_DATA_CHANGE")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"}}]})"
      R"(,"metadataCacheStatistics":{"tableMetadataCacheUsage":[{"explanation":"test-table-metadata")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"unusedReason":"EXCEEDED_MAX_STALENESS"}]})"
      R"(,"numDmlAffectedRows":"1234","performanceInsights":{"avgPreviousExecutionMs":"10")"
      R"(,"stagePerformanceChangeInsights":{"inputDataChange":{"recordsReadDiffPercentage":12.119999885559082})"
      R"(,"stageId":"1234"},"stagePerformanceStandaloneInsights":{"insufficientShuffleQuota":true)"
      R"(,"slotContention":true,"stageId":"1234"}},"queryPlan":[{"completedParallelInputs":"1234")"
      R"(,"computeMode":"BIGQUERY","computeMsAvg":"10","computeMsMax":"10")"
      R"(,"computeRatioAvg":1234.1234,"computeRatioMax":1234.1234,"endMs":"10")"
      R"(,"id":"1234","inputStages":["1234"],"name":"test-explain","parallelInputs":"1234")"
      R"(,"readMsAvg":"10","readMsMax":"10","readRatioAvg":1234.1234,"readRatioMax":1234.1234)"
      R"(,"recordsRead":"1234","recordsWritten":"1234","shuffleOutputBytes":"1234")"
      R"(,"shuffleOutputBytesSpilled":"1234","slotMs":"10","startMs":"10","status":"explain-status")"
      R"(,"steps":[{"kind":"sub-step-kind","substeps":["sub-step-1"]}])"
      R"(,"waitMsAvg":"10","waitMsMax":"10","waitRatioAvg":1234.1234)"
      R"(,"waitRatioMax":1234.1234,"writeMsAvg":"10","writeMsMax":"10")"
      R"(,"writeRatioAvg":1234.1234,"writeRatioMax":1234.1234}])"
      R"(,"referencedRoutines":[{"datasetId":"1","projectId":"2","routineId":"3"}])"
      R"(,"referencedTables":[{"datasetId":"1","projectId":"2","tableId":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]},"collation":"")"
      R"(,"defaultValueExpression":"","description":"","fields":{"fields":[]})"
      R"(,"maxLength":0,"mode":"fmode","name":"fname-1","policyTags":{"names":[]})"
      R"(,"precision":0,"rangeElementType":{"type":""},"roundingMode":"")"
      R"(,"scale":0,"type":""}]},"searchStatistics":{"indexUnusedReasons":[{)"
      R"("baseTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"code":"BASE_TABLE_TOO_SMALL","indexName":"test-index","message":""}])"
      R"(,"indexUsageMode":"PARTIALLY_USED"},"statementType":"statement_type")"
      R"(,"timeline":[{"activeUnits":"1234","completedUnits":"1234","elapsedMs":"10")"
      R"(,"estimatedRunnableUnits":"1234","pendingUnits":"1234","totalSlotMs":"10"}])"
      R"(,"totalBytesBilled":"1234","totalBytesProcessed":"1234",)"
      R"("totalBytesProcessedAccuracy":"total_bytes_processed_accuracy","totalPartitionsProcessed":"1234")"
      R"(,"totalSlotMs":"10","transferredBytes":"1234")"
      R"(,"undeclaredQueryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[],"type":"array-struct-type"}}])"
      R"(,"type":"array-type"},"structTypes":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{)"
      R"("arrayValues":[{"arrayValues":[],"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}],"structValues":{"qp-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"qp-map-value"}},"value":"query-parameter-value"}}]})"
      R"(,"quotaDeferments":["quota-defer-1"],"reservation_id":"reservation-id-123")"
      R"(,"rowLevelSecurityStatistics":{"rowLevelSecurityApplied":true})"
      R"(,"scriptStatistics":{"evaluationKind":"STATEMENT")"
      R"(,"stackFrames":[{"endColumn":1234,"endLine":1234,"procedureId":"proc-id")"
      R"(,"startColumn":1234,"startLine":1234,"text":"stack-frame-text"}]})"
      R"(,"sessionInfo":{"sessionId":"session-id-123"},"startTime":"10")"
      R"(,"totalBytesProcessed":"1234","totalSlotMs":"10","transactionInfo":{"transactionId":"transaction-id-123"}})"
      R"(,"status":{"errorResult":{"location":"","message":"","reason":""})"
      R"(,"errors":[],"state":"DONE"},"user_email":"a@b.com"})";

  auto expected_json = nlohmann::json::parse(expected_text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto expected = MakeJob();

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);

  Job actual;
  from_json(actual_json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

TEST(JobTest, ListFormatJobToFromJson) {
  auto const* const expected_text =
      R"({"configuration":{"dryRun":true,"jobTimeoutMs":"10")"
      R"(,"jobType":"QUERY","labels":{"label-key1":"label-val1"})"
      R"(,"query":{"allowLargeResults":true,"clustering":{"fields":["clustering-field-1")"
      R"(,"clustering-field-2"]},"connectionProperties":[{"key":"conn-prop-key")"
      R"(,"value":"conn-prop-val"}],"createDisposition":"job-create-disposition")"
      R"(,"createSession":true,"defaultDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"destinationEncryptionConfiguration":{"kmsKeyName":"encryption-key-name"})"
      R"(,"destinationTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"flattenResults":true,"maximumBytesBilled":"0","parameterMode":"job-param-mode")"
      R"(,"preserveNulls":true,"priority":"job-priority","query":"select 1;")"
      R"(,"queryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[],"type":"array-struct-type"}}])"
      R"(,"type":"array-type"},"structTypes":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{"arrayValues":[{"arrayValues":[])"
      R"(,"structValues":{"array-map-key":{"arrayValues":[],"structValues":{})"
      R"(,"value":"array-map-value"}},"value":"array-val-2"}],"structValues":{})"
      R"(,"value":"array-val-1"}],"structValues":{"qp-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"qp-map-value"}},"value":"query-parameter-value"}}])"
      R"(,"rangePartitioning":{"field":"rp-field-1","range":{"end":"range-end")"
      R"(,"interval":"range-interval","start":"range-start"}})"
      R"(,"schemaUpdateOptions":["job-update-options"])"
      R"(,"scriptOptions":{"keyResultStatement":"FIRST_SELECT")"
      R"(,"statementByteBudget":"10","statementTimeoutMs":"10"})"
      R"(,"systemVariables":{"types":{"sql-struct-type-key-1":{)"
      R"("structType":{"fields":[{"name":"f1-sql-struct-type-int64"}]})"
      R"(,"sub_type_index":2,"typeKind":"INT64"})"
      R"(,"sql-struct-type-key-2":{"structType":{"fields":[{"name":"f2-sql-struct-type-string"}]})"
      R"(,"sub_type_index":2,"typeKind":"STRING"})"
      R"(,"sql-struct-type-key-3":{"arrayElementType":{"structType":{"fields":[{"name":"f2-sql-struct-type-string"}]})"
      R"(,"sub_type_index":2,"typeKind":"STRING"},"sub_type_index":1,"typeKind":"STRING"}})"
      R"(,"values":{"fields":{"bool-key":{"kind_index":3,"valueKind":true})"
      R"(,"double-key":{"kind_index":1,"valueKind":3.4},"string-key":{"kind_index":2,"valueKind":"val3"}}}})"
      R"(,"timePartitioning":{"expirationTime":"0","field":"tp-field-1","type":"tp-field-type"})"
      R"(,"useLegacySql":true,"useQueryCache":true,"writeDisposition":"job-write-disposition"}})"
      R"(,"errorResult":{"location":"","message":"","reason":""},"id":"1")"
      R"(,"jobReference":{"jobId":"2","location":"us-east","projectId":"1"},"kind":"Job")"
      R"(,"principal_subject":"principal-sub","state":"DONE","statistics":{"completionRatio":1234.1234)"
      R"(,"creationTime":"10","dataMaskingStatistics":{"dataMaskingApplied":true},"endTime":"10")"
      R"(,"finalExecutionDurationMs":"10","numChildJobs":"1234","parentJobId":"parent-job-123")"
      R"(,"query":{"billingTier":1234,"cacheHit":true,"dclTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"dclTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dclTargetView":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"ddlAffectedRowAccessPolicyCount":"1234","ddlOperationPerformed":"ddl_operation_performed")"
      R"(,"ddlTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"ddlTargetRoutine":{"datasetId":"1","projectId":"2","routineId":"3"})"
      R"(,"ddlTargetRowAccessPolicy":{"datasetId":"1","policyId":"3")"
      R"(,"projectId":"1234","tableId":"2"},"ddlTargetTable":{"datasetId":"1")"
      R"(,"projectId":"2","tableId":"3"},"dmlStats":{"deletedRowCount":"1234")"
      R"(,"insertedRowCount":"1234","updatedRowCount":"1234"},"estimatedBytesProcessed":"1234")"
      R"(,"materializedViewStatistics":{"materializedView":[{"chosen":true,"estimatedBytesSaved":"1234")"
      R"(,"rejectedReason":"BASE_TABLE_DATA_CHANGE")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"}}]})"
      R"(,"metadataCacheStatistics":{"tableMetadataCacheUsage":[{"explanation":"test-table-metadata")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"unusedReason":"EXCEEDED_MAX_STALENESS"}]})"
      R"(,"numDmlAffectedRows":"1234","performanceInsights":{"avgPreviousExecutionMs":"10")"
      R"(,"stagePerformanceChangeInsights":{"inputDataChange":{"recordsReadDiffPercentage":12.119999885559082})"
      R"(,"stageId":"1234"},"stagePerformanceStandaloneInsights":{"insufficientShuffleQuota":true)"
      R"(,"slotContention":true,"stageId":"1234"}},"queryPlan":[{"completedParallelInputs":"1234")"
      R"(,"computeMode":"BIGQUERY","computeMsAvg":"10","computeMsMax":"10")"
      R"(,"computeRatioAvg":1234.1234,"computeRatioMax":1234.1234,"endMs":"10","id":"1234")"
      R"(,"inputStages":["1234"],"name":"test-explain","parallelInputs":"1234")"
      R"(,"readMsAvg":"10","readMsMax":"10","readRatioAvg":1234.1234,"readRatioMax":1234.1234)"
      R"(,"recordsRead":"1234","recordsWritten":"1234","shuffleOutputBytes":"1234")"
      R"(,"shuffleOutputBytesSpilled":"1234","slotMs":"10","startMs":"10","status":"explain-status")"
      R"(,"steps":[{"kind":"sub-step-kind","substeps":["sub-step-1"]}],"waitMsAvg":"10")"
      R"(,"waitMsMax":"10","waitRatioAvg":1234.1234,"waitRatioMax":1234.1234,"writeMsAvg":"10")"
      R"(,"writeMsMax":"10","writeRatioAvg":1234.1234,"writeRatioMax":1234.1234}])"
      R"(,"referencedRoutines":[{"datasetId":"1","projectId":"2","routineId":"3"}])"
      R"(,"referencedTables":[{"datasetId":"1","projectId":"2","tableId":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]},"collation":"")"
      R"(,"defaultValueExpression":"","description":"","fields":{"fields":[]})"
      R"(,"maxLength":0,"mode":"fmode","name":"fname-1","policyTags":{"names":[]})"
      R"(,"precision":0,"rangeElementType":{"type":""},"roundingMode":"")"
      R"(,"scale":0,"type":""}]},"searchStatistics":{"indexUnusedReasons":[{)"
      R"("baseTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"code":"BASE_TABLE_TOO_SMALL","indexName":"test-index","message":""}])"
      R"(,"indexUsageMode":"PARTIALLY_USED"},"statementType":"statement_type")"
      R"(,"timeline":[{"activeUnits":"1234","completedUnits":"1234","elapsedMs":"10")"
      R"(,"estimatedRunnableUnits":"1234","pendingUnits":"1234","totalSlotMs":"10"}])"
      R"(,"totalBytesBilled":"1234","totalBytesProcessed":"1234")"
      R"(,"totalBytesProcessedAccuracy":"total_bytes_processed_accuracy")"
      R"(,"totalPartitionsProcessed":"1234","totalSlotMs":"10","transferredBytes":"1234")"
      R"(,"undeclaredQueryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[],"type":"array-struct-type"}}])"
      R"(,"type":"array-type"},"structTypes":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{"arrayValues":[{"arrayValues":[])"
      R"(,"structValues":{"array-map-key":{"arrayValues":[],"structValues":{})"
      R"(,"value":"array-map-value"}},"value":"array-val-2"}],"structValues":{},"value":"array-val-1"}])"
      R"(,"structValues":{"qp-map-key":{"arrayValues":[],"structValues":{},"value":"qp-map-value"}})"
      R"(,"value":"query-parameter-value"}}]},"quotaDeferments":["quota-defer-1"])"
      R"(,"reservation_id":"reservation-id-123","rowLevelSecurityStatistics":{"rowLevelSecurityApplied":true})"
      R"(,"scriptStatistics":{"evaluationKind":"STATEMENT")"
      R"(,"stackFrames":[{"endColumn":1234,"endLine":1234,"procedureId":"proc-id")"
      R"(,"startColumn":1234,"startLine":1234,"text":"stack-frame-text"}]})"
      R"(,"sessionInfo":{"sessionId":"session-id-123"},"startTime":"10")"
      R"(,"totalBytesProcessed":"1234","totalSlotMs":"10")"
      R"(,"transactionInfo":{"transactionId":"transaction-id-123"}})"
      R"(,"status":{"errorResult":{"location":"","message":"","reason":""})"
      R"(,"errors":[],"state":"DONE"},"user_email":"a@b.com"})";

  auto expected_json = nlohmann::json::parse(expected_text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto expected = MakeListFormatJob();

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);

  ListFormatJob actual;
  from_json(actual_json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

// NOLINTEND

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
