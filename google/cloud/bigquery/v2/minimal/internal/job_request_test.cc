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

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::
    MakeFullGetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakePostQueryRequest;

auto static const kMinCreationTimeMs = 1111111111111;
auto static const kMaxCreationTimeMs = 1111111111211;
auto static const kMinTime = std::chrono::system_clock::from_time_t(0) +
                             std::chrono::milliseconds{kMinCreationTimeMs};
auto static const kMaxTime = std::chrono::system_clock::from_time_t(0) +
                             std::chrono::milliseconds{kMaxCreationTimeMs};

ListJobsRequest GetListJobsRequest() {
  ListJobsRequest request("1");
  request.set_all_users(true)
      .set_max_results(10)
      .set_min_creation_time(kMinTime)
      .set_max_creation_time(kMaxTime)
      .set_parent_job_id("1")
      .set_page_token("123")
      .set_projection(Projection::Full())
      .set_state_filter(StateFilter::Running());
  return request;
}

TEST(GetJobRequestTest, SuccessWithLocation) {
  GetJobRequest request("1", "2");
  request.set_location("useast");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2");
  expected.AddQueryParameter("location", "useast");
  EXPECT_EQ(expected, *actual);
}

TEST(GetJobRequestTest, SuccessWithoutLocation) {
  GetJobRequest request("1", "2");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);
  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2");
  EXPECT_EQ(expected, *actual);
}

TEST(GetJobRequestTest, SuccessWithEndpoint) {
  GetJobRequest request("1", "2");

  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
      {"http://myendpoint.google.com",
       "http://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
      {"myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
      {"https://myendpoint.google.com/",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for endpoint: " + test.endpoint +
                 ", expected: " + test.expected);
    Options opts;
    opts.set<EndpointOption>(test.endpoint);
    internal::OptionsSpan span(opts);

    auto actual = BuildRestRequest(request);
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ(test.expected, actual->path());
  }
}

TEST(GetJobRequestTest, OutputStream) {
  GetJobRequest request("test-project-id", "test-job-id");
  request.set_location("test-location");
  std::string expected = absl::StrCat(
      "GetJobRequest{project_id=", request.project_id(),
      ", job_id=", request.job_id(), ", location=", request.location(), "}");
  std::ostringstream os;
  os << request;
  EXPECT_EQ(expected, os.str());
}

TEST(ListJobsRequestTest, Success) {
  auto const request = GetListJobsRequest();
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);
  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs");
  expected.AddQueryParameter("allUsers", "true");
  expected.AddQueryParameter("maxResults", "10");
  expected.AddQueryParameter("minCreationTime",
                             std::to_string(kMinCreationTimeMs));
  expected.AddQueryParameter("maxCreationTime",
                             std::to_string(kMaxCreationTimeMs));
  expected.AddQueryParameter("pageToken", "123");
  expected.AddQueryParameter("projection", "FULL");
  expected.AddQueryParameter("stateFilter", "RUNNING");
  expected.AddQueryParameter("parentJobId", "1");

  EXPECT_EQ(expected, *actual);
}

TEST(ListJobsRequestTest, OutputStream) {
  auto const request = GetListJobsRequest();
  std::string expected =
      "ListJobsRequest{project_id=1, all_users=true, max_results=10"
      ", page_token=123, projection=FULL, state_filter=RUNNING"
      ", parent_job_id=1}";
  std::ostringstream os;
  os << request;
  EXPECT_EQ(expected, os.str());
}

TEST(GetJobRequestTest, DebugString) {
  GetJobRequest request("test-project-id", "test-job-id");
  request.set_location("test-location");

  EXPECT_EQ(request.DebugString("GetJobRequest", TracingOptions{}),
            R"(GetJobRequest {)"
            R"( project_id: "test-project-id")"
            R"( job_id: "test-job-id")"
            R"( location: "test-location")"
            R"( })");
  EXPECT_EQ(request.DebugString("GetJobRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(GetJobRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( job_id: "test-jo...<truncated>...")"
            R"( location: "test-lo...<truncated>...")"
            R"( })");
  EXPECT_EQ(request.DebugString("GetJobRequest", TracingOptions{}.SetOptions(
                                                     "single_line_mode=F")),
            R"(GetJobRequest {
  project_id: "test-project-id"
  job_id: "test-job-id"
  location: "test-location"
})");
}

TEST(ListJobsRequestTest, DebugString) {
  ListJobsRequest request("test-project-id");
  request.set_all_users(true)
      .set_max_results(10)
      .set_min_creation_time(
          std::chrono::system_clock::from_time_t(1585112316) +
          std::chrono::microseconds(123456))
      .set_max_creation_time(
          std::chrono::system_clock::from_time_t(1585112892) +
          std::chrono::microseconds(654321))
      .set_page_token("test-page-token")
      .set_projection(Projection::Full())
      .set_state_filter(StateFilter::Running())
      .set_parent_job_id("test-job-id");

  EXPECT_EQ(request.DebugString("ListJobsRequest", TracingOptions{}),
            R"(ListJobsRequest {)"
            R"( project_id: "test-project-id")"
            R"( all_users: true)"
            R"( max_results: 10)"
            R"( min_creation_time { "2020-03-25T04:58:36.123456Z" })"
            R"( max_creation_time { "2020-03-25T05:08:12.654321Z" })"
            R"( page_token: "test-page-token")"
            R"( projection { value: "FULL" })"
            R"( state_filter { value: "RUNNING" })"
            R"( parent_job_id: "test-job-id")"
            R"( })");
  EXPECT_EQ(request.DebugString("ListJobsRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(ListJobsRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( all_users: true)"
            R"( max_results: 10)"
            R"( min_creation_time { "2020-03-25T04:58:36.123456Z" })"
            R"( max_creation_time { "2020-03-25T05:08:12.654321Z" })"
            R"( page_token: "test-pa...<truncated>...")"
            R"( projection { value: "FULL" })"
            R"( state_filter { value: "RUNNING" })"
            R"( parent_job_id: "test-jo...<truncated>...")"
            R"( })");
  EXPECT_EQ(request.DebugString("ListJobsRequest", TracingOptions{}.SetOptions(
                                                       "single_line_mode=F")),
            R"(ListJobsRequest {
  project_id: "test-project-id"
  all_users: true
  max_results: 10
  min_creation_time {
    "2020-03-25T04:58:36.123456Z"
  }
  max_creation_time {
    "2020-03-25T05:08:12.654321Z"
  }
  page_token: "test-page-token"
  projection {
    value: "FULL"
  }
  state_filter {
    value: "RUNNING"
  }
  parent_job_id: "test-job-id"
})");
}

TEST(InsertJobRequestTest, Success) {
  InsertJobRequest request("1234", MakeJob());

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1234/jobs");
  EXPECT_EQ(expected, *actual);
}

TEST(InsertJobRequest, DebugString) {
  InsertJobRequest request("1234", MakeJob());

  EXPECT_EQ(
      request.DebugString("InsertJobRequest", TracingOptions{}),
      R"(InsertJobRequest { project_id: "1234" job { etag: "etag" kind: "Job")"
      R"( self_link: "self-link" id: "1" configuration { job_type: "QUERY")"
      R"( dry_run: true job_timeout { "10ms" } labels { key: "label-key1")"
      R"( value: "label-val1" } query_config { query: "select 1;")"
      R"( create_disposition: "job-create-disposition")"
      R"( write_disposition: "job-write-disposition" priority: "job-priority")"
      R"( parameter_mode: "job-param-mode" preserve_nulls: true)"
      R"( allow_large_results: true use_query_cache: true)"
      R"( flatten_results: true use_legacy_sql: true create_session: true)"
      R"( maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update-options")"
      R"( connection_properties { key: "conn-prop-key" value: "conn-prop-val" })"
      R"( query_parameters { name: "query-parameter-name" parameter_type {)"
      R"( type: "query-parameter-type" struct_types { name: "qp-struct-name")"
      R"( description: "qp-struct-description" } } parameter_value {)"
      R"( value: "query-parameter-value" } } default_dataset {)"
      R"( project_id: "2" dataset_id: "1" } destination_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } time_partitioning {)"
      R"( type: "tp-field-type" expiration_time { "0" } field: "tp-field-1")"
      R"( } range_partitioning { field: "rp-field-1" range { start: "range-start")"
      R"( end: "range-end" interval: "range-interval" } } clustering {)"
      R"( fields: "clustering-field-1" fields: "clustering-field-2" })"
      R"( destination_encryption_configuration {)"
      R"( kms_key_name: "encryption-key-name" } script_options {)"
      R"( statement_timeout { "10ms" } statement_byte_budget: 10)"
      R"( key_result_statement { value: "FIRST_SELECT" } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value { type_kind { value: "STRING" } } })"
      R"( types { key: "sql-struct-type-key-3" value { type_kind {)"
      R"( value: "STRING" } } } values { fields { key: "bool-key" value {)"
      R"( value_kind: true } } fields { key: "double-key" value {)"
      R"( value_kind: 3.4 } } fields { key: "string-key" value {)"
      R"( value_kind: "val3" } } } } } } reference { project_id: "1")"
      R"( job_id: "2" location: "us-east" } status { state: "DONE")"
      R"( error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "10ms" } start_time { "10ms" })"
      R"( end_time { "10ms" } total_slot_time { "10ms" })"
      R"( final_execution_duration { "10ms" } total_bytes_processed: 1234)"
      R"( num_child_jobs: 1234)"
      R"( row_level_security_applied: true data_masking_applied: true)"
      R"( completion_ratio: 1234.12 quota_deferments: "quota-defer-1")"
      R"( parent_job_id: "parent-job-123" session_id: "session-id-123")"
      R"( transaction_id: "transaction-id-123")"
      R"( reservation_id: "reservation-id-123" script_statistics {)"
      R"( stack_frames { start_line: 1234 start_column: 1234 end_line: 1234)"
      R"( end_column: 1234 procedure_id: "proc-id" text: "stack-frame-text" })"
      R"( evaluation_kind { value: "STATEMENT" } } job_query_stats {)"
      R"( estimated_bytes_processed: 1234 total_partitions_processed: 1234)"
      R"( total_bytes_processed: 1234 total_bytes_billed: 1234 billing_tier: 1234)"
      R"( num_dml_affected_rows: 1234 ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_bytes_processed_accuracy")"
      R"( statement_type: "statement_type")"
      R"( ddl_operation_performed: "ddl_operation_performed" total_slot_time {)"
      R"( "10ms" } cache_hit: true query_plan { name: "test-explain")"
      R"( status: "explain-status" id: 1234 shuffle_output_bytes: 1234)"
      R"( shuffle_output_bytes_spilled: 1234 records_read: 1234 records_written: 1234)"
      R"( parallel_inputs: 1234 completed_parallel_inputs: 1234 start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" } wait_avg_time_spent { "10ms" })"
      R"( wait_max_time_spent { "10ms" } read_avg_time_spent { "10ms" })"
      R"( read_max_time_spent { "10ms" } write_avg_time_spent { "10ms" })"
      R"( write_max_time_spent { "10ms" } compute_avg_time_spent { "10ms" })"
      R"( compute_max_time_spent { "10ms" } wait_ratio_avg: 1234.12)"
      R"( wait_ratio_max: 1234.12 read_ratio_avg: 1234.12 read_ratio_max: 1234.12)"
      R"( compute_ratio_avg: 1234.12 compute_ratio_max: 1234.12)"
      R"( write_ratio_avg: 1234.12 write_ratio_max: 1234.12)"
      R"( steps { kind: "sub-step-kind" substeps: "sub-step-1" })"
      R"( compute_mode { value: "BIGQUERY" } } timeline { elapsed_time { "10ms" })"
      R"( total_slot_time { "10ms" } pending_units: 1234 completed_units: 1234)"
      R"( active_units: 1234 estimated_runnable_units: 1234 } referenced_tables {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } referenced_routines {)"
      R"( project_id: "2" dataset_id: "1" routine_id: "3" } schema { fields {)"
      R"( name: "fname-1" type: "" mode: "fmode" description: "" collation: "")"
      R"( default_value_expression: "" max_length: 0 precision: 0 scale: 0)"
      R"( categories { } policy_tags { })"
      R"( rounding_mode { value: "" })"
      R"( range_element_type { type: "" } } } dml_stats { inserted_row_count: 1234)"
      R"( deleted_row_count: 1234 updated_row_count: 1234 } ddl_target_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( ddl_target_row_access_policy { project_id: "1234" dataset_id: "1")"
      R"( table_id: "2" policy_id: "3" } ddl_target_routine { project_id: "2")"
      R"( dataset_id: "1" routine_id: "3" } ddl_target_dataset { project_id: "2")"
      R"( dataset_id: "1" } dcl_target_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } dcl_target_view { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } dcl_target_dataset { project_id: "2" dataset_id: "1" })"
      R"( search_statistics { index_unused_reasons { message: "")"
      R"( index_name: "test-index" base_table { project_id: "2" dataset_id: "1")"
      R"( table_id: "3" } code { value: "BASE_TABLE_TOO_SMALL" } })"
      R"( index_usage_mode { value: "PARTIALLY_USED" } } performance_insights {)"
      R"( avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234 slot_contention: true)"
      R"( insufficient_shuffle_quota: true } stage_performance_change_insights {)"
      R"( stage_id: 1234 input_data_change { records_read_diff_percentage: 12.12 } } })"
      R"( materialized_view_statistics { materialized_view { chosen: true)"
      R"( estimated_bytes_saved: 1234 rejected_reason { value: "BASE_TABLE_DATA_CHANGE" })"
      R"( table_reference { project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table-metadata" unused_reason {)"
      R"( value: "EXCEEDED_MAX_STALENESS" } table_reference { project_id: "2")"
      R"( dataset_id: "1" table_id: "3" } } } } } } })");

  EXPECT_EQ(
      request.DebugString(
          "InsertJobRequest",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=10")),
      R"(InsertJobRequest { project_id: "1234" job { etag: "etag" kind: "Job")"
      R"( self_link: "self-link" id: "1" configuration { job_type: "QUERY")"
      R"( dry_run: true job_timeout { "10ms" } labels { key: "label-key1")"
      R"( value: "label-val1" } query_config { query: "select 1;")"
      R"( create_disposition: "job-create...<truncated>...")"
      R"( write_disposition: "job-write-...<truncated>...")"
      R"( priority: "job-priori...<truncated>...")"
      R"( parameter_mode: "job-param-...<truncated>...")"
      R"( preserve_nulls: true allow_large_results: true)"
      R"( use_query_cache: true flatten_results: true use_legacy_sql: true)"
      R"( create_session: true maximum_bytes_billed: 0)"
      R"( schema_update_options: "job-update...<truncated>...")"
      R"( connection_properties { key: "conn-prop-...<truncated>...")"
      R"( value: "conn-prop-...<truncated>..." } query_parameters {)"
      R"( name: "query-para...<truncated>..." parameter_type {)"
      R"( type: "query-para...<truncated>..." struct_types {)"
      R"( name: "qp-struct-...<truncated>...")"
      R"( description: "qp-struct-...<truncated>..." } })"
      R"( parameter_value { value: "query-para...<truncated>..." } })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( destination_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( time_partitioning { type: "tp-field-t...<truncated>...")"
      R"( expiration_time { "0" } field: "tp-field-1" } range_partitioning {)"
      R"( field: "rp-field-1" range { start: "range-star...<truncated>...")"
      R"( end: "range-end" interval: "range-inte...<truncated>..." } })"
      R"( clustering { fields: "clustering...<truncated>...")"
      R"( fields: "clustering...<truncated>..." })"
      R"( destination_encryption_configuration {)"
      R"( kms_key_name: "encryption...<truncated>..." } script_options {)"
      R"( statement_timeout { "10ms" } statement_byte_budget: 10)"
      R"( key_result_statement { value: "FIRST_SELE...<truncated>..." } })"
      R"( system_variables { types { key: "sql-struct-type-key-1")"
      R"( value { type_kind { value: "INT64" } } } types {)"
      R"( key: "sql-struct-type-key-2" value { type_kind { value: "STRING" } } })"
      R"( types { key: "sql-struct-type-key-3" value { type_kind {)"
      R"( value: "STRING" } } } values { fields { key: "bool-key" value {)"
      R"( value_kind: true } } fields { key: "double-key" value {)"
      R"( value_kind: 3.4 } } fields { key: "string-key" value {)"
      R"( value_kind: "val3" } } } } } } reference { project_id: "1")"
      R"( job_id: "2" location: "us-east" } status { state: "DONE")"
      R"( error_result { reason: "" location: "" message: "" } } statistics {)"
      R"( creation_time { "10ms" } start_time { "10ms" } end_time { "10ms" })"
      R"( total_slot_time { "10ms" } final_execution_duration { "10ms" })"
      R"( total_bytes_processed: 1234 num_child_jobs: 1234)"
      R"( row_level_security_applied: true data_masking_applied: true)"
      R"( completion_ratio: 1234.12 quota_deferments: "quota-defe...<truncated>...")"
      R"( parent_job_id: "parent-job...<truncated>...")"
      R"( session_id: "session-id...<truncated>...")"
      R"( transaction_id: "transactio...<truncated>...")"
      R"( reservation_id: "reservatio...<truncated>..." )"
      R"(script_statistics { stack_frames { start_line: 1234)"
      R"( start_column: 1234 end_line: 1234 end_column: 1234)"
      R"( procedure_id: "proc-id" text: "stack-fram...<truncated>..." })"
      R"( evaluation_kind { value: "STATEMENT" } } job_query_stats {)"
      R"( estimated_bytes_processed: 1234 total_partitions_processed: 1234)"
      R"( total_bytes_processed: 1234 total_bytes_billed: 1234)"
      R"( billing_tier: 1234)"
      R"( num_dml_affected_rows: 1234 ddl_affected_row_access_policy_count: 1234)"
      R"( total_bytes_processed_accuracy: "total_byte...<truncated>...")"
      R"( statement_type: "statement_...<truncated>...")"
      R"( ddl_operation_performed: "ddl_operat...<truncated>...")"
      R"( total_slot_time { "10ms" })"
      R"( cache_hit: true query_plan { name: "test-expla...<truncated>...")"
      R"( status: "explain-st...<truncated>..." id: 1234)"
      R"( shuffle_output_bytes: 1234)"
      R"( shuffle_output_bytes_spilled: 1234 records_read: 1234)"
      R"( records_written: 1234)"
      R"( parallel_inputs: 1234 completed_parallel_inputs: 1234)"
      R"( start_time { "10ms" })"
      R"( end_time { "10ms" } slot_time { "10ms" } wait_avg_time_spent { "10ms" })"
      R"( wait_max_time_spent { "10ms" } read_avg_time_spent { "10ms" })"
      R"( read_max_time_spent { "10ms" } write_avg_time_spent { "10ms" })"
      R"( write_max_time_spent { "10ms" } compute_avg_time_spent { "10ms" })"
      R"( compute_max_time_spent { "10ms" } wait_ratio_avg: 1234.12)"
      R"( wait_ratio_max: 1234.12 read_ratio_avg: 1234.12 read_ratio_max: 1234.12)"
      R"( compute_ratio_avg: 1234.12 compute_ratio_max: 1234.12 write_ratio_avg: 1234.12)"
      R"( write_ratio_max: 1234.12 steps { kind: "sub-step-k...<truncated>...")"
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
      R"( ddl_target_dataset { project_id: "2" dataset_id: "1" } dcl_target_table {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } dcl_target_view {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } dcl_target_dataset {)"
      R"( project_id: "2" dataset_id: "1" } search_statistics {)"
      R"( index_unused_reasons { message: "" index_name: "test-index")"
      R"( base_table { project_id: "2" dataset_id: "1" table_id: "3" })"
      R"( code { value: "BASE_TABLE...<truncated>..." } })"
      R"( index_usage_mode { value: "PARTIALLY_...<truncated>..." } })"
      R"( performance_insights { avg_previous_execution_time { "10ms" })"
      R"( stage_performance_standalone_insights { stage_id: 1234)"
      R"( slot_contention: true insufficient_shuffle_quota: true })"
      R"( stage_performance_change_insights { stage_id: 1234)"
      R"( input_data_change { records_read_diff_percentage: 12.12 } } })"
      R"( materialized_view_statistics { materialized_view { chosen: true)"
      R"( estimated_bytes_saved: 1234 rejected_reason {)"
      R"( value: "BASE_TABLE...<truncated>..." } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } })"
      R"( metadata_cache_statistics { table_metadata_cache_usage {)"
      R"( explanation: "test-table...<truncated>..." unused_reason {)"
      R"( value: "EXCEEDED_M...<truncated>..." } table_reference {)"
      R"( project_id: "2" dataset_id: "1" table_id: "3" } } } } } } })");

  EXPECT_EQ(request.DebugString("InsertJobRequest", TracingOptions{}.SetOptions(
                                                        "single_line_mode=F")),
            R"(InsertJobRequest {
  project_id: "1234"
  job {
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
  }
})");
}

TEST(CancelJobRequestTest, SuccessWithLocation) {
  CancelJobRequest request("1", "2");
  request.set_location("useast");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2/cancel");
  expected.AddQueryParameter("location", "useast");
  EXPECT_EQ(expected, *actual);
}

TEST(CancelJobRequestTest, SuccessWithoutLocation) {
  CancelJobRequest request("1", "2");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);
  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2/cancel");
  EXPECT_EQ(expected, *actual);
}

TEST(CancelJobRequestTest, SuccessWithDifferentEndpoints) {
  CancelJobRequest request("1", "2");

  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2/cancel"},
      {"http://myendpoint.google.com",
       "http://myendpoint.google.com/bigquery/v2/projects/1/jobs/2/cancel"},
      {"myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2/cancel"},
      {"https://myendpoint.google.com/",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2/cancel"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for endpoint: " + test.endpoint +
                 ", expected: " + test.expected);
    Options opts;
    opts.set<EndpointOption>(test.endpoint);
    internal::OptionsSpan span(opts);

    auto actual = BuildRestRequest(request);
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ(test.expected, actual->path());
  }
}

TEST(CancelJobRequestTest, DebugString) {
  CancelJobRequest request("test-project-id", "test-job-id");
  request.set_location("test-location");

  EXPECT_EQ(request.DebugString("CancelJobRequest", TracingOptions{}),
            R"(CancelJobRequest {)"
            R"( project_id: "test-project-id")"
            R"( job_id: "test-job-id")"
            R"( location: "test-location")"
            R"( })");
  EXPECT_EQ(request.DebugString("CancelJobRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(CancelJobRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( job_id: "test-jo...<truncated>...")"
            R"( location: "test-lo...<truncated>...")"
            R"( })");
  EXPECT_EQ(request.DebugString("CancelJobRequest", TracingOptions{}.SetOptions(
                                                        "single_line_mode=F")),
            R"(CancelJobRequest {
  project_id: "test-project-id"
  job_id: "test-job-id"
  location: "test-location"
})");
}

TEST(PostQueryRequestTest, Success) {
  auto const& request = MakePostQueryRequest();

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/test-project-id/"
      "queries");
  EXPECT_EQ(expected, *actual);
}

TEST(PostQueryRequestTest, DebugString) {
  auto const& request = MakePostQueryRequest();

  EXPECT_EQ(
      request.DebugString("PostQueryRequest", TracingOptions{}),
      R"(PostQueryRequest { project_id: "test-project-id" query_request {)"
      R"( query: "select 1;" kind: "query-kind" parameter_mode: "parameter-mode")"
      R"( location: "useast1" request_id: "1234" dry_run: true)"
      R"( preserver_nulls: true use_query_cache: true use_legacy_sql: true)"
      R"( create_session: true max_results: 10 maximum_bytes_billed: 100000)"
      R"( timeout { "10ms" } connection_properties { key: "conn-prop-key")"
      R"( value: "conn-prop-val" } query_parameters { name: "query-parameter-name")"
      R"( parameter_type { type: "query-parameter-type" struct_types {)"
      R"( name: "qp-struct-name" description: "qp-struct-description" } })"
      R"( parameter_value { value: "query-parameter-value" } })"
      R"( labels { key: "lk1" value: "lv1" } labels { key: "lk2" value: "lv2" })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( format_options { use_int64_timestamp: true } } })");

  EXPECT_EQ(
      request.DebugString(
          "PostQueryRequest",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(PostQueryRequest { project_id: "test-pr...<truncated>...")"
      R"( query_request { query: "select ...<truncated>...")"
      R"( kind: "query-k...<truncated>..." parameter_mode: "paramet...<truncated>...")"
      R"( location: "useast1" request_id: "1234" dry_run: true preserver_nulls: true)"
      R"( use_query_cache: true use_legacy_sql: true create_session: true)"
      R"( max_results: 10 maximum_bytes_billed: 100000 timeout { "10ms" })"
      R"( connection_properties { key: "conn-pr...<truncated>...")"
      R"( value: "conn-pr...<truncated>..." } query_parameters {)"
      R"( name: "query-p...<truncated>..." parameter_type {)"
      R"( type: "query-p...<truncated>..." struct_types {)"
      R"( name: "qp-stru...<truncated>..." description: "qp-stru...<truncated>..." } })"
      R"( parameter_value { value: "query-p...<truncated>..." } })"
      R"( labels { key: "lk1" value: "lv1" } labels { key: "lk2" value: "lv2" })"
      R"( default_dataset { project_id: "2" dataset_id: "1" })"
      R"( format_options { use_int64_timestamp: true } } })");

  EXPECT_EQ(request.DebugString("PostQueryRequest", TracingOptions{}.SetOptions(
                                                        "single_line_mode=F")),
            R"(PostQueryRequest {
  project_id: "test-project-id"
  query_request {
    query: "select 1;"
    kind: "query-kind"
    parameter_mode: "parameter-mode"
    location: "useast1"
    request_id: "1234"
    dry_run: true
    preserver_nulls: true
    use_query_cache: true
    use_legacy_sql: true
    create_session: true
    max_results: 10
    maximum_bytes_billed: 100000
    timeout {
      "10ms"
    }
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
    labels {
      key: "lk1"
      value: "lv1"
    }
    labels {
      key: "lk2"
      value: "lv2"
    }
    default_dataset {
      project_id: "2"
      dataset_id: "1"
    }
    format_options {
      use_int64_timestamp: true
    }
  }
})");
}

TEST(GetQueryResultsRequestTest, SuccessWithDifferentEndpoints) {
  GetQueryResultsRequest request("1", "2");

  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/queries/2"},
      {"http://myendpoint.google.com",
       "http://myendpoint.google.com/bigquery/v2/projects/1/queries/2"},
      {"myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/queries/2"},
      {"https://myendpoint.google.com/",
       "https://myendpoint.google.com/bigquery/v2/projects/1/queries/2"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for endpoint: " + test.endpoint +
                 ", expected: " + test.expected);
    Options opts;
    opts.set<EndpointOption>(test.endpoint);
    internal::OptionsSpan span(opts);

    auto actual = BuildRestRequest(request);
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ(test.expected, actual->path());
  }
}

TEST(GetQueryResultsRequestTest, SuccessWithQueryParameters) {
  auto request = MakeFullGetQueryResultsRequest();

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/queries/2");
  expected.AddQueryParameter("pageToken", "npt123");
  expected.AddQueryParameter("location", "useast");
  expected.AddQueryParameter("startIndex", "1");
  expected.AddQueryParameter("maxResults", "10");
  expected.AddQueryParameter("timeoutMs", "30");

  EXPECT_EQ(expected, *actual);
}

TEST(GetQueryResultsRequestTest, SuccessWithoutQueryParameters) {
  GetQueryResultsRequest request("1", "2");

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/queries/2");
  expected.AddQueryParameter("startIndex", "0");

  EXPECT_EQ(expected, *actual);
}

TEST(GetQueryResultsRequestTest, DebugString) {
  auto request = MakeFullGetQueryResultsRequest();

  EXPECT_EQ(request.DebugString("GetQueryResultsRequest", TracingOptions{}),
            R"(GetQueryResultsRequest {)"
            R"( project_id: "1" job_id: "2")"
            R"( page_token: "npt123" location: "useast")"
            R"( start_index: 1 max_results: 10)"
            R"( timeout { "30ms" })"
            R"( })");

  EXPECT_EQ(request.DebugString("GetQueryResultsRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(GetQueryResultsRequest {)"
            R"( project_id: "1" job_id: "2" page_token: "npt123")"
            R"( location: "useast" start_index: 1 max_results: 10)"
            R"( timeout { "30ms" })"
            R"( })");

  EXPECT_EQ(
      request.DebugString("GetQueryResultsRequest",
                          TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(GetQueryResultsRequest {
  project_id: "1"
  job_id: "2"
  page_token: "npt123"
  location: "useast"
  start_index: 1
  max_results: 10
  timeout {
    "30ms"
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
