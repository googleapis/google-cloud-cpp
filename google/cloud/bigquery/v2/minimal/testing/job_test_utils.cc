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
#include "google/cloud/bigquery/v2/minimal/testing/common_v2_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::ComputeMode;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::DmlStats;
using ::google::cloud::bigquery_v2_minimal_internal::EvaluationKind;
using ::google::cloud::bigquery_v2_minimal_internal::ExplainQueryStage;
using ::google::cloud::bigquery_v2_minimal_internal::IndexedUnusedReasonCode;
using ::google::cloud::bigquery_v2_minimal_internal::IndexUnusedReason;
using ::google::cloud::bigquery_v2_minimal_internal::IndexUsageMode;
using ::google::cloud::bigquery_v2_minimal_internal::JobQueryStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::JobStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::MaterializedView;
using ::google::cloud::bigquery_v2_minimal_internal::MaterializedViewStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::MetadataCacheStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::MetadataCacheUnusedReason;
using ::google::cloud::bigquery_v2_minimal_internal::PerformanceInsights;
using ::google::cloud::bigquery_v2_minimal_internal::QueryTimelineSample;
using ::google::cloud::bigquery_v2_minimal_internal::RejectedReason;
using ::google::cloud::bigquery_v2_minimal_internal::RoutineReference;
using ::google::cloud::bigquery_v2_minimal_internal::RowAccessPolicyReference;
using ::google::cloud::bigquery_v2_minimal_internal::ScriptStackFrame;
using ::google::cloud::bigquery_v2_minimal_internal::ScriptStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::SearchStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataCacheUsage;
using ::google::cloud::bigquery_v2_minimal_internal::TableReference;

using ::testing::IsEmpty;
using ::testing::Not;

constexpr std::chrono::milliseconds kDefaultTestTime =
    std::chrono::milliseconds{10};
constexpr std::int64_t kDefaultTestInt = 1234;
constexpr double kDefaultTestDouble = 1234.1234;
constexpr std::float_t kDefaultTestFloat = 12.12F;

namespace {
ExplainQueryStage MakeExplainQueryStage() {
  ExplainQueryStage e;
  e.name = "test-explain";
  e.status = "explain-status";

  e.id = kDefaultTestInt;
  e.shuffle_output_bytes = kDefaultTestInt;
  e.shuffle_output_bytes_spilled = kDefaultTestInt;
  e.records_read = kDefaultTestInt;
  e.records_written = kDefaultTestInt;
  e.parallel_inputs = kDefaultTestInt;
  e.completed_parallel_inputs = kDefaultTestInt;
  e.input_stages = {kDefaultTestInt};

  e.start_time = kDefaultTestTime;
  e.end_time = kDefaultTestTime;
  e.slot_time = kDefaultTestTime;
  e.wait_avg_time_spent = kDefaultTestTime;
  e.wait_max_time_spent = kDefaultTestTime;
  e.read_avg_time_spent = kDefaultTestTime;
  e.read_max_time_spent = kDefaultTestTime;
  e.write_avg_time_spent = kDefaultTestTime;
  e.write_max_time_spent = kDefaultTestTime;
  e.compute_avg_time_spent = kDefaultTestTime;
  e.compute_max_time_spent = kDefaultTestTime;

  e.wait_ratio_avg = kDefaultTestDouble;
  e.wait_ratio_max = kDefaultTestDouble;
  e.read_ratio_avg = kDefaultTestDouble;
  e.read_ratio_max = kDefaultTestDouble;
  e.compute_ratio_avg = kDefaultTestDouble;
  e.compute_ratio_max = kDefaultTestDouble;
  e.write_ratio_avg = kDefaultTestDouble;
  e.write_ratio_max = kDefaultTestDouble;
  e.steps.push_back({"sub-step-kind", {"sub-step-1"}});
  e.compute_mode = ComputeMode::BigQuery();

  return e;
}

QueryTimelineSample MakeQueryTimeLineSample() {
  QueryTimelineSample q;
  q.elapsed_time = kDefaultTestTime;
  q.total_slot_time = kDefaultTestTime;

  q.pending_units = kDefaultTestInt;
  q.completed_units = kDefaultTestInt;
  q.active_units = kDefaultTestInt;
  q.estimated_runnable_units = kDefaultTestInt;

  return q;
}

RowAccessPolicyReference MakeRowAccessPolicyReference() {
  RowAccessPolicyReference r;

  r.project_id = "1234";
  r.dataset_id = "1";
  r.table_id = "2";
  r.policy_id = "3";

  return r;
}

TableReference MakeTableReference() {
  TableReference t;
  t.dataset_id = "1";
  t.project_id = "2";
  t.table_id = "3";

  return t;
}

RoutineReference MakeRoutineReference() {
  RoutineReference r;
  r.dataset_id = "1";
  r.project_id = "2";
  r.routine_id = "3";

  return r;
}

DatasetReference MakeDatasetReference() {
  DatasetReference d;
  d.dataset_id = "1";
  d.project_id = "2";

  return d;
}

SearchStatistics MakeSearchStatistics() {
  SearchStatistics s;

  s.index_usage_mode = IndexUsageMode::PartiallyUsed();
  IndexUnusedReason i;
  i.base_table = MakeTableReference();
  i.index_name = "test-index";
  i.code = IndexedUnusedReasonCode::BaseTableTooSmall();
  s.index_unused_reasons.push_back(i);

  return s;
}

PerformanceInsights MakePerformanceInsights() {
  PerformanceInsights p;

  p.avg_previous_execution_time = kDefaultTestTime;
  p.stage_performance_change_insights.stage_id = kDefaultTestInt;
  p.stage_performance_change_insights.input_data_change
      .records_read_diff_percentage = kDefaultTestFloat;
  p.stage_performance_standalone_insights.insufficient_shuffle_quota = true;
  p.stage_performance_standalone_insights.slot_contention = true;
  p.stage_performance_standalone_insights.stage_id = kDefaultTestInt;

  return p;
}

MaterializedViewStatistics MakeMaterializedViewStatistics() {
  MaterializedViewStatistics m;
  MaterializedView mv;

  mv.chosen = true;
  mv.estimated_bytes_saved = kDefaultTestInt;
  mv.rejected_reason = RejectedReason::BaseTableDataChange();
  mv.table_reference = MakeTableReference();
  m.materialized_view.push_back(mv);

  return m;
}

MetadataCacheStatistics MakeMetadataCacheStatistics() {
  MetadataCacheStatistics m;

  TableMetadataCacheUsage t;
  t.explanation = "test-table-metadata";
  t.table_reference = MakeTableReference();
  t.unused_reason = MetadataCacheUnusedReason::ExceededMaxStaleness();
  m.table_metadata_cache_usage.push_back(t);

  return m;
}

ScriptStackFrame MakeScriptStackFrame() {
  ScriptStackFrame sf;
  sf.end_column = kDefaultTestInt;
  sf.end_line = kDefaultTestInt;
  sf.procedure_id = "proc-id";
  sf.start_column = kDefaultTestInt;
  sf.start_line = kDefaultTestInt;
  sf.text = "stack-frame-text";

  return sf;
}

ScriptStatistics MakeScriptStatistics() {
  ScriptStatistics s;
  s.evaluation_kind = EvaluationKind::Statement();
  s.stack_frames.push_back(MakeScriptStackFrame());
  return s;
}
}  // namespace

JobQueryStatistics MakeJobQueryStats() {
  JobQueryStatistics stats;

  stats.billing_tier = kDefaultTestInt;
  stats.estimated_bytes_processed = kDefaultTestInt;
  stats.total_partitions_processed = kDefaultTestInt;
  stats.total_bytes_processed = kDefaultTestInt;
  stats.total_bytes_billed = kDefaultTestInt;
  stats.billing_tier = kDefaultTestInt;
  stats.num_dml_affected_rows = kDefaultTestInt;
  stats.ddl_affected_row_access_policy_count = kDefaultTestInt;
  stats.transferred_bytes = kDefaultTestInt;

  stats.total_bytes_processed_accuracy = "total_bytes_processed_accuracy";
  stats.statement_type = "statement_type";
  stats.ddl_operation_performed = "ddl_operation_performed";

  stats.total_slot_time = kDefaultTestTime;
  stats.cache_hit = true;

  stats.query_plan.push_back({MakeExplainQueryStage()});
  stats.timeline.push_back(MakeQueryTimeLineSample());
  stats.referenced_tables.push_back(MakeTableReference());
  stats.referenced_routines.push_back(MakeRoutineReference());
  stats.undeclared_query_parameters.push_back(MakeQueryParameter());

  stats.schema = MakeTable().schema;
  stats.dml_stats = {kDefaultTestInt, kDefaultTestInt, kDefaultTestInt};

  stats.ddl_target_table = MakeTableReference();
  stats.ddl_destination_table = MakeTableReference();
  stats.ddl_target_routine = MakeRoutineReference();
  stats.ddl_target_dataset = MakeDatasetReference();
  stats.dcl_target_table = MakeTableReference();
  stats.dcl_target_view = MakeTableReference();
  stats.dcl_target_dataset = MakeDatasetReference();

  stats.ddl_target_row_access_policy = MakeRowAccessPolicyReference();
  stats.search_statistics = MakeSearchStatistics();
  stats.performance_insights = MakePerformanceInsights();
  stats.materialized_view_statistics = MakeMaterializedViewStatistics();
  stats.metadata_cache_statistics = MakeMetadataCacheStatistics();

  return stats;
}

JobStatistics MakeJobStats() {
  JobStatistics stats;
  stats.creation_time = kDefaultTestTime;
  stats.start_time = kDefaultTestTime;
  stats.end_time = kDefaultTestTime;
  stats.total_slot_time = kDefaultTestTime;
  stats.final_execution_duration = kDefaultTestTime;

  stats.total_bytes_processed = kDefaultTestInt;
  stats.num_child_jobs = kDefaultTestInt;
  stats.total_modified_partitions = kDefaultTestInt;

  stats.parent_job_id = "parent-job-123";
  stats.session_id = "session-id-123";
  stats.transaction_id = "transaction-id-123";
  stats.reservation_id = "reservation-id-123";

  stats.row_level_security_applied = true;
  stats.data_masking_applied = true;

  stats.completion_ratio = kDefaultTestDouble;
  stats.quota_deferments.emplace_back("quota-defer-1");

  stats.script_statistics = MakeScriptStatistics();

  stats.job_query_stats = MakeJobQueryStats();

  return stats;
}

void AssertJobStatsEquals(JobStatistics& expected, JobStatistics& actual) {
  EXPECT_EQ(expected.creation_time, actual.creation_time);
  EXPECT_EQ(expected.start_time, actual.start_time);
  EXPECT_EQ(expected.end_time, actual.end_time);
  EXPECT_EQ(expected.total_slot_time, actual.total_slot_time);
  EXPECT_EQ(expected.final_execution_duration, actual.final_execution_duration);

  EXPECT_EQ(expected.total_bytes_processed, actual.total_bytes_processed);
  EXPECT_EQ(expected.num_child_jobs, actual.num_child_jobs);
  EXPECT_EQ(expected.total_modified_partitions,
            actual.total_modified_partitions);

  EXPECT_EQ(expected.parent_job_id, actual.parent_job_id);
  EXPECT_EQ(expected.session_id, actual.session_id);
  EXPECT_EQ(expected.transaction_id, actual.transaction_id);
  EXPECT_EQ(expected.reservation_id, actual.reservation_id);

  EXPECT_EQ(expected.row_level_security_applied,
            actual.row_level_security_applied);
  EXPECT_EQ(expected.data_masking_applied, actual.data_masking_applied);

  EXPECT_EQ(expected.completion_ratio, actual.completion_ratio);
  EXPECT_TRUE(std::equal(expected.quota_deferments.begin(),
                         expected.quota_deferments.end(),
                         actual.quota_deferments.begin()));

  EXPECT_EQ(expected.script_statistics, actual.script_statistics);
  AssertJobQueryStatsEquals(expected.job_query_stats, actual.job_query_stats);
}

void AssertJobQueryStatsEquals(
    bigquery_v2_minimal_internal::JobQueryStatistics& expected,
    bigquery_v2_minimal_internal::JobQueryStatistics& actual) {
  EXPECT_EQ(expected.estimated_bytes_processed,
            actual.estimated_bytes_processed);
  EXPECT_EQ(expected.total_partitions_processed,
            actual.total_partitions_processed);
  EXPECT_EQ(expected.total_bytes_processed, actual.total_bytes_processed);
  EXPECT_EQ(expected.total_bytes_billed, actual.total_bytes_billed);
  EXPECT_EQ(expected.billing_tier, actual.billing_tier);
  EXPECT_EQ(expected.num_dml_affected_rows, actual.num_dml_affected_rows);
  EXPECT_EQ(expected.ddl_affected_row_access_policy_count,
            actual.ddl_affected_row_access_policy_count);
  EXPECT_EQ(expected.transferred_bytes, actual.transferred_bytes);
  EXPECT_EQ(expected.total_bytes_processed_accuracy,
            actual.total_bytes_processed_accuracy);
  EXPECT_EQ(expected.statement_type, actual.statement_type);
  EXPECT_EQ(expected.ddl_operation_performed, actual.ddl_operation_performed);
  EXPECT_EQ(expected.total_slot_time, actual.total_slot_time);
  EXPECT_EQ(expected.cache_hit, actual.cache_hit);

  EXPECT_TRUE(std::equal(expected.query_plan.begin(), expected.query_plan.end(),
                         actual.query_plan.begin()));
  EXPECT_TRUE(std::equal(expected.timeline.begin(), expected.timeline.end(),
                         actual.timeline.begin()));
  EXPECT_TRUE(std::equal(expected.referenced_tables.begin(),
                         expected.referenced_tables.end(),
                         actual.referenced_tables.begin()));
  EXPECT_TRUE(std::equal(expected.referenced_routines.begin(),
                         expected.referenced_routines.end(),
                         actual.referenced_routines.begin()));
  EXPECT_TRUE(std::equal(expected.undeclared_query_parameters.begin(),
                         expected.undeclared_query_parameters.end(),
                         actual.undeclared_query_parameters.begin()));

  ASSERT_THAT(expected.schema.fields, Not(IsEmpty()));
  ASSERT_THAT(actual.schema.fields, Not(IsEmpty()));
  EXPECT_EQ(expected.schema.fields.size(), actual.schema.fields.size());

  EXPECT_EQ(expected.dml_stats, actual.dml_stats);

  EXPECT_EQ(expected.ddl_target_table, actual.ddl_target_table);
  EXPECT_EQ(expected.ddl_destination_table, actual.ddl_destination_table);
  EXPECT_EQ(expected.dcl_target_table, actual.dcl_target_table);
  EXPECT_EQ(expected.dcl_target_view, actual.dcl_target_view);

  EXPECT_EQ(expected.ddl_target_routine, actual.ddl_target_routine);

  EXPECT_EQ(expected.ddl_target_dataset, actual.ddl_target_dataset);
  EXPECT_EQ(expected.dcl_target_dataset, actual.dcl_target_dataset);

  EXPECT_EQ(expected.ddl_target_row_access_policy,
            actual.ddl_target_row_access_policy);

  EXPECT_EQ(expected.search_statistics, actual.search_statistics);

  EXPECT_EQ(expected.performance_insights, actual.performance_insights);

  EXPECT_TRUE(std::equal(
      expected.materialized_view_statistics.materialized_view.begin(),
      expected.materialized_view_statistics.materialized_view.end(),
      actual.materialized_view_statistics.materialized_view.begin()));
  EXPECT_TRUE(std::equal(
      expected.metadata_cache_statistics.table_metadata_cache_usage.begin(),
      expected.metadata_cache_statistics.table_metadata_cache_usage.end(),
      actual.metadata_cache_statistics.table_metadata_cache_usage.begin()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google
