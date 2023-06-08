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

#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MakeJobStats;

TEST(JobStatsTest, JobStatsToFromJson) {
  auto const* const expected_text =
      R"({"completion_ratio":1234.1234,"creation_time":10)"
      R"(,"data_masking_applied":true,"end_time":10)"
      R"(,"final_execution_duration":10,"job_query_stats":{)"
      R"("billing_tier":1234,"cache_hit":true,"dcl_target_dataset":{)"
      R"("dataset_id":"1","project_id":"2"},"dcl_target_table":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"},"dcl_target_view":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"ddl_affected_row_access_policy_count":1234,"ddl_destination_table":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"ddl_operation_performed":"ddl_operation_performed")"
      R"(,"ddl_target_dataset":{"dataset_id":"1","project_id":"2"})"
      R"(,"ddl_target_routine":{"dataset_id":"1","project_id":"2")"
      R"(,"routine_id":"3"},"ddl_target_row_access_policy":{)"
      R"("dataset_id":"1","policy_id":"3","project_id":"1234")"
      R"(,"table_id":"2"},"ddl_target_table":{"dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"},"dml_stats":{"deleted_row_count":1234)"
      R"(,"inserted_row_count":1234,"updated_row_count":1234})"
      R"(,"estimated_bytes_processed":1234,"materialized_view_statistics":{)"
      R"("materialized_view":[{"chosen":true,"estimated_bytes_saved":1234)"
      R"(,"rejected_reason":{"value":"BASE_TABLE_DATA_CHANGE"})"
      R"(,"table_reference":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"}}]},"metadata_cache_statistics":{)"
      R"("table_metadata_cache_usage":[{"explanation":"test-table-metadata")"
      R"(,"table_reference":{"dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"unused_reason":{"value":"EXCEEDED_MAX_STALENESS"}}]})"
      R"(,"num_dml_affected_rows":1234,"performance_insights":{)"
      R"("avg_previous_execution_time":10,"stage_performance_change_insights":{)"
      R"("input_data_change":{"records_read_diff_percentage":12.119999885559082})"
      R"(,"stage_id":1234},"stage_performance_standalone_insights":{)"
      R"("insufficient_shuffle_quota":true,"slot_contention":true,"stage_id":1234}})"
      R"(,"query_plan":[{"completed_parallel_inputs":1234,"compute_avg_time_spent":10)"
      R"(,"compute_max_time_spent":10,"compute_mode":{"value":"BIGQUERY"})"
      R"(,"compute_ratio_avg":1234.1234,"compute_ratio_max":1234.1234)"
      R"(,"end_time":10,"id":1234,"input_stages":[1234],"name":"test-explain")"
      R"(,"parallel_inputs":1234,"read_avg_time_spent":10,"read_max_time_spent":10)"
      R"(,"read_ratio_avg":1234.1234,"read_ratio_max":1234.1234,"records_read":1234)"
      R"(,"records_written":1234,"shuffle_output_bytes":1234)"
      R"(,"shuffle_output_bytes_spilled":1234,"slot_time":10,"start_time":10)"
      R"(,"status":"explain-status","steps":[{"kind":"sub-step-kind")"
      R"(,"substeps":["sub-step-1"]}],"wait_avg_time_spent":10)"
      R"(,"wait_max_time_spent":10,"wait_ratio_avg":1234.1234)"
      R"(,"wait_ratio_max":1234.1234,"write_avg_time_spent":10)"
      R"(,"write_max_time_spent":10,"write_ratio_avg":1234.1234)"
      R"(,"write_ratio_max":1234.1234}],"referenced_routines":[{"dataset_id":"1")"
      R"(,"project_id":"2","routine_id":"3"}],"referenced_tables":[{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"}],"schema":{"fields":[{)"
      R"("categories":{"names":[]},"collation":"","data_classification_tags":{)"
      R"("names":[]},"default_value_expression":"","description":"","fields":{)"
      R"("fields":[]},"is_measure":true,"max_length":0,"mode":"fmode")"
      R"(,"name":"fname-1","policy_tags":{"names":[]},"precision":0)"
      R"(,"range_element_type":{"type":""},"rounding_mode":{"value":""})"
      R"(,"scale":0,"type":""}]},"search_statistics":{"index_unused_reasons":[{)"
      R"("base_table":{"dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"code":{"value":"BASE_TABLE_TOO_SMALL"},"index_name":"test-index")"
      R"(,"message":""}],"index_usage_mode":{"value":"PARTIALLY_USED"}})"
      R"(,"statement_type":"statement_type","timeline":[{"active_units":1234)"
      R"(,"completed_units":1234,"elapsed_time":10)"
      R"(,"estimated_runnable_units":1234)"
      R"(,"pending_units":1234,"total_slot_time":10}],"total_bytes_billed":1234)"
      R"(,"total_bytes_processed":1234)"
      R"(,"total_bytes_processed_accuracy":"total_bytes_processed_accuracy")"
      R"(,"total_partitions_processed":1234,"total_slot_time":10)"
      R"(,"transferred_bytes":1234,"undeclared_query_parameters":[{)"
      R"("name":"query-parameter-name","parameter_type":{)"
      R"("array_type":{"struct_types":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"struct_types":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"struct_types":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"struct_types":[])"
      R"(,"type":"qp-struct-type"}}],"type":"query-parameter-type"})"
      R"(,"parameter_value":{"array_values":[{"array_values":[{"array_values":[])"
      R"(,"struct_values":{"array-map-key":{"array_values":[],"struct_values":{})"
      R"(,"value":"array-map-value"}},"value":"array-val-2"}],"struct_values":{})"
      R"(,"value":"array-val-1"}],"struct_values":{)"
      R"("qp-map-key":{"array_values":[])"
      R"(,"struct_values":{},"value":"qp-map-value"}})"
      R"(,"value":"query-parameter-value"}}]})"
      R"(,"num_child_jobs":1234,"parent_job_id":"parent-job-123")"
      R"(,"quota_deferments":["quota-defer-1"],"reservation_id":"reservation-id-123")"
      R"(,"row_level_security_applied":true,"script_statistics":{)"
      R"("evaluation_kind":{"value":"STATEMENT"},"stack_frames":[{)"
      R"("end_column":1234,"end_line":1234,"procedure_id":"proc-id")"
      R"(,"start_column":1234,"start_line":1234,"text":"stack-frame-text"}]})"
      R"(,"session_id":"session-id-123","start_time":10)"
      R"(,"total_bytes_processed":1234,"total_modified_partitions":1234)"
      R"(,"total_slot_time":10,"transaction_id":"transaction-id-123"})";

  auto expected_json = nlohmann::json::parse(expected_text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto expected = MakeJobStats();

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);

  JobStatistics actual;
  from_json(actual_json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
