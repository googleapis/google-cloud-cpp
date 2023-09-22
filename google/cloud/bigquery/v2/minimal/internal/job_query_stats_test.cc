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

using ::google::cloud::bigquery_v2_minimal_testing::MakeJobQueryStats;

TEST(JobQueryStatsTest, JobQueryStatsToFromJson) {
  auto const* const expected_text =
      R"({"billingTier":1234,"cacheHit":true,"dclTargetDataset":{)"
      R"("datasetId":"1","projectId":"2"},"dclTargetTable":{)"
      R"("datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dclTargetView":{"datasetId":"1","projectId":"2")"
      R"(,"tableId":"3"},"ddlAffectedRowAccessPolicyCount":"1234")"
      R"(,"ddlOperationPerformed":"ddl_operation_performed")"
      R"(,"ddlTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"ddlTargetRoutine":{"datasetId":"1","projectId":"2")"
      R"(,"routineId":"3"},"ddlTargetRowAccessPolicy":{)"
      R"("datasetId":"1","policyId":"3","projectId":"1234")"
      R"(,"tableId":"2"})"
      R"(,"ddlTargetTable":{"datasetId":"1","projectId":"2")"
      R"(,"tableId":"3"},"dmlStats":{"deletedRowCount":"1234")"
      R"(,"insertedRowCount":"1234","updatedRowCount":"1234"})"
      R"(,"estimatedBytesProcessed":"1234","materializedViewStatistics":{)"
      R"("materializedView":[{"chosen":true,"estimatedBytesSaved":"1234")"
      R"(,"rejectedReason":"BASE_TABLE_DATA_CHANGE")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2")"
      R"(,"tableId":"3"}}]},"metadataCacheStatistics":{)"
      R"("tableMetadataCacheUsage":[{)"
      R"("explanation":"test-table-metadata")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2")"
      R"(,"tableId":"3"})"
      R"(,"unusedReason":"EXCEEDED_MAX_STALENESS"}]})"
      R"(,"numDmlAffectedRows":"1234","performanceInsights":{)"
      R"("avgPreviousExecutionMs":"10")"
      R"(,"stagePerformanceChangeInsights":{)"
      R"("inputDataChange":{)"
      R"("recordsReadDiffPercentage":12.119999885559082})"
      R"(,"stageId":"1234"},"stagePerformanceStandaloneInsights":{)"
      R"("insufficientShuffleQuota":true)"
      R"(,"slotContention":true,"stageId":"1234"}})"
      R"(,"queryPlan":[{"completedParallelInputs":"1234")"
      R"(,"computeMsAvg":"10")"
      R"(,"computeMsMax":"10","computeMode":)"
      R"("BIGQUERY")"
      R"(,"computeRatioAvg":1234.1234,"computeRatioMax":1234.1234)"
      R"(,"endMs":"10","id":"1234","inputStages":["1234"])"
      R"(,"name":"test-explain")"
      R"(,"parallelInputs":"1234","readMsAvg":"10")"
      R"(,"readMsMax":"10")"
      R"(,"readRatioAvg":1234.1234,"readRatioMax":1234.1234)"
      R"(,"recordsRead":"1234")"
      R"(,"recordsWritten":"1234","shuffleOutputBytes":"1234")"
      R"(,"shuffleOutputBytesSpilled":"1234","slotMs":"10")"
      R"(,"startMs":"10")"
      R"(,"status":"explain-status","steps":[{"kind":"sub-step-kind")"
      R"(,"substeps":["sub-step-1"]}],"waitMsAvg":"10")"
      R"(,"waitMsMax":"10")"
      R"(,"waitRatioAvg":1234.1234,"waitRatioMax":1234.1234)"
      R"(,"writeMsAvg":"10","writeMsMax":"10")"
      R"(,"writeRatioAvg":1234.1234,"writeRatioMax":1234.1234}])"
      R"(,"referencedRoutines":[{"datasetId":"1","projectId":"2")"
      R"(,"routineId":"3"}],"referencedTables":[{)"
      R"("datasetId":"1","projectId":"2","tableId":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]})"
      R"(,"collation":"")"
      R"(,"defaultValueExpression":"","description":"","fields":{)"
      R"("fields":[]},"maxLength":0)"
      R"(,"mode":"fmode","name":"fname-1","policyTags":{)"
      R"("names":[]},"precision":0,"rangeElementType":{"type":""})"
      R"(,"roundingMode":"","scale":0,"type":""}]})"
      R"(,"searchStatistics":{"indexUnusedReasons":[{"baseTable":{)"
      R"("datasetId":"1","projectId":"2","tableId":"3"},"code":)"
      R"("BASE_TABLE_TOO_SMALL","indexName":"test-index")"
      R"(,"message":""}],"indexUsageMode":"PARTIALLY_USED"})"
      R"(,"statementType":"statement_type","timeline":[{"activeUnits":"1234")"
      R"(,"completedUnits":"1234","elapsedMs":"10","estimatedRunnableUnits":"1234")"
      R"(,"pendingUnits":"1234","totalSlotMs":"10"}],"totalBytesBilled":"1234")"
      R"(,"totalBytesProcessed":"1234")"
      R"(,"totalBytesProcessedAccuracy":"total_bytes_processed_accuracy")"
      R"(,"totalPartitionsProcessed":"1234","totalSlotMs":"10")"
      R"(,"transferredBytes":"1234","undeclaredQueryParameters":[{)"
      R"("name":"query-parameter-name","parameterType":{"arrayType":{)"
      R"("structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"structTypes":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"structTypes":[])"
      R"(,"type":"qp-struct-type"}}],"type":"query-parameter-type"})"
      R"(,"parameterValue":{"arrayValues":[{"arrayValues":[{)"
      R"("arrayValues":[],"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}],"structValues":{)"
      R"("qp-map-key":{"arrayValues":[],"structValues":{})"
      R"(,"value":"qp-map-value"}},"value":"query-parameter-value"}}]})";

  auto expected_json = nlohmann::json::parse(expected_text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto expected = MakeJobQueryStats();

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);

  JobQueryStatistics actual;
  from_json(actual_json, actual);

  bigquery_v2_minimal_testing::AssertEquals(expected, actual);
}

TEST(JobQueryStatsTest, DebugString) {
  auto stats = bigquery_v2_minimal_testing::MakeJobQueryStats();

  EXPECT_EQ(
      stats.DebugString("JobQueryStatistics", TracingOptions{}),
      R"(JobQueryStatistics { estimated_bytes_processed: 1234)"
      R"( total_partitions_processed: 1234 total_bytes_processed: 1234)"
      R"( total_bytes_billed: 1234 billing_tier: 1234 num_dml_affected_rows: 1234)"
      R"( ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_bytes_processed_accuracy")"
      R"( statement_type: "statement_type")"
      R"( ddl_operation_performed: "ddl_operation_performed")"
      R"( total_slot_time { "10ms" } cache_hit: true)"
      R"( query_plan { name: "test-explain" status: "explain-status" id: 1234)"
      R"( shuffle_output_bytes: 1234 shuffle_output_bytes_spilled: 1234)"
      R"( records_read: 1234 records_written: 1234 parallel_inputs: 1234)"
      R"( completed_parallel_inputs: 1234)"
      R"( start_time { "10ms" } end_time { "10ms" } slot_time { "10ms" })"
      R"( wait_avg_time_spent { "10ms" } wait_max_time_spent { "10ms" })"
      R"( read_avg_time_spent { "10ms" } read_max_time_spent { "10ms" })"
      R"( write_avg_time_spent { "10ms" } write_max_time_spent { "10ms" })"
      R"( compute_avg_time_spent { "10ms" } compute_max_time_spent { "10ms" })"
      R"( wait_ratio_avg: 1234.12 wait_ratio_max: 1234.12 read_ratio_avg: 1234.12)"
      R"( read_ratio_max: 1234.12 compute_ratio_avg: 1234.12 compute_ratio_max: 1234.12)"
      R"( write_ratio_avg: 1234.12 write_ratio_max: 1234.12)"
      R"( steps { kind: "sub-step-kind" substeps: "sub-step-1" })"
      R"( compute_mode { value: "BIGQUERY" })"
      R"( })"
      R"( timeline { elapsed_time { "10ms" } total_slot_time { "10ms" })"
      R"( pending_units: 1234 completed_units: 1234 active_units: 1234 estimated_runnable_units: 1234)"
      R"( })"
      R"( referenced_tables { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( referenced_routines { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode" description: "")"
      R"( collation: "" default_value_expression: "" max_length: 0 precision: 0)"
      R"( scale: 0 categories { } policy_tags { })"
      R"( rounding_mode { value: "" })"
      R"( range_element_type { type: "" } } })"
      R"( dml_stats {)"
      R"( inserted_row_count: 1234 deleted_row_count: 1234 updated_row_count: 1234 })"
      R"( ddl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( ddl_target_row_access_policy {)"
      R"( project_id: "1234" dataset_id: "1" table_id: "2" policy_id: "3" })"
      R"( ddl_target_routine { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( dcl_target_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_view { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( dcl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( search_statistics { index_unused_reasons {)"
      R"( message: "" index_name: "test-index" base_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( code { value: "BASE_TABLE_TOO_SMALL" } })"
      R"( index_usage_mode { value: "PARTIALLY_USED" } })"
      R"( performance_insights { avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights {)"
      R"( stage_id: 1234 slot_contention: true insufficient_shuffle_quota: true })"
      R"( stage_performance_change_insights { stage_id: 1234 input_data_change {)"
      R"( records_read_diff_percentage: 12.12 } } } materialized_view_statistics {)"
      R"( materialized_view { chosen: true estimated_bytes_saved: 1234 rejected_reason {)"
      R"( value: "BASE_TABLE_DATA_CHANGE" } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table-metadata" unused_reason { value: "EXCEEDED_MAX_STALENESS" })"
      R"( table_reference { project_id: "2" dataset_id: "1" table_id: "3" } } } })");

  EXPECT_EQ(
      stats.DebugString(
          "JobQueryStatistics",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(JobQueryStatistics { estimated_bytes_processed: 1234)"
      R"( total_partitions_processed: 1234 total_bytes_processed: 1234)"
      R"( total_bytes_billed: 1234 billing_tier: 1234)"
      R"( num_dml_affected_rows: 1234)"
      R"( ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_b...<truncated>...")"
      R"( statement_type: "stateme...<truncated>...")"
      R"( ddl_operation_performed: "ddl_ope...<truncated>...")"
      R"( total_slot_time { "10ms" } cache_hit: true query_plan {)"
      R"( name: "test-ex...<truncated>..." status: "explain...<truncated>...")"
      R"( id: 1234 shuffle_output_bytes: 1234)"
      R"( shuffle_output_bytes_spilled: 1234 records_read: 1234)"
      R"( records_written: 1234 parallel_inputs: 1234)"
      R"( completed_parallel_inputs: 1234 start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" })"
      R"( wait_avg_time_spent { "10ms" } wait_max_time_spent { "10ms" })"
      R"( read_avg_time_spent { "10ms" } read_max_time_spent { "10ms" })"
      R"( write_avg_time_spent { "10ms" } write_max_time_spent { "10ms" })"
      R"( compute_avg_time_spent { "10ms" } compute_max_time_spent { "10ms" })"
      R"( wait_ratio_avg: 1234.12 wait_ratio_max: 1234.12)"
      R"( read_ratio_avg: 1234.12 read_ratio_max: 1234.12)"
      R"( compute_ratio_avg: 1234.12 compute_ratio_max: 1234.12)"
      R"( write_ratio_avg: 1234.12 write_ratio_max: 1234.12)"
      R"( steps { kind: "sub-ste...<truncated>...")"
      R"( substeps: "sub-ste...<truncated>..." })"
      R"( compute_mode { value: "BIGQUER...<truncated>..." } })"
      R"( timeline { elapsed_time { "10ms" } total_slot_time { "10ms" })"
      R"( pending_units: 1234 completed_units: 1234 active_units: 1234)"
      R"( estimated_runnable_units: 1234 })"
      R"( referenced_tables { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( referenced_routines { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode" description: "")"
      R"( collation: "" default_value_expression: "" max_length: 0 precision: 0)"
      R"( scale: 0 categories { } policy_tags { })"
      R"( rounding_mode { value: "" })"
      R"( range_element_type { type: "" } } })"
      R"( dml_stats {)"
      R"( inserted_row_count: 1234 deleted_row_count: 1234 updated_row_count: 1234 })"
      R"( ddl_target_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } ddl_target_row_access_policy {)"
      R"( project_id: "1234" dataset_id: "1" table_id: "2" policy_id: "3" })"
      R"( ddl_target_routine { project_id: "2" dataset_id: "1" routine_id: "3" })"
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" } dcl_target_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } dcl_target_view {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } dcl_target_dataset {)"
      R"( project_id: "2" dataset_id: "1" } search_statistics {)"
      R"( index_unused_reasons { message: "" index_name: "test-in...<truncated>...")"
      R"( base_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( code { value: "BASE_TA...<truncated>..." } })"
      R"( index_usage_mode { value: "PARTIAL...<truncated>..." } })"
      R"( performance_insights { avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234 slot_contention: true)"
      R"( insufficient_shuffle_quota: true } stage_performance_change_insights {)"
      R"( stage_id: 1234 input_data_change { records_read_diff_percentage: 12.12 } } })"
      R"( materialized_view_statistics { materialized_view {)"
      R"( chosen: true estimated_bytes_saved: 1234 rejected_reason {)"
      R"( value: "BASE_TA...<truncated>..." } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-ta...<truncated>...")"
      R"( unused_reason { value: "EXCEEDE...<truncated>..." })"
      R"( table_reference { project_id: "2" dataset_id: "1" table_id: "3" } } } })");

  EXPECT_EQ(stats.DebugString("JobQueryStatistics", TracingOptions{}.SetOptions(
                                                        "single_line_mode=F")),
            R"(JobQueryStatistics {
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
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
