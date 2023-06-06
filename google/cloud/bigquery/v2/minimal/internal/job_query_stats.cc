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

#include "google/cloud/bigquery/v2/minimal/internal/job_query_stats.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// ComputeMode Enum Codes.
ComputeMode ComputeMode::UnSpecified() {
  return ComputeMode{"COMPUTE_MODE_UNSPECIFIED"};
}
ComputeMode ComputeMode::BigQuery() { return ComputeMode{"BIGQUERY"}; }
ComputeMode ComputeMode::BIEngine() { return ComputeMode{"BI_ENGINE"}; }

// IndexUsageMode Enum Codes.
IndexUsageMode IndexUsageMode::UnSpecified() {
  return IndexUsageMode{"INDEX_USAGE_MODE_UNSPECIFIED"};
}
IndexUsageMode IndexUsageMode::Unused() { return IndexUsageMode{"UNUSED"}; }
IndexUsageMode IndexUsageMode::PartiallyUsed() {
  return IndexUsageMode{"PARTIALLY_USED"};
}
IndexUsageMode IndexUsageMode::FullyUsed() {
  return IndexUsageMode{"FULLY_USED"};
}

// IndexedUnusedReasonCode Enum Codes.
IndexedUnusedReasonCode IndexedUnusedReasonCode::UnSpecified() {
  return IndexedUnusedReasonCode{"CODE_UNSPECIFIED"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::IndexConfigNotAvailable() {
  return IndexedUnusedReasonCode{"INDEX_CONFIG_NOT_AVAILABLE"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::PendingIndexCreation() {
  return IndexedUnusedReasonCode{"PENDING_INDEX_CREATION"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::BaseTableTruncated() {
  return IndexedUnusedReasonCode{"BASE_TABLE_TRUNCATED"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::IndexConfigModified() {
  return IndexedUnusedReasonCode{"INDEX_CONFIG_MODIFIED"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::TimeTravelQuery() {
  return IndexedUnusedReasonCode{"TIME_TRAVEL_QUERY"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::NoPruningPower() {
  return IndexedUnusedReasonCode{"NO_PRUNING_POWER"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::UnIndexedSearchFields() {
  return IndexedUnusedReasonCode{"UNINDEXED_SEARCH_FIELDS"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::UnSupportedSearchPattern() {
  return IndexedUnusedReasonCode{"UNSUPPORTED_SEARCH_PATTERN"};
}
IndexedUnusedReasonCode
IndexedUnusedReasonCode::OptimizedWithMaterializedView() {
  return IndexedUnusedReasonCode{"OPTIMIZED_WITH_MATERIALIZED_VIEW"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::SecuredByDataMasking() {
  return IndexedUnusedReasonCode{"SECURED_BY_DATA_MASKING"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::MismatchedTextAnalyzer() {
  return IndexedUnusedReasonCode{"MISMATCHED_TEXT_ANALYZER"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::BaseTableTooSmall() {
  return IndexedUnusedReasonCode{"BASE_TABLE_TOO_SMALL"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::BaseTableTooLarge() {
  return IndexedUnusedReasonCode{"BASE_TABLE_TOO_LARGE"};
}
IndexedUnusedReasonCode
IndexedUnusedReasonCode::EstimatedPerformanceGainTooLow() {
  return IndexedUnusedReasonCode{"ESTIMATED_PERFORMANCE_GAIN_TOO_LOW"};
}
IndexedUnusedReasonCode
IndexedUnusedReasonCode::NotSupportedInStandardEdition() {
  return IndexedUnusedReasonCode{"NOT_SUPPORTED_IN_STANDARD_EDITION"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::InternalError() {
  return IndexedUnusedReasonCode{"INTERNAL_ERROR"};
}
IndexedUnusedReasonCode IndexedUnusedReasonCode::OtherReason() {
  return IndexedUnusedReasonCode{"OTHER_REASON"};
}

// RejectedReason Enum Codes.
RejectedReason RejectedReason::UnSpecified() {
  return RejectedReason{"REJECTED_REASON_UNSPECIFIED"};
}
RejectedReason RejectedReason::NoData() { return RejectedReason{"NO_DATA"}; }
RejectedReason RejectedReason::Cost() { return RejectedReason{"COST"}; }
RejectedReason RejectedReason::BaseTableTruncated() {
  return RejectedReason{"BASE_TABLE_TRUNCATED"};
}
RejectedReason RejectedReason::BaseTableDataChange() {
  return RejectedReason{"BASE_TABLE_DATA_CHANGE"};
}
RejectedReason RejectedReason::BaseTablePartitionExpirationChange() {
  return RejectedReason{"BASE_TABLE_PARTITION_EXPIRATION_CHANGE"};
}
RejectedReason RejectedReason::BaseTableExpiredPartition() {
  return RejectedReason{"BASE_TABLE_EXPIRED_PARTITION"};
}
RejectedReason RejectedReason::BaseTableIncompatibleMetadataChange() {
  return RejectedReason{"BASE_TABLE_INCOMPATIBLE_METADATA_CHANGE"};
}
RejectedReason RejectedReason::TimeZone() {
  return RejectedReason{"TIME_ZONE"};
}
RejectedReason RejectedReason::OutOfTimeTravelWindow() {
  return RejectedReason{"OUT_OF_TIME_TRAVEL_WINDOW"};
}

// MetadataCacheUnusedReason Enum Codes.
MetadataCacheUnusedReason MetadataCacheUnusedReason::UnSpecified() {
  return MetadataCacheUnusedReason{"UNUSED_REASON_UNSPECIFIED"};
}
MetadataCacheUnusedReason MetadataCacheUnusedReason::ExceededMaxStaleness() {
  return MetadataCacheUnusedReason{"EXCEEDED_MAX_STALENESS"};
}
MetadataCacheUnusedReason MetadataCacheUnusedReason::OtherReason() {
  return MetadataCacheUnusedReason{"OTHER_REASON"};
}

void to_json(nlohmann::json& j, ExplainQueryStage const& q) {
  j = nlohmann::json{
      {"name", q.name},
      {"status", q.status},
      {"id", q.id},
      {"shuffle_output_bytes", q.shuffle_output_bytes},
      {"shuffle_output_bytes_spilled", q.shuffle_output_bytes_spilled},
      {"records_read", q.records_read},
      {"records_written", q.records_written},
      {"parallel_inputs", q.parallel_inputs},
      {"completed_parallel_inputs", q.completed_parallel_inputs},
      {"input_stages", q.input_stages},
      {"start_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(q.start_time)
           .count()},
      {"end_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(q.end_time)
           .count()},
      {"slot_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(q.slot_time)
           .count()},
      {"wait_avg_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.wait_avg_time_spent)
           .count()},
      {"wait_max_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.wait_max_time_spent)
           .count()},
      {"read_avg_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.read_avg_time_spent)
           .count()},
      {"read_max_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.read_max_time_spent)
           .count()},
      {"write_avg_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.write_avg_time_spent)
           .count()},
      {"write_max_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.write_max_time_spent)
           .count()},
      {"compute_avg_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.compute_avg_time_spent)
           .count()},
      {"compute_max_time_spent",
       std::chrono::duration_cast<std::chrono::milliseconds>(
           q.compute_max_time_spent)
           .count()},
      {"wait_ratio_avg", q.wait_ratio_avg},
      {"wait_ratio_max", q.wait_ratio_max},
      {"read_ratio_avg", q.read_ratio_avg},
      {"read_ratio_max", q.read_ratio_max},
      {"compute_ratio_avg", q.compute_ratio_avg},
      {"compute_ratio_max", q.compute_ratio_max},
      {"write_ratio_avg", q.write_ratio_avg},
      {"write_ratio_max", q.write_ratio_max},

      {"steps", q.steps},
      {"compute_mode", q.compute_mode}};
}

void from_json(nlohmann::json const& j, ExplainQueryStage& q) {
  if (j.contains("name")) j.at("name").get_to(q.name);
  if (j.contains("status")) j.at("status").get_to(q.status);
  if (j.contains("id")) j.at("id").get_to(q.id);
  if (j.contains("shuffle_output_bytes"))
    j.at("shuffle_output_bytes").get_to(q.shuffle_output_bytes);
  if (j.contains("shuffle_output_bytes_spilled"))
    j.at("shuffle_output_bytes_spilled").get_to(q.shuffle_output_bytes_spilled);
  if (j.contains("records_read")) j.at("records_read").get_to(q.records_read);
  if (j.contains("records_written"))
    j.at("records_written").get_to(q.records_written);
  if (j.contains("parallel_inputs"))
    j.at("parallel_inputs").get_to(q.parallel_inputs);
  if (j.contains("completed_parallel_inputs"))
    j.at("completed_parallel_inputs").get_to(q.completed_parallel_inputs);
  if (j.contains("input_stages")) j.at("input_stages").get_to(q.input_stages);
  if (j.contains("start_time")) {
    std::int64_t millis;
    j.at("start_time").get_to(millis);
    q.start_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("end_time")) {
    std::int64_t millis;
    j.at("end_time").get_to(millis);
    q.end_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("slot_time")) {
    std::int64_t millis;
    j.at("slot_time").get_to(millis);
    q.slot_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("wait_avg_time_spent")) {
    std::int64_t millis;
    j.at("wait_avg_time_spent").get_to(millis);
    q.wait_avg_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("wait_max_time_spent")) {
    std::int64_t millis;
    j.at("wait_max_time_spent").get_to(millis);
    q.wait_max_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("read_avg_time_spent")) {
    std::int64_t millis;
    j.at("read_avg_time_spent").get_to(millis);
    q.read_avg_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("read_max_time_spent")) {
    std::int64_t millis;
    j.at("read_max_time_spent").get_to(millis);
    q.read_max_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("write_avg_time_spent")) {
    std::int64_t millis;
    j.at("write_avg_time_spent").get_to(millis);
    q.write_avg_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("write_max_time_spent")) {
    std::int64_t millis;
    j.at("write_max_time_spent").get_to(millis);
    q.write_max_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("compute_avg_time_spent")) {
    std::int64_t millis;
    j.at("compute_avg_time_spent").get_to(millis);
    q.compute_avg_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("compute_max_time_spent")) {
    std::int64_t millis;
    j.at("compute_max_time_spent").get_to(millis);
    q.compute_max_time_spent = std::chrono::milliseconds(millis);
  }
  if (j.contains("wait_ratio_avg"))
    j.at("wait_ratio_avg").get_to(q.wait_ratio_avg);
  if (j.contains("wait_ratio_max"))
    j.at("wait_ratio_max").get_to(q.wait_ratio_max);
  if (j.contains("read_ratio_avg"))
    j.at("read_ratio_avg").get_to(q.read_ratio_avg);
  if (j.contains("read_ratio_max"))
    j.at("read_ratio_max").get_to(q.read_ratio_max);
  if (j.contains("compute_ratio_avg"))
    j.at("compute_ratio_avg").get_to(q.compute_ratio_avg);
  if (j.contains("compute_ratio_max"))
    j.at("compute_ratio_max").get_to(q.compute_ratio_max);
  if (j.contains("write_ratio_avg"))
    j.at("write_ratio_avg").get_to(q.write_ratio_avg);
  if (j.contains("write_ratio_max"))
    j.at("write_ratio_max").get_to(q.write_ratio_max);
  if (j.contains("steps")) j.at("steps").get_to(q.steps);
  if (j.contains("compute_mode")) j.at("compute_mode").get_to(q.compute_mode);
}

void to_json(nlohmann::json& j, QueryTimelineSample const& q) {
  j = nlohmann::json{
      {"elapsed_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(q.elapsed_time)
           .count()},
      {"total_slot_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(q.total_slot_time)
           .count()},
      {"pending_units", q.pending_units},
      {"completed_units", q.completed_units},
      {"active_units", q.active_units},
      {"estimated_runnable_units", q.estimated_runnable_units}};
}

void from_json(nlohmann::json const& j, QueryTimelineSample& q) {
  if (j.contains("elapsed_time")) {
    std::int64_t millis;
    j.at("elapsed_time").get_to(millis);
    q.elapsed_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("total_slot_time")) {
    std::int64_t millis;
    j.at("total_slot_time").get_to(millis);
    q.total_slot_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("pending_units"))
    j.at("pending_units").get_to(q.pending_units);
  if (j.contains("completed_units"))
    j.at("completed_units").get_to(q.completed_units);
  if (j.contains("active_units")) j.at("active_units").get_to(q.active_units);
  if (j.contains("estimated_runnable_units"))
    j.at("estimated_runnable_units").get_to(q.estimated_runnable_units);
}

void to_json(nlohmann::json& j, PerformanceInsights const& p) {
  j = nlohmann::json{{"avg_previous_execution_time",
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          p.avg_previous_execution_time)
                          .count()},
                     {"stage_performance_standalone_insights",
                      p.stage_performance_standalone_insights},
                     {"stage_performance_change_insights",
                      p.stage_performance_change_insights}};
}

void from_json(nlohmann::json const& j, PerformanceInsights& p) {
  if (j.contains("avg_previous_execution_time")) {
    std::int64_t millis;
    j.at("avg_previous_execution_time").get_to(millis);
    p.avg_previous_execution_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("stage_performance_standalone_insights"))
    j.at("stage_performance_standalone_insights")
        .get_to(p.stage_performance_standalone_insights);
  if (j.contains("stage_performance_change_insights"))
    j.at("stage_performance_change_insights")
        .get_to(p.stage_performance_change_insights);
}
void to_json(nlohmann::json& j, JobQueryStatistics const& q) {
  j = nlohmann::json{
      {"estimated_bytes_processed", q.estimated_bytes_processed},
      {"total_partitions_processed", q.total_partitions_processed},
      {"total_bytes_processed", q.total_bytes_processed},
      {"total_bytes_billed", q.total_bytes_billed},
      {"billing_tier", q.billing_tier},
      {"num_dml_affected_rows", q.num_dml_affected_rows},
      {"ddl_affected_row_access_policy_count",
       q.ddl_affected_row_access_policy_count},
      {"transferred_bytes", q.transferred_bytes},
      {"total_bytes_processed_accuracy", q.total_bytes_processed_accuracy},
      {"statement_type", q.statement_type},
      {"ddl_operation_performed", q.ddl_operation_performed},
      {"total_slot_time",
       std::chrono::duration_cast<std::chrono::milliseconds>(q.total_slot_time)
           .count()},
      {"cache_hit", q.cache_hit},
      {"query_plan", q.query_plan},
      {"timeline", q.timeline},
      {"referenced_tables", q.referenced_tables},
      {"referenced_routines", q.referenced_routines},
      {"undeclared_query_parameters", q.undeclared_query_parameters},
      {"schema", q.schema},
      {"dml_stats", q.dml_stats},
      {"ddl_target_table", q.ddl_target_table},
      {"ddl_destination_table", q.ddl_destination_table},
      {"ddl_target_row_access_policy", q.ddl_target_row_access_policy},
      {"ddl_target_routine", q.ddl_target_routine},
      {"ddl_target_dataset", q.ddl_target_dataset},
      {"dcl_target_table", q.dcl_target_table},
      {"dcl_target_view", q.dcl_target_view},
      {"dcl_target_dataset", q.dcl_target_dataset},
      {"search_statistics", q.search_statistics},
      {"performance_insights", q.performance_insights},
      {"materialized_view_statistics", q.materialized_view_statistics},
      {"metadata_cache_statistics", q.metadata_cache_statistics}};
}

void from_json(nlohmann::json const& j, JobQueryStatistics& q) {
  if (j.contains("estimated_bytes_processed"))
    j.at("estimated_bytes_processed").get_to(q.estimated_bytes_processed);
  if (j.contains("total_partitions_processed"))
    j.at("total_partitions_processed").get_to(q.total_partitions_processed);
  if (j.contains("total_bytes_processed"))
    j.at("total_bytes_processed").get_to(q.total_bytes_processed);
  if (j.contains("total_bytes_billed"))
    j.at("total_bytes_billed").get_to(q.total_bytes_billed);
  if (j.contains("billing_tier")) j.at("billing_tier").get_to(q.billing_tier);
  if (j.contains("num_dml_affected_rows"))
    j.at("num_dml_affected_rows").get_to(q.num_dml_affected_rows);
  if (j.contains("ddl_affected_row_access_policy_count"))
    j.at("ddl_affected_row_access_policy_count")
        .get_to(q.ddl_affected_row_access_policy_count);
  if (j.contains("transferred_bytes"))
    j.at("transferred_bytes").get_to(q.transferred_bytes);
  if (j.contains("total_bytes_processed_accuracy"))
    j.at("total_bytes_processed_accuracy")
        .get_to(q.total_bytes_processed_accuracy);
  if (j.contains("statement_type"))
    j.at("statement_type").get_to(q.statement_type);
  if (j.contains("ddl_operation_performed"))
    j.at("ddl_operation_performed").get_to(q.ddl_operation_performed);
  if (j.contains("total_slot_time")) {
    std::int64_t millis;
    j.at("total_slot_time").get_to(millis);
    q.total_slot_time = std::chrono::milliseconds(millis);
  }
  if (j.contains("cache_hit")) j.at("cache_hit").get_to(q.cache_hit);
  if (j.contains("query_plan")) j.at("query_plan").get_to(q.query_plan);
  if (j.contains("timeline")) j.at("timeline").get_to(q.timeline);
  if (j.contains("referenced_tables"))
    j.at("referenced_tables").get_to(q.referenced_tables);
  if (j.contains("referenced_routines"))
    j.at("referenced_routines").get_to(q.referenced_routines);
  if (j.contains("undeclared_query_parameters"))
    j.at("undeclared_query_parameters").get_to(q.undeclared_query_parameters);
  if (j.contains("schema")) j.at("schema").get_to(q.schema);
  if (j.contains("dml_stats")) j.at("dml_stats").get_to(q.dml_stats);
  if (j.contains("ddl_target_table"))
    j.at("ddl_target_table").get_to(q.ddl_target_table);
  if (j.contains("ddl_destination_table"))
    j.at("ddl_destination_table").get_to(q.ddl_destination_table);
  if (j.contains("ddl_target_row_access_policy"))
    j.at("ddl_target_row_access_policy").get_to(q.ddl_target_row_access_policy);
  if (j.contains("ddl_target_routine"))
    j.at("ddl_target_routine").get_to(q.ddl_target_routine);
  if (j.contains("ddl_target_dataset"))
    j.at("ddl_target_dataset").get_to(q.ddl_target_dataset);
  if (j.contains("dcl_target_table"))
    j.at("dcl_target_table").get_to(q.dcl_target_table);
  if (j.contains("dcl_target_view"))
    j.at("dcl_target_view").get_to(q.dcl_target_view);
  if (j.contains("dcl_target_dataset"))
    j.at("dcl_target_dataset").get_to(q.dcl_target_dataset);
  if (j.contains("search_statistics"))
    j.at("search_statistics").get_to(q.search_statistics);
  if (j.contains("performance_insights"))
    j.at("performance_insights").get_to(q.performance_insights);
  if (j.contains("materialized_view_statistics"))
    j.at("materialized_view_statistics").get_to(q.materialized_view_statistics);
  if (j.contains("metadata_cache_statistics"))
    j.at("metadata_cache_statistics").get_to(q.metadata_cache_statistics);
}

bool operator==(MaterializedView const& lhs, MaterializedView const& rhs) {
  return (lhs.chosen == rhs.chosen &&
          lhs.estimated_bytes_saved == rhs.estimated_bytes_saved &&
          lhs.rejected_reason.value == rhs.rejected_reason.value &&
          lhs.table_reference.dataset_id == rhs.table_reference.dataset_id &&
          lhs.table_reference.project_id == rhs.table_reference.project_id &&
          lhs.table_reference.table_id == rhs.table_reference.table_id);
}

bool operator==(TableMetadataCacheUsage const& lhs,
                TableMetadataCacheUsage const& rhs) {
  return (lhs.explanation == rhs.explanation &&
          lhs.unused_reason.value == rhs.unused_reason.value &&
          lhs.table_reference.dataset_id == rhs.table_reference.dataset_id &&
          lhs.table_reference.project_id == rhs.table_reference.project_id &&
          lhs.table_reference.table_id == rhs.table_reference.table_id);
}

bool operator==(QueryTimelineSample const& lhs,
                QueryTimelineSample const& rhs) {
  return (lhs.active_units == rhs.active_units &&
          lhs.completed_units == rhs.completed_units &&
          lhs.elapsed_time == rhs.elapsed_time &&
          lhs.estimated_runnable_units == rhs.estimated_runnable_units &&
          lhs.pending_units == rhs.pending_units &&
          lhs.total_slot_time == rhs.total_slot_time);
}

bool operator==(ExplainQueryStep const& lhs, ExplainQueryStep const& rhs) {
  auto eq = (lhs.kind == rhs.kind);
  return eq && std::equal(lhs.substeps.begin(), lhs.substeps.end(),
                          rhs.substeps.begin());
}

bool operator==(ExplainQueryStage const& lhs, ExplainQueryStage const& rhs) {
  auto eq =
      (lhs.name == rhs.name && lhs.status == rhs.status && lhs.id == rhs.id &&
       lhs.shuffle_output_bytes == rhs.shuffle_output_bytes &&
       lhs.shuffle_output_bytes_spilled == rhs.shuffle_output_bytes_spilled &&
       lhs.records_read == rhs.records_read &&
       lhs.records_written == rhs.records_written &&
       lhs.parallel_inputs == rhs.parallel_inputs &&
       lhs.completed_parallel_inputs == rhs.completed_parallel_inputs &&
       lhs.start_time == rhs.start_time && lhs.end_time == rhs.end_time &&
       lhs.slot_time == rhs.slot_time &&
       lhs.wait_avg_time_spent == rhs.wait_avg_time_spent &&
       lhs.wait_max_time_spent == rhs.wait_max_time_spent &&
       lhs.read_avg_time_spent == rhs.read_avg_time_spent &&
       lhs.read_max_time_spent == rhs.read_max_time_spent &&
       lhs.write_avg_time_spent == rhs.write_avg_time_spent &&
       lhs.write_max_time_spent == rhs.write_max_time_spent &&
       lhs.compute_avg_time_spent == rhs.compute_avg_time_spent &&
       lhs.compute_max_time_spent == rhs.compute_max_time_spent &&
       lhs.wait_ratio_avg == rhs.wait_ratio_avg &&
       lhs.wait_ratio_max == rhs.wait_ratio_max &&
       lhs.read_ratio_avg == rhs.read_ratio_avg &&
       lhs.read_ratio_max == rhs.read_ratio_max &&
       lhs.compute_ratio_avg == rhs.compute_ratio_avg &&
       lhs.write_ratio_avg == rhs.write_ratio_avg &&
       lhs.write_ratio_max == rhs.write_ratio_max &&
       lhs.compute_mode == rhs.compute_mode);

  auto steps_eq =
      std::equal(lhs.steps.begin(), lhs.steps.end(), rhs.steps.begin());
  auto stages_eq = std::equal(lhs.input_stages.begin(), lhs.input_stages.end(),
                              rhs.input_stages.begin());

  return eq && steps_eq && stages_eq;
}

bool operator==(PerformanceInsights const& lhs,
                PerformanceInsights const& rhs) {
  return (
      lhs.avg_previous_execution_time == rhs.avg_previous_execution_time &&
      lhs.stage_performance_standalone_insights.insufficient_shuffle_quota ==
          rhs.stage_performance_standalone_insights
              .insufficient_shuffle_quota &&
      lhs.stage_performance_standalone_insights.slot_contention ==
          rhs.stage_performance_standalone_insights.slot_contention &&
      lhs.stage_performance_standalone_insights.stage_id ==
          rhs.stage_performance_standalone_insights.stage_id &&
      lhs.stage_performance_change_insights.stage_id ==
          rhs.stage_performance_change_insights.stage_id &&
      lhs.stage_performance_change_insights.input_data_change
              .records_read_diff_percentage ==
          rhs.stage_performance_change_insights.input_data_change
              .records_read_diff_percentage);
}

bool operator==(IndexUnusedReason const& lhs, IndexUnusedReason const& rhs) {
  return lhs.base_table == rhs.base_table && lhs.code == rhs.code &&
         lhs.index_name == rhs.index_name && lhs.message == rhs.message;
}

bool operator==(SearchStatistics const& lhs, SearchStatistics const& rhs) {
  bool eq = lhs.index_usage_mode == rhs.index_usage_mode;
  return eq && std::equal(lhs.index_unused_reasons.begin(),
                          lhs.index_unused_reasons.end(),
                          rhs.index_unused_reasons.begin());
}

bool operator==(DmlStats const& lhs, DmlStats const& rhs) {
  return lhs.deleted_row_count == rhs.deleted_row_count &&
         lhs.inserted_row_count == rhs.inserted_row_count &&
         lhs.updated_row_count == rhs.updated_row_count;
}

bool operator==(RowAccessPolicyReference const& lhs,
                RowAccessPolicyReference const& rhs) {
  return lhs.dataset_id == rhs.dataset_id && lhs.project_id == rhs.project_id &&
         lhs.table_id == rhs.table_id && lhs.policy_id == rhs.policy_id;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
