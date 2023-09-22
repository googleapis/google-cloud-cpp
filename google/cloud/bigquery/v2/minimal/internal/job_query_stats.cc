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
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

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
      {"id", std::to_string(q.id)},
      {"shuffleOutputBytes", std::to_string(q.shuffle_output_bytes)},
      {"shuffleOutputBytesSpilled",
       std::to_string(q.shuffle_output_bytes_spilled)},
      {"recordsRead", std::to_string(q.records_read)},
      {"recordsWritten", std::to_string(q.records_written)},
      {"parallelInputs", std::to_string(q.parallel_inputs)},
      {"completedParallelInputs", std::to_string(q.completed_parallel_inputs)},
      {"inputStages", q.input_stages},
      {"waitRatioAvg", q.wait_ratio_avg},
      {"waitRatioMax", q.wait_ratio_max},
      {"readRatioAvg", q.read_ratio_avg},
      {"readRatioMax", q.read_ratio_max},
      {"computeRatioAvg", q.compute_ratio_avg},
      {"computeRatioMax", q.compute_ratio_max},
      {"writeRatioAvg", q.write_ratio_avg},
      {"writeRatioMax", q.write_ratio_max},
      {"steps", q.steps},
      {"computeMode", q.compute_mode.value}};

  ToJson(q.start_time, j, "startMs");
  ToJson(q.end_time, j, "endMs");
  ToJson(q.slot_time, j, "slotMs");
  ToJson(q.wait_avg_time_spent, j, "waitMsAvg");
  ToJson(q.wait_max_time_spent, j, "waitMsMax");
  ToJson(q.read_avg_time_spent, j, "readMsAvg");
  ToJson(q.read_max_time_spent, j, "readMsMax");
  ToJson(q.write_avg_time_spent, j, "writeMsAvg");
  ToJson(q.write_max_time_spent, j, "writeMsMax");
  ToJson(q.compute_avg_time_spent, j, "computeMsAvg");
  ToJson(q.compute_max_time_spent, j, "computeMsMax");
}

void from_json(nlohmann::json const& j, ExplainQueryStage& q) {
  SafeGetTo(q.name, j, "name");
  SafeGetTo(q.status, j, "status");
  q.id = GetNumberFromJson(j, "id");
  q.shuffle_output_bytes = GetNumberFromJson(j, "shuffleOutputBytes");
  q.shuffle_output_bytes_spilled =
      GetNumberFromJson(j, "shuffleOutputBytesSpilled");
  q.records_read = GetNumberFromJson(j, "recordsRead");
  q.records_written = GetNumberFromJson(j, "recordsWritten");
  q.parallel_inputs = GetNumberFromJson(j, "parallelInputs");
  q.completed_parallel_inputs = GetNumberFromJson(j, "completedParallelInputs");
  SafeGetTo(q.input_stages, j, "inputStages");
  SafeGetTo(q.wait_ratio_avg, j, "waitRatioAvg");
  SafeGetTo(q.wait_ratio_max, j, "waitRatioMax");
  SafeGetTo(q.read_ratio_avg, j, "readRatioAvg");
  SafeGetTo(q.read_ratio_max, j, "readRatioMax");
  SafeGetTo(q.compute_ratio_avg, j, "computeRatioAvg");
  SafeGetTo(q.compute_ratio_max, j, "computeRatioMax");
  SafeGetTo(q.write_ratio_avg, j, "writeRatioAvg");
  SafeGetTo(q.write_ratio_max, j, "writeRatioMax");
  SafeGetTo(q.steps, j, "steps");
  SafeGetTo(q.compute_mode.value, j, "computeMode");

  FromJson(q.start_time, j, "startMs");
  FromJson(q.end_time, j, "endMs");
  FromJson(q.slot_time, j, "slotMs");
  FromJson(q.wait_avg_time_spent, j, "waitMsAvg");
  FromJson(q.wait_max_time_spent, j, "waitMsMax");
  FromJson(q.read_avg_time_spent, j, "readMsAvg");
  FromJson(q.read_max_time_spent, j, "readMsMax");
  FromJson(q.write_avg_time_spent, j, "writeMsAvg");
  FromJson(q.write_max_time_spent, j, "writeMsMax");
  FromJson(q.compute_avg_time_spent, j, "computeMsAvg");
  FromJson(q.compute_max_time_spent, j, "computeMsMax");
}

void to_json(nlohmann::json& j, QueryTimelineSample const& q) {
  j = nlohmann::json{
      {"pendingUnits", std::to_string(q.pending_units)},
      {"completedUnits", std::to_string(q.completed_units)},
      {"activeUnits", std::to_string(q.active_units)},
      {"estimatedRunnableUnits", std::to_string(q.estimated_runnable_units)}};

  ToJson(q.elapsed_time, j, "elapsedMs");
  ToJson(q.total_slot_time, j, "totalSlotMs");
}

void from_json(nlohmann::json const& j, QueryTimelineSample& q) {
  q.pending_units = GetNumberFromJson(j, "pendingUnits");
  q.completed_units = GetNumberFromJson(j, "completedUnits");
  q.active_units = GetNumberFromJson(j, "activeUnits");
  q.estimated_runnable_units = GetNumberFromJson(j, "estimatedRunnableUnits");

  FromJson(q.elapsed_time, j, "elapsedMs");
  FromJson(q.total_slot_time, j, "totalSlotMs");
}

void to_json(nlohmann::json& j, PerformanceInsights const& p) {
  j = nlohmann::json{
      {"stagePerformanceStandaloneInsights",
       p.stage_performance_standalone_insights},
      {"stagePerformanceChangeInsights", p.stage_performance_change_insights}};

  ToJson(p.avg_previous_execution_time, j, "avgPreviousExecutionMs");
}

void from_json(nlohmann::json const& j, PerformanceInsights& p) {
  SafeGetTo(p.stage_performance_standalone_insights, j,
            "stagePerformanceStandaloneInsights");
  SafeGetTo(p.stage_performance_change_insights, j,
            "stagePerformanceChangeInsights");

  FromJson(p.avg_previous_execution_time, j, "avgPreviousExecutionMs");
}

void to_json(nlohmann::json& j, JobQueryStatistics const& q) {
  j = nlohmann::json{
      {"estimatedBytesProcessed", std::to_string(q.estimated_bytes_processed)},
      {"totalPartitionsProcessed",
       std::to_string(q.total_partitions_processed)},
      {"totalBytesProcessed", std::to_string(q.total_bytes_processed)},
      {"totalBytesBilled", std::to_string(q.total_bytes_billed)},
      {"billingTier", q.billing_tier},
      {"numDmlAffectedRows", std::to_string(q.num_dml_affected_rows)},
      {"ddlAffectedRowAccessPolicyCount",
       std::to_string(q.ddl_affected_row_access_policy_count)},
      {"transferredBytes", std::to_string(q.transferred_bytes)},
      {"totalBytesProcessedAccuracy", q.total_bytes_processed_accuracy},
      {"statementType", q.statement_type},
      {"ddlOperationPerformed", q.ddl_operation_performed},
      {"cacheHit", q.cache_hit},
      {"queryPlan", q.query_plan},
      {"timeline", q.timeline},
      {"referencedTables", q.referenced_tables},
      {"referencedRoutines", q.referenced_routines},
      {"undeclaredQueryParameters", q.undeclared_query_parameters},
      {"schema", q.schema},
      {"dmlStats", q.dml_stats},
      {"ddlTargetTable", q.ddl_target_table},
      {"ddlTargetRowAccessPolicy", q.ddl_target_row_access_policy},
      {"ddlTargetRoutine", q.ddl_target_routine},
      {"ddlTargetDataset", q.ddl_target_dataset},
      {"dclTargetTable", q.dcl_target_table},
      {"dclTargetView", q.dcl_target_view},
      {"dclTargetDataset", q.dcl_target_dataset},
      {"searchStatistics", q.search_statistics},
      {"performanceInsights", q.performance_insights},
      {"materializedViewStatistics", q.materialized_view_statistics},
      {"metadataCacheStatistics", q.metadata_cache_statistics}};

  ToJson(q.total_slot_time, j, "totalSlotMs");
}

void from_json(nlohmann::json const& j, JobQueryStatistics& q) {
  q.estimated_bytes_processed = GetNumberFromJson(j, "estimatedBytesProcessed");
  q.total_partitions_processed =
      GetNumberFromJson(j, "totalPartitionsProcessed");
  q.total_bytes_processed = GetNumberFromJson(j, "totalBytesProcessed");
  q.total_bytes_billed = GetNumberFromJson(j, "totalBytesBilled");
  SafeGetTo(q.billing_tier, j, "billingTier");
  q.num_dml_affected_rows = GetNumberFromJson(j, "numDmlAffectedRows");
  q.ddl_affected_row_access_policy_count =
      GetNumberFromJson(j, "ddlAffectedRowAccessPolicyCount");
  q.transferred_bytes = GetNumberFromJson(j, "transferredBytes");
  SafeGetTo(q.total_bytes_processed_accuracy, j, "totalBytesProcessedAccuracy");
  SafeGetTo(q.statement_type, j, "statementType");
  SafeGetTo(q.ddl_operation_performed, j, "ddlOperationPerformed");
  SafeGetTo(q.cache_hit, j, "cacheHit");
  SafeGetTo(q.query_plan, j, "queryPlan");
  SafeGetTo(q.timeline, j, "timeline");
  SafeGetTo(q.referenced_tables, j, "referencedTables");
  SafeGetTo(q.referenced_routines, j, "referencedRoutines");
  SafeGetTo(q.undeclared_query_parameters, j, "undeclaredQueryParameters");
  SafeGetTo(q.schema, j, "schema");
  SafeGetTo(q.dml_stats, j, "dmlStats");
  SafeGetTo(q.ddl_target_table, j, "ddlTargetTable");
  SafeGetTo(q.ddl_target_row_access_policy, j, "ddlTargetRowAccessPolicy");
  SafeGetTo(q.ddl_target_routine, j, "ddlTargetRoutine");
  SafeGetTo(q.ddl_target_dataset, j, "ddlTargetDataset");
  SafeGetTo(q.dcl_target_table, j, "dclTargetTable");
  SafeGetTo(q.dcl_target_view, j, "dclTargetView");
  SafeGetTo(q.dcl_target_dataset, j, "dclTargetDataset");
  SafeGetTo(q.search_statistics, j, "searchStatistics");
  SafeGetTo(q.performance_insights, j, "performanceInsights");
  SafeGetTo(q.materialized_view_statistics, j, "materializedViewStatistics");
  SafeGetTo(q.metadata_cache_statistics, j, "metadataCacheStatistics");

  FromJson(q.total_slot_time, j, "totalSlotMs");
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
  return eq && std::equal(lhs.sub_steps.begin(), lhs.sub_steps.end(),
                          rhs.sub_steps.begin());
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

std::string ExplainQueryStep::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .Field("substeps", sub_steps)
      .Build();
}

std::string ComputeMode::DebugString(absl::string_view name,
                                     TracingOptions const& options,
                                     int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string ExplainQueryStage::DebugString(absl::string_view eq_name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(eq_name, options, indent)
      .StringField("name", name)
      .StringField("status", status)
      .Field("id", id)
      .Field("shuffle_output_bytes", shuffle_output_bytes)
      .Field("shuffle_output_bytes_spilled", shuffle_output_bytes_spilled)
      .Field("records_read", records_read)
      .Field("records_written", records_written)
      .Field("parallel_inputs", parallel_inputs)
      .Field("completed_parallel_inputs", completed_parallel_inputs)
      .Field("start_time", start_time)
      .Field("end_time", end_time)
      .Field("slot_time", slot_time)
      .Field("wait_avg_time_spent", wait_avg_time_spent)
      .Field("wait_max_time_spent", wait_max_time_spent)
      .Field("read_avg_time_spent", read_avg_time_spent)
      .Field("read_max_time_spent", read_max_time_spent)
      .Field("write_avg_time_spent", write_avg_time_spent)
      .Field("write_max_time_spent", write_max_time_spent)
      .Field("compute_avg_time_spent", compute_avg_time_spent)
      .Field("compute_max_time_spent", compute_max_time_spent)
      .Field("wait_ratio_avg", wait_ratio_avg)
      .Field("wait_ratio_max", wait_ratio_max)
      .Field("read_ratio_avg", read_ratio_avg)
      .Field("read_ratio_max", read_ratio_max)
      .Field("compute_ratio_avg", compute_ratio_avg)
      .Field("compute_ratio_max", compute_ratio_max)
      .Field("write_ratio_avg", write_ratio_avg)
      .Field("write_ratio_max", write_ratio_max)
      .Field("steps", steps)
      .SubMessage("compute_mode", compute_mode)
      .Build();
}

std::string QueryTimelineSample::DebugString(absl::string_view name,
                                             TracingOptions const& options,
                                             int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("elapsed_time", elapsed_time)
      .Field("total_slot_time", total_slot_time)
      .Field("pending_units", pending_units)
      .Field("completed_units", completed_units)
      .Field("active_units", active_units)
      .Field("estimated_runnable_units", estimated_runnable_units)
      .Build();
}

std::string DmlStats::DebugString(absl::string_view name,
                                  TracingOptions const& options,
                                  int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("inserted_row_count", inserted_row_count)
      .Field("deleted_row_count", deleted_row_count)
      .Field("updated_row_count", updated_row_count)
      .Build();
}

std::string RowAccessPolicyReference::DebugString(absl::string_view name,
                                                  TracingOptions const& options,
                                                  int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id)
      .StringField("dataset_id", dataset_id)
      .StringField("table_id", table_id)
      .StringField("policy_id", policy_id)
      .Build();
}

std::string IndexUsageMode::DebugString(absl::string_view name,
                                        TracingOptions const& options,
                                        int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string IndexedUnusedReasonCode::DebugString(absl::string_view name,
                                                 TracingOptions const& options,
                                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string IndexUnusedReason::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("message", message)
      .StringField("index_name", index_name)
      .SubMessage("base_table", base_table)
      .SubMessage("code", code)
      .Build();
}

std::string SearchStatistics::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("index_unused_reasons", index_unused_reasons)
      .SubMessage("index_usage_mode", index_usage_mode)
      .Build();
}

std::string InputDataChange::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("records_read_diff_percentage",
             static_cast<double>(records_read_diff_percentage))
      .Build();
}

std::string StagePerformanceChangeInsight::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("stage_id", stage_id)
      .SubMessage("input_data_change", input_data_change)
      .Build();
}

std::string StagePerformanceStandaloneInsight::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("stage_id", stage_id)
      .Field("slot_contention", slot_contention)
      .Field("insufficient_shuffle_quota", insufficient_shuffle_quota)
      .Build();
}

std::string PerformanceInsights::DebugString(absl::string_view name,
                                             TracingOptions const& options,
                                             int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("avg_previous_execution_time", avg_previous_execution_time)
      .SubMessage("stage_performance_standalone_insights",
                  stage_performance_standalone_insights)
      .SubMessage("stage_performance_change_insights",
                  stage_performance_change_insights)
      .Build();
}

std::string RejectedReason::DebugString(absl::string_view name,
                                        TracingOptions const& options,
                                        int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string MaterializedView::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("chosen", chosen)
      .Field("estimated_bytes_saved", estimated_bytes_saved)
      .SubMessage("rejected_reason", rejected_reason)
      .SubMessage("table_reference", table_reference)
      .Build();
}

std::string MaterializedViewStatistics::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("materialized_view", materialized_view)
      .Build();
}

std::string MetadataCacheUnusedReason::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string TableMetadataCacheUsage::DebugString(absl::string_view name,
                                                 TracingOptions const& options,
                                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("explanation", explanation)
      .SubMessage("unused_reason", unused_reason)
      .SubMessage("table_reference", table_reference)
      .Build();
}

std::string MetadataCacheStatistics::DebugString(absl::string_view name,
                                                 TracingOptions const& options,
                                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("table_metadata_cache_usage", table_metadata_cache_usage)
      .Build();
}

std::string JobQueryStatistics::DebugString(absl::string_view name,
                                            TracingOptions const& options,
                                            int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("estimated_bytes_processed", estimated_bytes_processed)
      .Field("total_partitions_processed", total_partitions_processed)
      .Field("total_bytes_processed", total_bytes_processed)
      .Field("total_bytes_billed", total_bytes_billed)
      .Field("billing_tier", billing_tier)
      .Field("num_dml_affected_rows", num_dml_affected_rows)
      .Field("ddl_affected_row_access_policy_count",
             ddl_affected_row_access_policy_count)
      .StringField("total_bytes_processed_accuracy",
                   total_bytes_processed_accuracy)
      .StringField("statement_type", statement_type)
      .StringField("ddl_operation_performed", ddl_operation_performed)
      .Field("total_slot_time", total_slot_time)
      .Field("cache_hit", cache_hit)
      .Field("query_plan", query_plan)
      .Field("timeline", timeline)
      .Field("referenced_tables", referenced_tables)
      .Field("referenced_routines", referenced_routines)
      .SubMessage("schema", schema)
      .SubMessage("dml_stats", dml_stats)
      .SubMessage("ddl_target_table", ddl_target_table)
      .SubMessage("ddl_target_row_access_policy", ddl_target_row_access_policy)
      .SubMessage("ddl_target_routine", ddl_target_routine)
      .SubMessage("ddl_target_dataset", ddl_target_dataset)
      .SubMessage("dcl_target_table", dcl_target_table)
      .SubMessage("dcl_target_view", dcl_target_view)
      .SubMessage("dcl_target_dataset", dcl_target_dataset)
      .SubMessage("search_statistics", search_statistics)
      .SubMessage("performance_insights", performance_insights)
      .SubMessage("materialized_view_statistics", materialized_view_statistics)
      .SubMessage("metadata_cache_statistics", metadata_cache_statistics)
      .Build();
}

void to_json(nlohmann::json& j, MetadataCacheStatistics const& m) {
  j = nlohmann::json{{"tableMetadataCacheUsage", m.table_metadata_cache_usage}};
}
void from_json(nlohmann::json const& j, MetadataCacheStatistics& m) {
  SafeGetTo(m.table_metadata_cache_usage, j, "tableMetadataCacheUsage");
}

void to_json(nlohmann::json& j, TableMetadataCacheUsage const& t) {
  j = nlohmann::json{{"explanation", t.explanation},
                     {"tableReference", t.table_reference},
                     {"unusedReason", t.unused_reason.value}};
}
void from_json(nlohmann::json const& j, TableMetadataCacheUsage& t) {
  SafeGetTo(t.explanation, j, "explanation");
  SafeGetTo(t.table_reference, j, "tableReference");
  SafeGetTo(t.unused_reason.value, j, "unusedReason");
}

void to_json(nlohmann::json& j, MaterializedViewStatistics const& m) {
  j = nlohmann::json{{"materializedView", m.materialized_view}};
}
void from_json(nlohmann::json const& j, MaterializedViewStatistics& m) {
  SafeGetTo(m.materialized_view, j, "materializedView");
}

void to_json(nlohmann::json& j, MaterializedView const& m) {
  j = nlohmann::json{
      {"chosen", m.chosen},
      {"estimatedBytesSaved", std::to_string(m.estimated_bytes_saved)},
      {"rejectedReason", m.rejected_reason.value},
      {"tableReference", m.table_reference}};
}
void from_json(nlohmann::json const& j, MaterializedView& m) {
  SafeGetTo(m.chosen, j, "chosen");
  m.estimated_bytes_saved = GetNumberFromJson(j, "estimatedBytesSaved");
  SafeGetTo(m.rejected_reason.value, j, "rejectedReason");
  SafeGetTo(m.table_reference, j, "tableReference");
}

void to_json(nlohmann::json& j, StagePerformanceStandaloneInsight const& s) {
  j = nlohmann::json{
      {"stageId", std::to_string(s.stage_id)},
      {"slotContention", s.slot_contention},
      {"insufficientShuffleQuota", s.insufficient_shuffle_quota}};
}
void from_json(nlohmann::json const& j, StagePerformanceStandaloneInsight& s) {
  s.stage_id = GetNumberFromJson(j, "stageId");
  SafeGetTo(s.slot_contention, j, "slotContention");
  SafeGetTo(s.insufficient_shuffle_quota, j, "insufficientShuffleQuota");
}

void to_json(nlohmann::json& j, StagePerformanceChangeInsight const& s) {
  j = nlohmann::json{{"stageId", std::to_string(s.stage_id)},
                     {"inputDataChange", s.input_data_change}};
}
void from_json(nlohmann::json const& j, StagePerformanceChangeInsight& s) {
  s.stage_id = GetNumberFromJson(j, "stageId");
  SafeGetTo(s.input_data_change, j, "inputDataChange");
}

void to_json(nlohmann::json& j, InputDataChange const& i) {
  j = nlohmann::json{
      {"recordsReadDiffPercentage", i.records_read_diff_percentage}};
}
void from_json(nlohmann::json const& j, InputDataChange& i) {
  SafeGetTo(i.records_read_diff_percentage, j, "recordsReadDiffPercentage");
}

void to_json(nlohmann::json& j, SearchStatistics const& s) {
  j = nlohmann::json{{"indexUsageMode", s.index_usage_mode.value},
                     {"indexUnusedReasons", s.index_unused_reasons}};
}
void from_json(nlohmann::json const& j, SearchStatistics& s) {
  SafeGetTo(s.index_usage_mode.value, j, "indexUsageMode");
  SafeGetTo(s.index_unused_reasons, j, "indexUnusedReasons");
}

void to_json(nlohmann::json& j, IndexUnusedReason const& i) {
  j = nlohmann::json{{"message", i.message},
                     {"indexName", i.index_name},
                     {"baseTable", i.base_table},
                     {"code", i.code.value}};
}
void from_json(nlohmann::json const& j, IndexUnusedReason& i) {
  SafeGetTo(i.message, j, "message");
  SafeGetTo(i.index_name, j, "indexName");
  SafeGetTo(i.base_table, j, "baseTable");
  SafeGetTo(i.code.value, j, "code");
}

void to_json(nlohmann::json& j, RowAccessPolicyReference const& r) {
  j = nlohmann::json{{"projectId", r.project_id},
                     {"datasetId", r.dataset_id},
                     {"tableId", r.table_id},
                     {"policyId", r.policy_id}};
}
void from_json(nlohmann::json const& j, RowAccessPolicyReference& r) {
  SafeGetTo(r.project_id, j, "projectId");
  SafeGetTo(r.dataset_id, j, "datasetId");
  SafeGetTo(r.table_id, j, "tableId");
  SafeGetTo(r.policy_id, j, "policyId");
}

void to_json(nlohmann::json& j, DmlStats const& d) {
  j = nlohmann::json{{"insertedRowCount", std::to_string(d.inserted_row_count)},
                     {"deletedRowCount", std::to_string(d.deleted_row_count)},
                     {"updatedRowCount", std::to_string(d.updated_row_count)}};
}
void from_json(nlohmann::json const& j, DmlStats& d) {
  d.inserted_row_count = GetNumberFromJson(j, "insertedRowCount");
  d.deleted_row_count = GetNumberFromJson(j, "deletedRowCount");
  d.updated_row_count = GetNumberFromJson(j, "updatedRowCount");
}

void to_json(nlohmann::json& j, ExplainQueryStep const& q) {
  j = nlohmann::json{{"kind", q.kind}, {"substeps", q.sub_steps}};
}
void from_json(nlohmann::json const& j, ExplainQueryStep& q) {
  SafeGetTo(q.kind, j, "kind");
  SafeGetTo(q.sub_steps, j, "substeps");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
