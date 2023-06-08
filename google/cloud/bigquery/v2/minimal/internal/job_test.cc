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

  EXPECT_EQ(job.DebugString("Job", TracingOptions{}), R"(Job {)"
                                                      R"( etag: "etag")"
                                                      R"( kind: "Job")"
                                                      R"( id: "1")"
                                                      R"( job_configuration {)"
                                                      R"( job_type: "QUERY")"
                                                      R"( query: "select 1;")"
                                                      R"( })"
                                                      R"( job_reference {)"
                                                      R"( project_id: "1")"
                                                      R"( job_id: "2")"
                                                      R"( location: "")"
                                                      R"( })"
                                                      R"( job_status: "DONE")"
                                                      R"( error_result: "")"
                                                      R"( })");
}

TEST(JobTest, ListFormatJobDebugString) {
  auto job = bigquery_v2_minimal_testing::MakeListFormatJob();

  EXPECT_EQ(job.DebugString("ListFormatJob", TracingOptions{}),
            R"(ListFormatJob {)"
            R"( id: "1")"
            R"( kind: "Job")"
            R"( state: "DONE")"
            R"( job_configuration {)"
            R"( job_type: "QUERY")"
            R"( query: "select 1;")"
            R"( })"
            R"( job_reference {)"
            R"( project_id: "1")"
            R"( job_id: "2")"
            R"( location: "")"
            R"( })"
            R"( job_status: "DONE")"
            R"( error_result: "")"
            R"( })");
}

TEST(JobTest, JobToFromJson) {
  auto const* const expected_text =
      R"({"configuration":{"dry_run":false,"job_timeout_ms":0)"
      R"(,"job_type":"QUERY")"
      R"(,"labels":{},"query_config":{"allow_large_results":false)"
      R"(,"connection_properties":[],"create_disposition":"")"
      R"(,"create_session":false,"default_dataset":{"dataset_id":"")"
      R"(,"project_id":""},"destination_table":{"dataset_id":"")"
      R"(,"project_id":"","table_id":""},"flatten_results":false)"
      R"(,"maximum_bytes_billed":0,"parameter_mode":"")"
      R"(,"preserve_nulls":false,"priority":"","query":"select 1;")"
      R"(,"query_parameters":[],"schema_update_options":[])"
      R"(,"use_legacy_sql":false,"use_query_cache":false)"
      R"(,"write_disposition":""}},"etag":"etag","id":"1")"
      R"(,"kind":"Job","reference":{"job_id":"2","location":"")"
      R"(,"project_id":"1"},"self_link":"","statistics":{)"
      R"("completion_ratio":1234.1234,"creation_time":10,)"
      R"("data_masking_applied":true,"end_time":10)"
      R"(,"final_execution_duration":10,"job_query_stats":{)"
      R"("billing_tier":1234,"cache_hit":true,"dcl_target_dataset":{)"
      R"("dataset_id":"1","project_id":"2"},"dcl_target_table":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"dcl_target_view":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"},"ddl_affected_row_access_policy_count":1234)"
      R"(,"ddl_destination_table":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"})"
      R"(,"ddl_operation_performed":"ddl_operation_performed")"
      R"(,"ddl_target_dataset":{"dataset_id":"1","project_id":"2"})"
      R"(,"ddl_target_routine":{"dataset_id":"1","project_id":"2")"
      R"(,"routine_id":"3"},"ddl_target_row_access_policy":{)"
      R"("dataset_id":"1","policy_id":"3","project_id":"1234")"
      R"(,"table_id":"2"},"ddl_target_table":{"dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"},"dml_stats":{)"
      R"("deleted_row_count":1234,"inserted_row_count":1234)"
      R"(,"updated_row_count":1234},"estimated_bytes_processed":1234)"
      R"(,"materialized_view_statistics":{"materialized_view":[{)"
      R"("chosen":true,"estimated_bytes_saved":1234,"rejected_reason":{)"
      R"("value":"BASE_TABLE_DATA_CHANGE"},"table_reference":{)"
      R"("dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"}}]},"metadata_cache_statistics":{)"
      R"("table_metadata_cache_usage":[{"explanation":"test-table-metadata")"
      R"(,"table_reference":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"})"
      R"(,"unused_reason":{"value":"EXCEEDED_MAX_STALENESS"}}]})"
      R"(,"num_dml_affected_rows":1234,"performance_insights":{)"
      R"("avg_previous_execution_time":10)"
      R"(,"stage_performance_change_insights":{)"
      R"("input_data_change":{)"
      R"("records_read_diff_percentage":12.119999885559082})"
      R"(,"stage_id":1234},"stage_performance_standalone_insights":{)"
      R"("insufficient_shuffle_quota":true,"slot_contention":true)"
      R"(,"stage_id":1234}},"query_plan":[{"completed_parallel_inputs":1234)"
      R"(,"compute_avg_time_spent":10,"compute_max_time_spent":10)"
      R"(,"compute_mode":{)"
      R"("value":"BIGQUERY"},"compute_ratio_avg":1234.1234)"
      R"(,"compute_ratio_max":1234.1234,"end_time":10,"id":1234)"
      R"(,"input_stages":[1234],"name":"test-explain","parallel_inputs":1234)"
      R"(,"read_avg_time_spent":10,"read_max_time_spent":10)"
      R"(,"read_ratio_avg":1234.1234,"read_ratio_max":1234.1234)"
      R"(,"records_read":1234,"records_written":1234)"
      R"(,"shuffle_output_bytes":1234,"shuffle_output_bytes_spilled":1234)"
      R"(,"slot_time":10,"start_time":10,"status":"explain-status","steps":[{)"
      R"("kind":"sub-step-kind","substeps":["sub-step-1"]}])"
      R"(,"wait_avg_time_spent":10,"wait_max_time_spent":10)"
      R"(,"wait_ratio_avg":1234.1234,"wait_ratio_max":1234.1234)"
      R"(,"write_avg_time_spent":10,"write_max_time_spent":10)"
      R"(,"write_ratio_avg":1234.1234,"write_ratio_max":1234.1234}])"
      R"(,"referenced_routines":[{"dataset_id":"1","project_id":"2")"
      R"(,"routine_id":"3"}])"
      R"(,"referenced_tables":[{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]},"collation":"")"
      R"(,"data_classification_tags":{"names":[]})"
      R"(,"default_value_expression":"","description":"","fields":{)"
      R"("fields":[]},"is_measure":true,"max_length":0,"mode":"fmode")"
      R"(,"name":"fname-1","policy_tags":{"names":[]},"precision":0)"
      R"(,"range_element_type":{"type":""},"rounding_mode":{"value":""})"
      R"(,"scale":0,"type":""}]},"search_statistics":{)"
      R"("index_unused_reasons":[{"base_table":{"dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"},"code":{)"
      R"("value":"BASE_TABLE_TOO_SMALL"},"index_name":"test-index")"
      R"(,"message":""}],"index_usage_mode":{"value":"PARTIALLY_USED"}})"
      R"(,"statement_type":"statement_type","timeline":[{"active_units":1234)"
      R"(,"completed_units":1234,"elapsed_time":10)"
      R"(,"estimated_runnable_units":1234,"pending_units":1234)"
      R"(,"total_slot_time":10}],"total_bytes_billed":1234)"
      R"(,"total_bytes_processed":1234)"
      R"(,"total_bytes_processed_accuracy":"total_bytes_processed_accuracy")"
      R"(,"total_partitions_processed":1234,"total_slot_time":10)"
      R"(,"transferred_bytes":1234,"undeclared_query_parameters":[{)"
      R"("name":"query-parameter-name","parameter_type":{)"
      R"("array_type":{"struct_types":[{)"
      R"("description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"struct_types":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"struct_types":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"struct_types":[])"
      R"(,"type":"qp-struct-type"}}],"type":"query-parameter-type"})"
      R"(,"parameter_value":{"array_values":[{)"
      R"("array_values":[{"array_values":[],"struct_values":{)"
      R"("array-map-key":{"array_values":[],"struct_values":{})"
      R"(,"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"struct_values":{},"value":"array-val-1"}],"struct_values":{)"
      R"("qp-map-key":{"array_values":[],"struct_values":{})"
      R"(,"value":"qp-map-value"}},"value":"query-parameter-value"}}]})"
      R"(,"num_child_jobs":1234,"parent_job_id":"parent-job-123")"
      R"(,"quota_deferments":["quota-defer-1"])"
      R"(,"reservation_id":"reservation-id-123","row_level_security_applied":true)"
      R"(,"script_statistics":{"evaluation_kind":{"value":"STATEMENT"})"
      R"(,"stack_frames":[{"end_column":1234,"end_line":1234)"
      R"(,"procedure_id":"proc-id","start_column":1234,"start_line":1234)"
      R"(,"text":"stack-frame-text"}]},"session_id":"session-id-123")"
      R"(,"start_time":10,"total_bytes_processed":1234)"
      R"(,"total_modified_partitions":1234,"total_slot_time":10)"
      R"(,"transaction_id":"transaction-id-123"},"status":{)"
      R"("error_result":{"location":"","message":"","reason":""})"
      R"(,"errors":[],"state":"DONE"},"user_email":""})";

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
      R"({"configuration":{"dry_run":false,"job_timeout_ms":0)"
      R"(,"job_type":"QUERY","labels":{},"query_config":{)"
      R"("allow_large_results":false,"connection_properties":[])"
      R"(,"create_disposition":"","create_session":false)"
      R"(,"default_dataset":{"dataset_id":"","project_id":""})"
      R"(,"destination_table":{"dataset_id":"","project_id":"")"
      R"(,"table_id":""})"
      R"(,"flatten_results":false,"maximum_bytes_billed":0)"
      R"(,"parameter_mode":"","preserve_nulls":false,"priority":"")"
      R"(,"query":"select 1;","query_parameters":[])"
      R"(,"schema_update_options":[])"
      R"(,"use_legacy_sql":false)"
      R"(,"use_query_cache":false,"write_disposition":""}})"
      R"(,"error_result":{"location":"","message":"","reason":""},"id":"1")"
      R"(,"kind":"Job","principal_subject":"","reference":{)"
      R"("job_id":"2","location":"","project_id":"1"},"state":"DONE")"
      R"(,"statistics":{"completion_ratio":1234.1234,"creation_time":10)"
      R"(,"data_masking_applied":true,"end_time":10)"
      R"(,"final_execution_duration":10,"job_query_stats":{)"
      R"("billing_tier":1234,"cache_hit":true,"dcl_target_dataset":{)"
      R"("dataset_id":"1","project_id":"2"},"dcl_target_table":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"dcl_target_view":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"},"ddl_affected_row_access_policy_count":1234)"
      R"(,"ddl_destination_table":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"},"ddl_operation_performed":"ddl_operation_performed")"
      R"(,"ddl_target_dataset":{"dataset_id":"1","project_id":"2"})"
      R"(,"ddl_target_routine":{"dataset_id":"1","project_id":"2")"
      R"(,"routine_id":"3"},"ddl_target_row_access_policy":{)"
      R"("dataset_id":"1","policy_id":"3","project_id":"1234","table_id":"2"})"
      R"(,"ddl_target_table":{"dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"dml_stats":{"deleted_row_count":1234,"inserted_row_count":1234)"
      R"(,"updated_row_count":1234},"estimated_bytes_processed":1234)"
      R"(,"materialized_view_statistics":{"materialized_view":[{)"
      R"("chosen":true,"estimated_bytes_saved":1234,"rejected_reason":{)"
      R"("value":"BASE_TABLE_DATA_CHANGE"},"table_reference":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"}}]})"
      R"(,"metadata_cache_statistics":{"table_metadata_cache_usage":[{)"
      R"("explanation":"test-table-metadata","table_reference":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"unused_reason":{"value":"EXCEEDED_MAX_STALENESS"}}]})"
      R"(,"num_dml_affected_rows":1234,"performance_insights":{)"
      R"("avg_previous_execution_time":10)"
      R"(,"stage_performance_change_insights":{"input_data_change":{)"
      R"("records_read_diff_percentage":12.119999885559082})"
      R"(,"stage_id":1234})"
      R"(,"stage_performance_standalone_insights":{)"
      R"("insufficient_shuffle_quota":true,"slot_contention":true)"
      R"(,"stage_id":1234}},"query_plan":[{"completed_parallel_inputs":1234)"
      R"(,"compute_avg_time_spent":10,"compute_max_time_spent":10)"
      R"(,"compute_mode":{"value":"BIGQUERY"},"compute_ratio_avg":1234.1234)"
      R"(,"compute_ratio_max":1234.1234,"end_time":10,"id":1234)"
      R"(,"input_stages":[1234],"name":"test-explain","parallel_inputs":1234)"
      R"(,"read_avg_time_spent":10,"read_max_time_spent":10)"
      R"(,"read_ratio_avg":1234.1234,"read_ratio_max":1234.1234)"
      R"(,"records_read":1234,"records_written":1234)"
      R"(,"shuffle_output_bytes":1234,"shuffle_output_bytes_spilled":1234)"
      R"(,"slot_time":10,"start_time":10)"
      R"(,"status":"explain-status","steps":[{)"
      R"("kind":"sub-step-kind","substeps":["sub-step-1"]}])"
      R"(,"wait_avg_time_spent":10,"wait_max_time_spent":10)"
      R"(,"wait_ratio_avg":1234.1234,"wait_ratio_max":1234.1234)"
      R"(,"write_avg_time_spent":10,"write_max_time_spent":10)"
      R"(,"write_ratio_avg":1234.1234,"write_ratio_max":1234.1234}])"
      R"(,"referenced_routines":[{"dataset_id":"1")"
      R"(,"project_id":"2","routine_id":"3"}])"
      R"(,"referenced_tables":[{"dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]},"collation":"")"
      R"(,"data_classification_tags":{"names":[]})"
      R"(,"default_value_expression":"")"
      R"(,"description":"","fields":{"fields":[]})"
      R"(,"is_measure":true,"max_length":0)"
      R"(,"mode":"fmode","name":"fname-1","policy_tags":{"names":[]})"
      R"(,"precision":0,"range_element_type":{"type":""})"
      R"(,"rounding_mode":{"value":""},"scale":0,"type":""}]})"
      R"(,"search_statistics":{"index_unused_reasons":[{"base_table":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"code":{"value":"BASE_TABLE_TOO_SMALL"},"index_name":"test-index")"
      R"(,"message":""}],"index_usage_mode":{"value":"PARTIALLY_USED"}})"
      R"(,"statement_type":"statement_type","timeline":[{"active_units":1234)"
      R"(,"completed_units":1234,"elapsed_time":10)"
      R"(,"estimated_runnable_units":1234)"
      R"(,"pending_units":1234,"total_slot_time":10}])"
      R"(,"total_bytes_billed":1234)"
      R"(,"total_bytes_processed":1234)"
      R"(,"total_bytes_processed_accuracy":"total_bytes_processed_accuracy")"
      R"(,"total_partitions_processed":1234,"total_slot_time":10)"
      R"(,"transferred_bytes":1234,"undeclared_query_parameters":[{)"
      R"("name":"query-parameter-name","parameter_type":{"array_type":{)"
      R"("struct_types":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"struct_types":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"struct_types":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"struct_types":[])"
      R"(,"type":"qp-struct-type"}}],"type":"query-parameter-type"})"
      R"(,"parameter_value":{"array_values":[{"array_values":[{)"
      R"("array_values":[],"struct_values":{"array-map-key":{"array_values":[])"
      R"(,"struct_values":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"struct_values":{},"value":"array-val-1"}],"struct_values":{)"
      R"("qp-map-key":{"array_values":[],"struct_values":{},"value":"qp-map-value"}})"
      R"(,"value":"query-parameter-value"}}]},"num_child_jobs":1234)"
      R"(,"parent_job_id":"parent-job-123","quota_deferments":["quota-defer-1"])"
      R"(,"reservation_id":"reservation-id-123")"
      R"(,"row_level_security_applied":true,"script_statistics":{)"
      R"("evaluation_kind":{"value":"STATEMENT"},"stack_frames":[{)"
      R"("end_column":1234,"end_line":1234,"procedure_id":"proc-id")"
      R"(,"start_column":1234,"start_line":1234,"text":"stack-frame-text"}]})"
      R"(,"session_id":"session-id-123","start_time":10)"
      R"(,"total_bytes_processed":1234,"total_modified_partitions":1234)"
      R"(,"total_slot_time":10,"transaction_id":"transaction-id-123"})"
      R"(,"status":{"error_result":{"location":"","message":"","reason":""})"
      R"(,"errors":[],"state":"DONE"},"user_email":""})";

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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
