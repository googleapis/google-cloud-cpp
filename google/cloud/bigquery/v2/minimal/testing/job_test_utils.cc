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

using ::google::cloud::bigquery_v2_minimal_internal::Clustering;
using ::google::cloud::bigquery_v2_minimal_internal::ComputeMode;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::DmlStats;
using ::google::cloud::bigquery_v2_minimal_internal::EncryptionConfiguration;
using ::google::cloud::bigquery_v2_minimal_internal::EvaluationKind;
using ::google::cloud::bigquery_v2_minimal_internal::ExplainQueryStage;
using ::google::cloud::bigquery_v2_minimal_internal::IndexedUnusedReasonCode;
using ::google::cloud::bigquery_v2_minimal_internal::IndexUnusedReason;
using ::google::cloud::bigquery_v2_minimal_internal::IndexUsageMode;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobConfiguration;
using ::google::cloud::bigquery_v2_minimal_internal::JobConfigurationQuery;
using ::google::cloud::bigquery_v2_minimal_internal::JobQueryStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::JobStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::KeyResultStatementKind;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::MaterializedView;
using ::google::cloud::bigquery_v2_minimal_internal::MaterializedViewStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::MetadataCacheStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::MetadataCacheUnusedReason;
using ::google::cloud::bigquery_v2_minimal_internal::PerformanceInsights;
using ::google::cloud::bigquery_v2_minimal_internal::QueryTimelineSample;
using ::google::cloud::bigquery_v2_minimal_internal::RangePartitioning;
using ::google::cloud::bigquery_v2_minimal_internal::RejectedReason;
using ::google::cloud::bigquery_v2_minimal_internal::RoutineReference;
using ::google::cloud::bigquery_v2_minimal_internal::RowAccessPolicyReference;
using ::google::cloud::bigquery_v2_minimal_internal::ScriptOptions;
using ::google::cloud::bigquery_v2_minimal_internal::ScriptStackFrame;
using ::google::cloud::bigquery_v2_minimal_internal::ScriptStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::SearchStatistics;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataCacheUsage;
using ::google::cloud::bigquery_v2_minimal_internal::TableReference;
using ::google::cloud::bigquery_v2_minimal_internal::TimePartitioning;

using ::google::cloud::bigquery_v2_minimal_testing::MakeConnectionProperty;
using ::google::cloud::bigquery_v2_minimal_testing::MakeDatasetReference;

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

  r.projectId = "1234";
  r.datasetId = "1";
  r.tableId = "2";
  r.policyId = "3";

  return r;
}

TableReference MakeTableReference() {
  TableReference t;
  t.datasetId = "1";
  t.projectId = "2";
  t.tableId = "3";

  return t;
}

RoutineReference MakeRoutineReference() {
  RoutineReference r;
  r.datasetId = "1";
  r.projectId = "2";
  r.routineId = "3";

  return r;
}

SearchStatistics MakeSearchStatistics() {
  SearchStatistics s;

  s.indexUsageMode = IndexUsageMode::PartiallyUsed();
  IndexUnusedReason i;
  i.baseTable = MakeTableReference();
  i.indexName = "test-index";
  i.code = IndexedUnusedReasonCode::BaseTableTooSmall();
  s.indexUnusedReasons.push_back(i);

  return s;
}

PerformanceInsights MakePerformanceInsights() {
  PerformanceInsights p;

  p.avg_previous_execution_time = kDefaultTestTime;
  p.stage_performance_change_insights.stageId = kDefaultTestInt;
  p.stage_performance_change_insights.inputDataChange
      .recordsReadDiffPercentage = kDefaultTestFloat;
  p.stage_performance_standalone_insights.insufficientShuffleQuota = true;
  p.stage_performance_standalone_insights.slotContention = true;
  p.stage_performance_standalone_insights.stageId = kDefaultTestInt;

  return p;
}

MaterializedViewStatistics MakeMaterializedViewStatistics() {
  MaterializedViewStatistics m;
  MaterializedView mv;

  mv.chosen = true;
  mv.estimatedBytesSaved = kDefaultTestInt;
  mv.rejectedReason = RejectedReason::BaseTableDataChange();
  mv.tableReference = MakeTableReference();
  m.materializedView.push_back(mv);

  return m;
}

MetadataCacheStatistics MakeMetadataCacheStatistics() {
  MetadataCacheStatistics m;

  TableMetadataCacheUsage t;
  t.explanation = "test-table-metadata";
  t.tableReference = MakeTableReference();
  t.unusedReason = MetadataCacheUnusedReason::ExceededMaxStaleness();
  m.tableMetadataCacheUsage.push_back(t);

  return m;
}

ScriptStackFrame MakeScriptStackFrame() {
  ScriptStackFrame sf;
  sf.endColumn = kDefaultTestInt;
  sf.endLine = kDefaultTestInt;
  sf.procedureId = "proc-id";
  sf.startColumn = kDefaultTestInt;
  sf.startLine = kDefaultTestInt;
  sf.text = "stack-frame-text";

  return sf;
}

ScriptStatistics MakeScriptStatistics() {
  ScriptStatistics s;
  s.evaluationKind = EvaluationKind::Statement();
  s.stackFrames.push_back(MakeScriptStackFrame());

  return s;
}

ScriptOptions MakeScriptOptions() {
  ScriptOptions s;
  s.statementByteBudget = 10;
  s.statementTimeoutMs = 10;
  s.keyResultStatement = KeyResultStatementKind::FirstSelect();

  return s;
}

EncryptionConfiguration MakeEncryptionConfiguration() {
  EncryptionConfiguration e;
  e.kmsKeyName = "encryption-key-name";

  return e;
}

TimePartitioning MakeTimePartitioning() {
  TimePartitioning tp;
  tp.field = "tp-field-1";
  tp.type = "tp-field-type";

  return tp;
}

Clustering MakeClustering() {
  Clustering c;
  c.fields.emplace_back("clustering-field-1");
  c.fields.emplace_back("clustering-field-2");

  return c;
}

RangePartitioning MakeRangePartitioning() {
  RangePartitioning rp;
  rp.field = "rp-field-1";
  rp.range.end = "range-end";
  rp.range.start = "range-start";
  rp.range.interval = "range-interval";

  return rp;
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

  stats.parent_job_id = "parent-job-123";
  stats.session_info.sessionId = "session-id-123";
  stats.transaction_info.transactionId = "transaction-id-123";
  stats.reservation_id = "reservation-id-123";

  stats.row_level_security_statistics.rowLevelSecurityApplied = true;
  stats.data_masking_statistics.dataMaskingApplied = true;

  stats.completion_ratio = kDefaultTestDouble;
  stats.quota_deferments.emplace_back("quota-defer-1");

  stats.script_statistics = MakeScriptStatistics();

  stats.job_query_stats = MakeJobQueryStats();

  return stats;
}

JobConfigurationQuery MakeJobConfigurationQuery() {
  JobConfigurationQuery config;
  config.query = "select 1;";
  config.createDisposition = "job-create-disposition";
  config.writeDisposition = "job-write-disposition";
  config.priority = "job-priority";
  config.parameterMode = "job-param-mode";
  config.preserveNulls = true;
  config.allowLargeResults = true;
  config.useQueryCache = true;
  config.flattenResults = true;
  config.useLegacySql = true;
  config.createSession = true;
  config.maximumBytesBilled = 0;

  config.queryParameters.push_back(MakeQueryParameter());
  config.schemaUpdateOptions.emplace_back("job-update-options");
  config.connectionProperties.push_back(MakeConnectionProperty());

  config.defaultDataset = MakeDatasetReference();
  config.destinationTable = MakeTableReference();
  config.timePartitioning = MakeTimePartitioning();

  config.rangePartitioning = MakeRangePartitioning();
  config.clustering = MakeClustering();
  config.destinationEncryptionConfiguration = MakeEncryptionConfiguration();
  config.scriptOptions = MakeScriptOptions();
  config.systemVariables = MakeSystemVariables();

  return config;
}

JobConfiguration MakeJobConfiguration() {
  JobConfiguration jc;

  jc.dryRun = true;
  jc.jobTimeoutMs = 10;
  jc.jobType = "QUERY";
  jc.labels.insert({"label-key1", "label-val1"});
  jc.query = MakeJobConfigurationQuery();

  return jc;
}

Job MakeJob() {
  Job job;

  job.etag = "etag";
  job.id = "1";
  job.kind = "Job";
  job.selfLink = "self-link";
  job.user_email = "a@b.com";
  job.jobReference.projectId = "1";
  job.jobReference.jobId = "2";
  job.jobReference.location = "us-east";
  job.status.state = "DONE";
  job.configuration = MakeJobConfiguration();
  job.statistics = MakeJobStats();

  return job;
}

ListFormatJob MakeListFormatJob() {
  ListFormatJob job;

  job.id = "1";
  job.kind = "Job";
  job.user_email = "a@b.com";
  job.principal_subject = "principal-sub";
  job.jobReference.projectId = "1";
  job.jobReference.jobId = "2";
  job.jobReference.location = "us-east";
  job.state = "DONE";
  job.status.state = "DONE";
  job.configuration = MakeJobConfiguration();
  job.statistics = MakeJobStats();

  return job;
}

Job MakePartialJob() {
  Job job;

  job.kind = "jkind";
  job.etag = "jtag";
  job.id = "j123";
  job.selfLink = "jselfLink";
  job.user_email = "juserEmail";
  job.status.state = "DONE";
  job.jobReference.projectId = "p123";
  job.jobReference.jobId = "j123";
  job.configuration.jobType = "QUERY";
  job.configuration.query.query = "select 1;";

  return job;
}

void AssertEqualsPartial(Job& expected, Job& actual) {
  EXPECT_EQ(expected.kind, actual.kind);
  EXPECT_EQ(expected.etag, actual.etag);
  EXPECT_EQ(expected.id, actual.id);
  EXPECT_EQ(expected.selfLink, actual.selfLink);
  EXPECT_EQ(expected.user_email, actual.user_email);
  EXPECT_EQ(expected.status.state, actual.status.state);
  EXPECT_EQ(expected.jobReference.projectId, actual.jobReference.projectId);
  EXPECT_EQ(expected.jobReference.jobId, actual.jobReference.jobId);
  EXPECT_EQ(expected.configuration.jobType, actual.configuration.jobType);
  EXPECT_EQ(expected.configuration.query.query,
            actual.configuration.query.query);
}

void AssertEquals(Job& expected, Job& actual) {
  EXPECT_EQ(expected.etag, actual.etag);
  EXPECT_EQ(expected.id, actual.id);
  EXPECT_EQ(expected.kind, actual.kind);
  EXPECT_EQ(expected.jobReference.projectId, actual.jobReference.projectId);
  EXPECT_EQ(expected.jobReference.jobId, actual.jobReference.jobId);
  EXPECT_EQ(expected.status.state, actual.status.state);
  EXPECT_EQ(expected.configuration.jobType, actual.configuration.jobType);
  EXPECT_EQ(expected.configuration.query.query,
            actual.configuration.query.query);

  AssertEquals(expected.statistics, actual.statistics);
}

void AssertEquals(ListFormatJob& expected, ListFormatJob& actual) {
  EXPECT_EQ(expected.id, actual.id);
  EXPECT_EQ(expected.kind, actual.kind);
  EXPECT_EQ(expected.jobReference.projectId, actual.jobReference.projectId);
  EXPECT_EQ(expected.jobReference.jobId, actual.jobReference.jobId);
  EXPECT_EQ(expected.status.state, actual.status.state);
  EXPECT_EQ(expected.configuration.jobType, actual.configuration.jobType);
  EXPECT_EQ(expected.configuration.query.query,
            actual.configuration.query.query);

  AssertEquals(expected.statistics, actual.statistics);
}

void AssertEquals(JobStatistics& expected, JobStatistics& actual) {
  EXPECT_EQ(expected.creation_time, actual.creation_time);
  EXPECT_EQ(expected.start_time, actual.start_time);
  EXPECT_EQ(expected.end_time, actual.end_time);
  EXPECT_EQ(expected.total_slot_time, actual.total_slot_time);
  EXPECT_EQ(expected.final_execution_duration, actual.final_execution_duration);

  EXPECT_EQ(expected.total_bytes_processed, actual.total_bytes_processed);
  EXPECT_EQ(expected.num_child_jobs, actual.num_child_jobs);
  EXPECT_EQ(expected.parent_job_id, actual.parent_job_id);
  EXPECT_EQ(expected.session_info.sessionId, actual.session_info.sessionId);
  EXPECT_EQ(expected.transaction_info.transactionId,
            actual.transaction_info.transactionId);
  EXPECT_EQ(expected.reservation_id, actual.reservation_id);

  EXPECT_EQ(expected.row_level_security_statistics.rowLevelSecurityApplied,
            actual.row_level_security_statistics.rowLevelSecurityApplied);
  EXPECT_EQ(expected.data_masking_statistics.dataMaskingApplied,
            actual.data_masking_statistics.dataMaskingApplied);

  EXPECT_EQ(expected.completion_ratio, actual.completion_ratio);
  EXPECT_TRUE(std::equal(expected.quota_deferments.begin(),
                         expected.quota_deferments.end(),
                         actual.quota_deferments.begin()));

  EXPECT_EQ(expected.script_statistics, actual.script_statistics);
  AssertEquals(expected.job_query_stats, actual.job_query_stats);
}

void AssertEquals(bigquery_v2_minimal_internal::JobQueryStatistics& expected,
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
  EXPECT_EQ(expected.dcl_target_table, actual.dcl_target_table);
  EXPECT_EQ(expected.dcl_target_view, actual.dcl_target_view);

  EXPECT_EQ(expected.ddl_target_routine, actual.ddl_target_routine);

  EXPECT_EQ(expected.ddl_target_dataset, actual.ddl_target_dataset);
  EXPECT_EQ(expected.dcl_target_dataset, actual.dcl_target_dataset);

  EXPECT_EQ(expected.ddl_target_row_access_policy,
            actual.ddl_target_row_access_policy);

  EXPECT_EQ(expected.search_statistics, actual.search_statistics);

  EXPECT_EQ(expected.performance_insights, actual.performance_insights);

  EXPECT_TRUE(
      std::equal(expected.materialized_view_statistics.materializedView.begin(),
                 expected.materialized_view_statistics.materializedView.end(),
                 actual.materialized_view_statistics.materializedView.begin()));
  EXPECT_TRUE(std::equal(
      expected.metadata_cache_statistics.tableMetadataCacheUsage.begin(),
      expected.metadata_cache_statistics.tableMetadataCacheUsage.end(),
      actual.metadata_cache_statistics.tableMetadataCacheUsage.begin()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google
