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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_QUERY_STATS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_QUERY_STATS_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_schema.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

// Describes an operation within a query stage.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#explainquerystep
struct ExplainQueryStep {
  std::string kind;
  std::vector<std::string> sub_steps;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ExplainQueryStep const& q);
void from_json(nlohmann::json const& j, ExplainQueryStep& q);
bool operator==(ExplainQueryStep const& lhs, ExplainQueryStep const& rhs);

// Indicates the type of compute mode for the query stage.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#computemode
struct ComputeMode {
  static ComputeMode UnSpecified();
  static ComputeMode BigQuery();
  static ComputeMode BIEngine();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ComputeMode, value);
inline bool operator==(ComputeMode const& lhs, ComputeMode const& rhs) {
  return lhs.value == rhs.value;
}

// Describes execution plan for the query i.e
// a single stage of query execution.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#ExplainQueryStage.
struct ExplainQueryStage {
  std::string name;
  std::string status;

  std::int64_t id = 0;
  std::int64_t shuffle_output_bytes = 0;
  std::int64_t shuffle_output_bytes_spilled = 0;
  std::int64_t records_read = 0;
  std::int64_t records_written = 0;
  std::int64_t parallel_inputs = 0;
  std::int64_t completed_parallel_inputs = 0;
  std::vector<std::string> input_stages;  // Can sometimes be alphanumeric.

  std::chrono::milliseconds start_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds end_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds slot_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds wait_avg_time_spent = std::chrono::milliseconds(0);
  std::chrono::milliseconds wait_max_time_spent = std::chrono::milliseconds(0);
  std::chrono::milliseconds read_avg_time_spent = std::chrono::milliseconds(0);
  std::chrono::milliseconds read_max_time_spent = std::chrono::milliseconds(0);
  std::chrono::milliseconds write_avg_time_spent = std::chrono::milliseconds(0);
  std::chrono::milliseconds write_max_time_spent = std::chrono::milliseconds(0);
  std::chrono::milliseconds compute_avg_time_spent =
      std::chrono::milliseconds(0);
  std::chrono::milliseconds compute_max_time_spent =
      std::chrono::milliseconds(0);

  double wait_ratio_avg = 0;
  double wait_ratio_max = 0;
  double read_ratio_avg = 0;
  double read_ratio_max = 0;
  double compute_ratio_avg = 0;
  double compute_ratio_max = 0;
  double write_ratio_avg = 0;
  double write_ratio_max = 0;

  std::vector<ExplainQueryStep> steps;
  ComputeMode compute_mode;

  std::string DebugString(absl::string_view eq_name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ExplainQueryStage const& q);
void from_json(nlohmann::json const& j, ExplainQueryStage& q);
bool operator==(ExplainQueryStage const& lhs, ExplainQueryStage const& rhs);

// Describes a timeline of job execution including a summary of the
// state of query execution at a given time.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#querytimelinesample.
struct QueryTimelineSample {
  std::chrono::milliseconds elapsed_time = std::chrono::milliseconds(0);
  std::chrono::milliseconds total_slot_time = std::chrono::milliseconds(0);

  std::int64_t pending_units = 0;
  std::int64_t completed_units = 0;
  std::int64_t active_units = 0;
  std::int64_t estimated_runnable_units = 0;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, QueryTimelineSample const& q);
void from_json(nlohmann::json const& j, QueryTimelineSample& q);
bool operator==(QueryTimelineSample const& lhs, QueryTimelineSample const& rhs);

// Represents the detailed statistics for DML statements INSERT,
// UPDATE, DELETE, MERGE or TRUNCATE.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/DmlStats.
struct DmlStats {
  std::int64_t inserted_row_count = 0;
  std::int64_t deleted_row_count = 0;
  std::int64_t updated_row_count = 0;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, DmlStats const& d);
void from_json(nlohmann::json const& j, DmlStats& d);
bool operator==(DmlStats const& lhs, DmlStats const& rhs);

// Represents the Id path of a row access policy. It is
// present only for CREATE/DROP ROW ACCESS POLICY queries.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/RowAccessPolicyReference.
struct RowAccessPolicyReference {
  std::string project_id;
  std::string dataset_id;
  std::string table_id;
  std::string policy_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, RowAccessPolicyReference const& r);
void from_json(nlohmann::json const& j, RowAccessPolicyReference& r);
bool operator==(RowAccessPolicyReference const& lhs,
                RowAccessPolicyReference const& rhs);

// Indicates the type of search index usage in the entire search query.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#indexusagemode.
struct IndexUsageMode {
  static IndexUsageMode UnSpecified();
  static IndexUsageMode Unused();
  static IndexUsageMode PartiallyUsed();
  static IndexUsageMode FullyUsed();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(IndexUsageMode, value);
inline bool operator==(IndexUsageMode const& lhs, IndexUsageMode const& rhs) {
  return lhs.value == rhs.value;
}

// Indicates the high-level reason for the scenario when no
// search index was used.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#code_1
struct IndexedUnusedReasonCode {
  static IndexedUnusedReasonCode UnSpecified();
  static IndexedUnusedReasonCode IndexConfigNotAvailable();
  static IndexedUnusedReasonCode PendingIndexCreation();
  static IndexedUnusedReasonCode BaseTableTruncated();
  static IndexedUnusedReasonCode IndexConfigModified();
  static IndexedUnusedReasonCode TimeTravelQuery();
  static IndexedUnusedReasonCode NoPruningPower();
  static IndexedUnusedReasonCode UnIndexedSearchFields();
  static IndexedUnusedReasonCode UnSupportedSearchPattern();
  static IndexedUnusedReasonCode OptimizedWithMaterializedView();
  static IndexedUnusedReasonCode SecuredByDataMasking();
  static IndexedUnusedReasonCode MismatchedTextAnalyzer();
  static IndexedUnusedReasonCode BaseTableTooSmall();
  static IndexedUnusedReasonCode BaseTableTooLarge();
  static IndexedUnusedReasonCode EstimatedPerformanceGainTooLow();
  static IndexedUnusedReasonCode NotSupportedInStandardEdition();
  static IndexedUnusedReasonCode InternalError();
  static IndexedUnusedReasonCode OtherReason();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(IndexedUnusedReasonCode, value);
inline bool operator==(IndexedUnusedReasonCode const& lhs,
                       IndexedUnusedReasonCode const& rhs) {
  return lhs.value == rhs.value;
}

// Reason about why no search index was used in the
// search query (or sub-query).
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#indexunusedreason.
struct IndexUnusedReason {
  std::string message;
  std::string index_name;

  TableReference base_table;
  IndexedUnusedReasonCode code;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, IndexUnusedReason const& i);
void from_json(nlohmann::json const& j, IndexUnusedReason& i);
bool operator==(IndexUnusedReason const& lhs, IndexUnusedReason const& rhs);

// Describes search query specific statistics and is
// populated as part of JobQueryStatistics.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#searchstatistics.
struct SearchStatistics {
  IndexUsageMode index_usage_mode;
  std::vector<IndexUnusedReason> index_unused_reasons;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, SearchStatistics const& s);
void from_json(nlohmann::json const& j, SearchStatistics& s);
bool operator==(SearchStatistics const& lhs, SearchStatistics const& rhs);

// Details about the input data change insight.
//
// For more details on fields, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#inputdatachange
struct InputDataChange {
  std::float_t records_read_diff_percentage = 0;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, InputDataChange const& i);
void from_json(nlohmann::json const& j, InputDataChange& i);

// Performance insights compared to the previous executions for a specific
// stage.
//
// For more details on fields, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#stageperformancechangeinsight
struct StagePerformanceChangeInsight {
  std::int64_t stage_id = 0;
  InputDataChange input_data_change;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, StagePerformanceChangeInsight const& s);
void from_json(nlohmann::json const& j, StagePerformanceChangeInsight& s);

// Standalone performance insights for a specific stage.
//
// For more details on fields, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#stageperformancestandaloneinsight
struct StagePerformanceStandaloneInsight {
  std::int64_t stage_id = 0;
  bool slot_contention = false;
  bool insufficient_shuffle_quota = false;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, StagePerformanceStandaloneInsight const& s);
void from_json(nlohmann::json const& j, StagePerformanceStandaloneInsight& s);

// Performance insights for the job.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#performanceinsights.
struct PerformanceInsights {
  std::chrono::milliseconds avg_previous_execution_time =
      std::chrono::milliseconds(0);

  StagePerformanceStandaloneInsight stage_performance_standalone_insights;
  StagePerformanceChangeInsight stage_performance_change_insights;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, PerformanceInsights const& p);
void from_json(nlohmann::json const& j, PerformanceInsights& p);
bool operator==(PerformanceInsights const& lhs, PerformanceInsights const& rhs);

// Reason why a materialized view was not chosen for a query.
//
// For more information, see
// https://cloud.google.com/bigquery/docs/materialized-views-use#understand-rejected
struct RejectedReason {
  static RejectedReason UnSpecified();
  static RejectedReason NoData();
  static RejectedReason Cost();
  static RejectedReason BaseTableTruncated();
  static RejectedReason BaseTableDataChange();
  static RejectedReason BaseTablePartitionExpirationChange();
  static RejectedReason BaseTableExpiredPartition();
  static RejectedReason BaseTableIncompatibleMetadataChange();
  static RejectedReason TimeZone();
  static RejectedReason OutOfTimeTravelWindow();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RejectedReason, value);

// A materialized view considered for a query job.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#materializedview
struct MaterializedView {
  bool chosen = false;
  std::int64_t estimated_bytes_saved = 0;

  RejectedReason rejected_reason;
  TableReference table_reference;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, MaterializedView const& m);
void from_json(nlohmann::json const& j, MaterializedView& m);
bool operator==(MaterializedView const& lhs, MaterializedView const& rhs);

// Statistics of materialized views of a query job.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#materializedviewstatistics
struct MaterializedViewStatistics {
  std::vector<MaterializedView> materialized_view;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, MaterializedViewStatistics const& m);
void from_json(nlohmann::json const& j, MaterializedViewStatistics& m);

// Reasons for not using metadata caching.
//
// For more details on fields, see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#unusedreason
struct MetadataCacheUnusedReason {
  static MetadataCacheUnusedReason UnSpecified();
  static MetadataCacheUnusedReason ExceededMaxStaleness();
  static MetadataCacheUnusedReason OtherReason();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MetadataCacheUnusedReason,
                                                value);

// Table level detail on the usage of metadata caching. Only set for Metadata
// caching eligible tables referenced in the query.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#tablemetadatacacheusage
struct TableMetadataCacheUsage {
  std::string explanation;

  TableReference table_reference;
  MetadataCacheUnusedReason unused_reason;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, TableMetadataCacheUsage const& t);
void from_json(nlohmann::json const& j, TableMetadataCacheUsage& t);
bool operator==(TableMetadataCacheUsage const& lhs,
                TableMetadataCacheUsage const& rhs);

// Statistics of metadata cache usage in a query for BigLake tables.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#metadatacachestatistics.
struct MetadataCacheStatistics {
  std::vector<TableMetadataCacheUsage> table_metadata_cache_usage;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, MetadataCacheStatistics const& m);
void from_json(nlohmann::json const& j, MetadataCacheStatistics& m);

// Statistics for a query job.
//
// For more details on field members, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#jobstatistics2
struct JobQueryStatistics {
  std::int64_t estimated_bytes_processed = 0;
  std::int64_t total_partitions_processed = 0;
  std::int64_t total_bytes_processed = 0;
  std::int64_t total_bytes_billed = 0;
  std::int64_t billing_tier = 0;
  std::int64_t num_dml_affected_rows = 0;
  std::int64_t ddl_affected_row_access_policy_count = 0;
  std::int64_t transferred_bytes = 0;

  std::string total_bytes_processed_accuracy;
  std::string statement_type;
  std::string ddl_operation_performed;

  std::chrono::milliseconds total_slot_time = std::chrono::milliseconds(0);
  bool cache_hit = false;

  std::vector<ExplainQueryStage> query_plan;
  std::vector<QueryTimelineSample> timeline;
  std::vector<TableReference> referenced_tables;
  std::vector<RoutineReference> referenced_routines;
  std::vector<QueryParameter> undeclared_query_parameters;

  TableSchema schema;
  DmlStats dml_stats;

  TableReference ddl_target_table;
  RowAccessPolicyReference ddl_target_row_access_policy;
  RoutineReference ddl_target_routine;
  DatasetReference ddl_target_dataset;
  TableReference dcl_target_table;
  TableReference dcl_target_view;
  DatasetReference dcl_target_dataset;
  SearchStatistics search_statistics;
  PerformanceInsights performance_insights;
  MaterializedViewStatistics materialized_view_statistics;
  MetadataCacheStatistics metadata_cache_statistics;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, JobQueryStatistics const& q);
void from_json(nlohmann::json const& j, JobQueryStatistics& q);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_QUERY_STATS_H
