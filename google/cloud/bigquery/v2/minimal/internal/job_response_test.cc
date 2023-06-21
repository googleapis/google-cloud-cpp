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

#include "google/cloud/bigquery/v2/minimal/internal/job_response.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MakeJob;

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(GetJobResponseTest, SuccessTopLevelFields) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {},
          "reference": {},
          "configuration": {}})";
  auto const job_response =
      GetJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(job_response);
  EXPECT_FALSE(job_response->http_response.payload.empty());
  EXPECT_THAT(job_response->job.kind, Eq("jkind"));
  EXPECT_THAT(job_response->job.etag, Eq("jtag"));
  EXPECT_THAT(job_response->job.id, Eq("j123"));
  EXPECT_THAT(job_response->job.self_link, Eq("jselfLink"));
  EXPECT_THAT(job_response->job.user_email, Eq("juserEmail"));
  EXPECT_THAT(job_response->job.status.state, IsEmpty());
  EXPECT_THAT(job_response->job.reference.project_id, IsEmpty());
  EXPECT_THAT(job_response->job.reference.job_id, IsEmpty());
  EXPECT_THAT(job_response->job.configuration.job_type, IsEmpty());
}

TEST(GetJobResponseTest, SuccessNestedFields) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "reference": {"project_id": "p123", "job_id": "j123"},
          "configuration": {
            "job_type": "QUERY",
            "query_config": {"query": "select 1;"}
          }})";
  auto const job_response =
      GetJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(job_response);
  EXPECT_FALSE(job_response->http_response.payload.empty());
  EXPECT_THAT(job_response->job.kind, Eq("jkind"));
  EXPECT_THAT(job_response->job.etag, Eq("jtag"));
  EXPECT_THAT(job_response->job.id, Eq("j123"));
  EXPECT_THAT(job_response->job.self_link, Eq("jselfLink"));
  EXPECT_THAT(job_response->job.user_email, Eq("juserEmail"));
  EXPECT_THAT(job_response->job.status.state, Eq("DONE"));
  EXPECT_THAT(job_response->job.reference.project_id, Eq("p123"));
  EXPECT_THAT(job_response->job.reference.job_id, Eq("j123"));
  EXPECT_THAT(job_response->job.configuration.job_type, Eq("QUERY"));
  EXPECT_THAT(job_response->job.configuration.query_config.query,
              Eq("select 1;"));
}

TEST(GetJobResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const job_response =
      GetJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(job_response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Empty payload in HTTP response")));
}

TEST(GetJobResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const job_response =
      GetJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(job_response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(GetJobResponseTest, InvalidJob) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail"})";
  auto const job_response =
      GetJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(job_response, StatusIs(StatusCode::kInternal,
                                     HasSubstr("Not a valid Json Job object")));
}

TEST(ListJobsResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "next_page_token": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "reference": {"project_id": "p123", "job_id": "j123"},
                "state": "DONE",
                "configuration": {
                   "job_type": "QUERY",
                   "query_config": {"query": "select 1;"}
                },
                "status": {"state": "DONE"},
                "user_email": "user-email",
                "principal_subject": "principal-subj"
              }
  ]})";
  auto const list_jobs_response =
      ListJobsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_jobs_response);
  EXPECT_FALSE(list_jobs_response->http_response.payload.empty());
  EXPECT_EQ(list_jobs_response->kind, "kind-1");
  EXPECT_EQ(list_jobs_response->etag, "tag-1");
  EXPECT_EQ(list_jobs_response->next_page_token, "npt-123");

  auto const jobs = list_jobs_response->jobs;
  ASSERT_EQ(jobs.size(), 1);
  EXPECT_EQ(jobs[0].id, "1");
  EXPECT_EQ(jobs[0].kind, "kind-2");
  EXPECT_EQ(jobs[0].status.state, "DONE");
  EXPECT_EQ(jobs[0].state, "DONE");
  EXPECT_EQ(jobs[0].user_email, "user-email");
  EXPECT_EQ(jobs[0].reference.project_id, "p123");
  EXPECT_EQ(jobs[0].reference.job_id, "j123");
  EXPECT_EQ(jobs[0].configuration.job_type, "QUERY");
  EXPECT_EQ(jobs[0].configuration.query_config.query, "select 1;");
}

TEST(ListJobsResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response = ListJobsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Empty payload in HTTP response")));
}

TEST(ListJobsResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Invalid";
  auto const response = ListJobsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListJobsResponseTest, InvalidJobList) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag"})";
  auto const response = ListJobsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json JobList object")));
}

TEST(ListJobsResponseTest, InvalidListFormatJob) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "next_page_token": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2"
              }
  ]})";
  auto const response = ListJobsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Not a valid Json ListFormatJob object")));
}

TEST(GetJobResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "reference": {"project_id": "p123", "job_id": "j123"},
          "configuration": {
            "job_type": "QUERY",
            "query_config": {"query": "select 1;"}
          }})";
  auto response = GetJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("GetJobResponse", TracingOptions{}),
      R"(GetJobResponse { http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLink" id: "j123")"
      R"( configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout_ms: 0 query_config { query: "select 1;")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false continuous: false maximum_bytes_billed: 0)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning {)"
      R"( type: "" expiration_time { "0" } field: "" } range_partitioning {)"
      R"( field: "" range { start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout_ms: 0 statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables {)"
      R"( values { } } } } reference { project_id: "p123" job_id: "j123")"
      R"( location: "" } status { state: "DONE" error_result { reason: "")"
      R"( location: "" message: "" } } statistics { creation_time { "0" })"
      R"( start_time { "0" } end_time { "0" } total_slot_time { "0" })"
      R"( final_execution_duration { "0" } total_bytes_processed: 0)"
      R"( num_child_jobs: 0 total_modified_partitions: 0)"
      R"( row_level_security_applied: false data_masking_applied: false)"
      R"( completion_ratio: 0 parent_job_id: "" session_id: "")"
      R"( transaction_id: "" reservation_id: "" script_statistics {)"
      R"( evaluation_kind { value: "" } } job_query_stats {)"
      R"( estimated_bytes_processed: 0 total_partitions_processed: 0)"
      R"( total_bytes_processed: 0 total_bytes_billed: 0 billing_tier: 0)"
      R"( num_dml_affected_rows: 0 ddl_affected_row_access_policy_count: 0)"
      R"( total_bytes_processed_accuracy: "" statement_type: "")"
      R"( ddl_operation_performed: "" total_slot_time { "0" })"
      R"( cache_hit: false schema { } dml_stats { inserted_row_count: 0)"
      R"( deleted_row_count: 0 updated_row_count: 0 } ddl_target_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } ddl_destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" })"
      R"( ddl_target_row_access_policy { project_id: "")"
      R"( dataset_id: "" table_id: "" policy_id: "" })"
      R"( ddl_target_routine { project_id: "" dataset_id: "")"
      R"( routine_id: "" } ddl_target_dataset { project_id: "")"
      R"( dataset_id: "" } dcl_target_table { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_view { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_dataset { project_id: "" dataset_id: "" })"
      R"( search_statistics { index_usage_mode { value: "" } })"
      R"( performance_insights { avg_previous_execution_time { "0" })"
      R"( stage_performance_standalone_insights { stage_id: 0 )"
      R"(slot_contention: false insufficient_shuffle_quota: false })"
      R"( stage_performance_change_insights { stage_id: 0 input_data_change {)"
      R"( records_read_diff_percentage: 0 } } } materialized_view_statistics { })"
      R"( metadata_cache_statistics { } } } } })");

  EXPECT_EQ(
      response->DebugString(
          "GetJobResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(GetJobResponse { http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLi...<truncated>...")"
      R"( id: "j123" configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout_ms: 0 query_config { query: "select ...<truncated>...")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false continuous: false maximum_bytes_billed: 0)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning { field: "" range {)"
      R"( start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout_ms: 0 statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables { values { } } } })"
      R"( reference { project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "0" } start_time { "0" } end_time { "0" })"
      R"( total_slot_time { "0" } final_execution_duration { "0" })"
      R"( total_bytes_processed: 0 num_child_jobs: 0 total_modified_partitions: 0)"
      R"( row_level_security_applied: false data_masking_applied: false)"
      R"( completion_ratio: 0 parent_job_id: "" session_id: "" transaction_id: "")"
      R"( reservation_id: "" script_statistics { evaluation_kind { value: "" } })"
      R"( job_query_stats { estimated_bytes_processed: 0 total_partitions_processed: 0)"
      R"( total_bytes_processed: 0 total_bytes_billed: 0 billing_tier: 0)"
      R"( num_dml_affected_rows: 0 ddl_affected_row_access_policy_count: 0)"
      R"( total_bytes_processed_accuracy: "" statement_type: "")"
      R"( ddl_operation_performed: "" total_slot_time { "0" })"
      R"( cache_hit: false schema { } dml_stats { inserted_row_count: 0)"
      R"( deleted_row_count: 0 updated_row_count: 0 } ddl_target_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } ddl_destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" })"
      R"( ddl_target_row_access_policy { project_id: "" dataset_id: "")"
      R"( table_id: "" policy_id: "" } ddl_target_routine { project_id: "")"
      R"( dataset_id: "" routine_id: "" } ddl_target_dataset { project_id: "")"
      R"( dataset_id: "" } dcl_target_table { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_view { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_dataset { project_id: "" dataset_id: "" })"
      R"( search_statistics { index_usage_mode { value: "" } })"
      R"( performance_insights { avg_previous_execution_time { "0" })"
      R"( stage_performance_standalone_insights { stage_id: 0)"
      R"( slot_contention: false insufficient_shuffle_quota: false })"
      R"( stage_performance_change_insights { stage_id: 0 input_data_change {)"
      R"( records_read_diff_percentage: 0 } } })"
      R"( materialized_view_statistics { })"
      R"( metadata_cache_statistics { } } } } })");

  EXPECT_EQ(response->DebugString("GetJobResponse", TracingOptions{}.SetOptions(
                                                        "single_line_mode=F")),
            R"(GetJobResponse {
  http_response {
    status_code: 200
    http_headers {
      key: "header1"
      value: "value1"
    }
    payload: REDACTED
  }
  job {
    etag: "jtag"
    kind: "jkind"
    self_link: "jselfLink"
    id: "j123"
    configuration {
      job_type: "QUERY"
      dry_run: false
      job_timeout_ms: 0
      query_config {
        query: "select 1;"
        create_disposition: ""
        write_disposition: ""
        priority: ""
        parameter_mode: ""
        preserve_nulls: false
        allow_large_results: false
        use_query_cache: false
        flatten_results: false
        use_legacy_sql: false
        create_session: false
        continuous: false
        maximum_bytes_billed: 0
        default_dataset {
          project_id: ""
          dataset_id: ""
        }
        destination_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        time_partitioning {
          type: ""
          expiration_time {
            "0"
          }
          field: ""
        }
        range_partitioning {
          field: ""
          range {
            start: ""
            end: ""
            interval: ""
          }
        }
        clustering {
        }
        destination_encryption_configuration {
          kms_key_name: ""
        }
        script_options {
          statement_timeout_ms: 0
          statement_byte_budget: 0
          key_result_statement {
            value: ""
          }
        }
        system_variables {
          values {
          }
        }
      }
    }
    reference {
      project_id: "p123"
      job_id: "j123"
      location: ""
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
        "0"
      }
      start_time {
        "0"
      }
      end_time {
        "0"
      }
      total_slot_time {
        "0"
      }
      final_execution_duration {
        "0"
      }
      total_bytes_processed: 0
      num_child_jobs: 0
      total_modified_partitions: 0
      row_level_security_applied: false
      data_masking_applied: false
      completion_ratio: 0
      parent_job_id: ""
      session_id: ""
      transaction_id: ""
      reservation_id: ""
      script_statistics {
        evaluation_kind {
          value: ""
        }
      }
      job_query_stats {
        estimated_bytes_processed: 0
        total_partitions_processed: 0
        total_bytes_processed: 0
        total_bytes_billed: 0
        billing_tier: 0
        num_dml_affected_rows: 0
        ddl_affected_row_access_policy_count: 0
        total_bytes_processed_accuracy: ""
        statement_type: ""
        ddl_operation_performed: ""
        total_slot_time {
          "0"
        }
        cache_hit: false
        schema {
        }
        dml_stats {
          inserted_row_count: 0
          deleted_row_count: 0
          updated_row_count: 0
        }
        ddl_target_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        ddl_destination_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        ddl_target_row_access_policy {
          project_id: ""
          dataset_id: ""
          table_id: ""
          policy_id: ""
        }
        ddl_target_routine {
          project_id: ""
          dataset_id: ""
          routine_id: ""
        }
        ddl_target_dataset {
          project_id: ""
          dataset_id: ""
        }
        dcl_target_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        dcl_target_view {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        dcl_target_dataset {
          project_id: ""
          dataset_id: ""
        }
        search_statistics {
          index_usage_mode {
            value: ""
          }
        }
        performance_insights {
          avg_previous_execution_time {
            "0"
          }
          stage_performance_standalone_insights {
            stage_id: 0
            slot_contention: false
            insufficient_shuffle_quota: false
          }
          stage_performance_change_insights {
            stage_id: 0
            input_data_change {
              records_read_diff_percentage: 0
            }
          }
        }
        materialized_view_statistics {
        }
        metadata_cache_statistics {
        }
      }
    }
  }
})");
}

TEST(ListJobsResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "next_page_token": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "reference": {"project_id": "p123", "job_id": "j123"},
                "state": "DONE",
                "configuration": {
                   "job_type": "QUERY",
                   "query_config": {"query": "select 1;"}
                },
                "status": {"state": "DONE"},
                "user_email": "user-email",
                "principal_subject": "principal-subj"
              }
  ]})";
  auto response = ListJobsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("ListJobsResponse", TracingOptions{}),
      R"(ListJobsResponse { jobs { id: "1" kind: "kind-2" state: "DONE")"
      R"( configuration { job_type: "QUERY" dry_run: false job_timeout_ms: 0)"
      R"( query_config { query: "select 1;" create_disposition: "")"
      R"( write_disposition: "" priority: "" parameter_mode: "")"
      R"( preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false)"
      R"( use_legacy_sql: false create_session: false continuous: false)"
      R"( maximum_bytes_billed: 0 default_dataset { project_id: "")"
      R"( dataset_id: "" } destination_table { project_id: "")"
      R"( dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning {)"
      R"( field: "" range { start: "" end: "" interval: "" } })"
      R"( clustering { } destination_encryption_configuration {)"
      R"( kms_key_name: "" } script_options { statement_timeout_ms: 0)"
      R"( statement_byte_budget: 0 key_result_statement { value: "" } })"
      R"( system_variables { values { } } } } reference {)"
      R"( project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "")"
      R"( message: "" } } statistics { creation_time { "0" })"
      R"( start_time { "0" } end_time { "0" } total_slot_time { "0" })"
      R"( final_execution_duration { "0" } total_bytes_processed: 0)"
      R"( num_child_jobs: 0 total_modified_partitions: 0)"
      R"( row_level_security_applied: false data_masking_applied: false)"
      R"( completion_ratio: 0 parent_job_id: "" session_id: "")"
      R"( transaction_id: "" reservation_id: "" script_statistics {)"
      R"( evaluation_kind { value: "" } } job_query_stats {)"
      R"( estimated_bytes_processed: 0 total_partitions_processed: 0)"
      R"( total_bytes_processed: 0 total_bytes_billed: 0 billing_tier: 0)"
      R"( num_dml_affected_rows: 0 ddl_affected_row_access_policy_count: 0)"
      R"( total_bytes_processed_accuracy: "" statement_type: "")"
      R"( ddl_operation_performed: "" total_slot_time { "0" })"
      R"( cache_hit: false schema { } dml_stats { inserted_row_count: 0)"
      R"( deleted_row_count: 0 updated_row_count: 0 } ddl_target_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } ddl_destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" })"
      R"( ddl_target_row_access_policy { project_id: "" dataset_id: "")"
      R"( table_id: "" policy_id: "" } ddl_target_routine { project_id: "")"
      R"( dataset_id: "" routine_id: "" } ddl_target_dataset { project_id: "")"
      R"( dataset_id: "" } dcl_target_table { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_view { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_dataset { project_id: "" dataset_id: "" })"
      R"( search_statistics { index_usage_mode { value: "" } })"
      R"( performance_insights { avg_previous_execution_time { "0" })"
      R"( stage_performance_standalone_insights { stage_id: 0 slot_contention: false)"
      R"( insufficient_shuffle_quota: false } stage_performance_change_insights {)"
      R"( stage_id: 0 input_data_change { records_read_diff_percentage: 0 } } })"
      R"( materialized_view_statistics { } metadata_cache_statistics { } } })"
      R"( error_result { reason: "" location: "" message: "" } })"
      R"( next_page_token: "npt-123" kind: "kind-1" etag: "tag-1")"
      R"( http_response { status_code: 200 http_headers { key: "header1")"
      R"( value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString(
          "ListJobsResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(ListJobsResponse { jobs { id: "1" kind: "kind-2" state: "DONE")"
      R"( configuration { job_type: "QUERY" dry_run: false job_timeout_ms: 0)"
      R"( query_config { query: "select ...<truncated>..." create_disposition: "")"
      R"( write_disposition: "" priority: "" parameter_mode: "")"
      R"( preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false continuous: false maximum_bytes_billed: 0)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning { field: "" range {)"
      R"( start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout_ms: 0 statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables { values { } } } })"
      R"( reference { project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "0" } start_time { "0" } end_time { "0" })"
      R"( total_slot_time { "0" } final_execution_duration { "0" })"
      R"( total_bytes_processed: 0 num_child_jobs: 0 total_modified_partitions: 0)"
      R"( row_level_security_applied: false data_masking_applied: false)"
      R"( completion_ratio: 0 parent_job_id: "" session_id: "" transaction_id: "")"
      R"( reservation_id: "" script_statistics { evaluation_kind { value: "" } })"
      R"( job_query_stats { estimated_bytes_processed: 0)"
      R"( total_partitions_processed: 0 total_bytes_processed: 0)"
      R"( total_bytes_billed: 0 billing_tier: 0 num_dml_affected_rows: 0)"
      R"( ddl_affected_row_access_policy_count: 0)"
      R"( total_bytes_processed_accuracy: "" statement_type: "")"
      R"( ddl_operation_performed: "" total_slot_time { "0" } cache_hit: false)"
      R"( schema { } dml_stats { inserted_row_count: 0 deleted_row_count: 0)"
      R"( updated_row_count: 0 } ddl_target_table { project_id: "")"
      R"( dataset_id: "" table_id: "" } ddl_destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" })"
      R"( ddl_target_row_access_policy { project_id: "" dataset_id: "")"
      R"( table_id: "" policy_id: "" } ddl_target_routine { project_id: "")"
      R"( dataset_id: "" routine_id: "" } ddl_target_dataset { project_id: "")"
      R"( dataset_id: "" } dcl_target_table { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_view { project_id: "" dataset_id: "")"
      R"( table_id: "" } dcl_target_dataset { project_id: "" dataset_id: "" })"
      R"( search_statistics { index_usage_mode { value: "" } })"
      R"( performance_insights { avg_previous_execution_time { "0" })"
      R"( stage_performance_standalone_insights { stage_id: 0)"
      R"( slot_contention: false insufficient_shuffle_quota: false })"
      R"( stage_performance_change_insights { stage_id: 0 input_data_change {)"
      R"( records_read_diff_percentage: 0 } } } materialized_view_statistics { })"
      R"( metadata_cache_statistics { } } } error_result { reason: "" location: "")"
      R"( message: "" } } next_page_token: "npt-123" kind: "kind-1")"
      R"( etag: "tag-1" http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString("ListJobsResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListJobsResponse {
  jobs {
    id: "1"
    kind: "kind-2"
    state: "DONE"
    configuration {
      job_type: "QUERY"
      dry_run: false
      job_timeout_ms: 0
      query_config {
        query: "select 1;"
        create_disposition: ""
        write_disposition: ""
        priority: ""
        parameter_mode: ""
        preserve_nulls: false
        allow_large_results: false
        use_query_cache: false
        flatten_results: false
        use_legacy_sql: false
        create_session: false
        continuous: false
        maximum_bytes_billed: 0
        default_dataset {
          project_id: ""
          dataset_id: ""
        }
        destination_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        time_partitioning {
          type: ""
          expiration_time {
            "0"
          }
          field: ""
        }
        range_partitioning {
          field: ""
          range {
            start: ""
            end: ""
            interval: ""
          }
        }
        clustering {
        }
        destination_encryption_configuration {
          kms_key_name: ""
        }
        script_options {
          statement_timeout_ms: 0
          statement_byte_budget: 0
          key_result_statement {
            value: ""
          }
        }
        system_variables {
          values {
          }
        }
      }
    }
    reference {
      project_id: "p123"
      job_id: "j123"
      location: ""
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
        "0"
      }
      start_time {
        "0"
      }
      end_time {
        "0"
      }
      total_slot_time {
        "0"
      }
      final_execution_duration {
        "0"
      }
      total_bytes_processed: 0
      num_child_jobs: 0
      total_modified_partitions: 0
      row_level_security_applied: false
      data_masking_applied: false
      completion_ratio: 0
      parent_job_id: ""
      session_id: ""
      transaction_id: ""
      reservation_id: ""
      script_statistics {
        evaluation_kind {
          value: ""
        }
      }
      job_query_stats {
        estimated_bytes_processed: 0
        total_partitions_processed: 0
        total_bytes_processed: 0
        total_bytes_billed: 0
        billing_tier: 0
        num_dml_affected_rows: 0
        ddl_affected_row_access_policy_count: 0
        total_bytes_processed_accuracy: ""
        statement_type: ""
        ddl_operation_performed: ""
        total_slot_time {
          "0"
        }
        cache_hit: false
        schema {
        }
        dml_stats {
          inserted_row_count: 0
          deleted_row_count: 0
          updated_row_count: 0
        }
        ddl_target_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        ddl_destination_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        ddl_target_row_access_policy {
          project_id: ""
          dataset_id: ""
          table_id: ""
          policy_id: ""
        }
        ddl_target_routine {
          project_id: ""
          dataset_id: ""
          routine_id: ""
        }
        ddl_target_dataset {
          project_id: ""
          dataset_id: ""
        }
        dcl_target_table {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        dcl_target_view {
          project_id: ""
          dataset_id: ""
          table_id: ""
        }
        dcl_target_dataset {
          project_id: ""
          dataset_id: ""
        }
        search_statistics {
          index_usage_mode {
            value: ""
          }
        }
        performance_insights {
          avg_previous_execution_time {
            "0"
          }
          stage_performance_standalone_insights {
            stage_id: 0
            slot_contention: false
            insufficient_shuffle_quota: false
          }
          stage_performance_change_insights {
            stage_id: 0
            input_data_change {
              records_read_diff_percentage: 0
            }
          }
        }
        materialized_view_statistics {
        }
        metadata_cache_statistics {
        }
      }
    }
    error_result {
      reason: ""
      location: ""
      message: ""
    }
  }
  next_page_token: "npt-123"
  kind: "kind-1"
  etag: "tag-1"
  http_response {
    status_code: 200
    http_headers {
      key: "header1"
      value: "value1"
    }
    payload: REDACTED
  }
})");
}

TEST(InsertJobResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"configuration":{"dry_run":true,"job_timeout_ms":10)"
      R"(,"job_type":"QUERY")"
      R"(,"labels":{"label-key1":"label-val1"})"
      R"(,"query_config":{"allow_large_results":true)"
      R"(,"clustering":{"fields":["clustering-field-1")"
      R"(,"clustering-field-2"]})"
      R"(,"connection_properties":[{"key":"conn-prop-key")"
      R"(,"value":"conn-prop-val"}])"
      R"(,"continuous":true,"create_disposition":"job-create-disposition")"
      R"(,"create_session":true,"default_dataset":{"dataset_id":"1")"
      R"(,"project_id":"2"},"destination_encryption_configuration":{)"
      R"("kms_key_name":"encryption-key-name"},"destination_table":{")"
      R"(dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"flatten_results":true)"
      R"(,"maximum_bytes_billed":0,"parameter_mode":"job-param-mode")"
      R"(,"preserve_nulls":true,"priority":"job-priority","query":"select 1;")"
      R"(,"query_parameters":[{"name":"query-parameter-name","parameter_type":{)"
      R"("array_type":{"struct_types":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"struct_types":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"struct_types":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"struct_types":[])"
      R"(,"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameter_value":{)"
      R"("array_values":[{"array_values":[{"array_values":[],"struct_values":{)"
      R"("array-map-key":{"array_values":[],"struct_values":{})"
      R"(,"value":"array-map-value"}})"
      R"(,"value":"array-val-2"}],"struct_values":{},"value":"array-val-1"}])"
      R"(,"struct_values":{"qp-map-key":{"array_values":[],"struct_values":{})"
      R"(,"value":"qp-map-value"}},"value":"query-parameter-value"}}])"
      R"(,"range_partitioning":{"field":"rp-field-1","range":{"end":"range-end")"
      R"(,"interval":"range-interval","start":"range-start"}})"
      R"(,"schema_update_options":["job-update-options"])"
      R"(,"script_options":{)"
      R"("key_result_statement":{"value":"FIRST_SELECT"})"
      R"(,"statement_byte_budget":10)"
      R"(,"statement_timeout_ms":10},"system_variables":{"types":{)"
      R"("sql-struct-type-key-1":{"sub_type":{"fields":[{)"
      R"("name":"f1-sql-struct-type-int64"}]})"
      R"(,"sub_type_index":2,"type_kind":{"value":"INT64"}})"
      R"(,"sql-struct-type-key-2":{"sub_type":{"fields":[{)"
      R"("name":"f2-sql-struct-type-string"}]},"sub_type_index":2)"
      R"(,"type_kind":{"value":"STRING"}},"sql-struct-type-key-3":{)"
      R"("sub_type":{"sub_type":{"fields":[{"name":"f2-sql-struct-type-string"}]})"
      R"(,"sub_type_index":2,"type_kind":{"value":"STRING"}},"sub_type_index":1)"
      R"(,"type_kind":{"value":"STRING"}}},"values":{"fields":{"bool-key":{)"
      R"("kind_index":3,"value_kind":true},"double-key":{"kind_index":1)"
      R"(,"value_kind":3.4},"string-key":{"kind_index":2,"value_kind":"val3"}}}})"
      R"(,"time_partitioning":{"expiration_time":0,"field":"tp-field-1")"
      R"(,"type":"tp-field-type"},"use_legacy_sql":true,"use_query_cache":true)"
      R"(,"write_disposition":"job-write-disposition"}},"etag":"etag","id":"1")"
      R"(,"kind":"Job","reference":{"job_id":"2","location":"us-east")"
      R"(,"project_id":"1"},"self_link":"self-link","statistics":{)"
      R"("completion_ratio":1234.1234,"creation_time":10,"data_masking_applied":true)"
      R"(,"end_time":10,"final_execution_duration":10,"job_query_stats":{)"
      R"("billing_tier":1234,"cache_hit":true,"dcl_target_dataset":{"dataset_id":"1")"
      R"(,"project_id":"2"},"dcl_target_table":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"},"dcl_target_view":{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"},"ddl_affected_row_access_policy_count":1234)"
      R"(,"ddl_destination_table":{"dataset_id":"1","project_id":"2","table_id":"3"})"
      R"(,"ddl_operation_performed":"ddl_operation_performed")"
      R"(,"ddl_target_dataset":{"dataset_id":"1","project_id":"2"})"
      R"(,"ddl_target_routine":{"dataset_id":"1","project_id":"2","routine_id":"3"})"
      R"(,"ddl_target_row_access_policy":{"dataset_id":"1","policy_id":"3")"
      R"(,"project_id":"1234","table_id":"2"},"ddl_target_table":{"dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"},"dml_stats":{"deleted_row_count":1234)"
      R"(,"inserted_row_count":1234,"updated_row_count":1234})"
      R"(,"estimated_bytes_processed":1234,"materialized_view_statistics":{)"
      R"("materialized_view":[{"chosen":true,"estimated_bytes_saved":1234)"
      R"(,"rejected_reason":{"value":"BASE_TABLE_DATA_CHANGE"},"table_reference":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"}}]})"
      R"(,"metadata_cache_statistics":{"table_metadata_cache_usage":[{)"
      R"("explanation":"test-table-metadata","table_reference":{"dataset_id":"1")"
      R"(,"project_id":"2","table_id":"3"},"unused_reason":{)"
      R"("value":"EXCEEDED_MAX_STALENESS"}}]},"num_dml_affected_rows":1234)"
      R"(,"performance_insights":{"avg_previous_execution_time":10)"
      R"(,"stage_performance_change_insights":{"input_data_change":{)"
      R"("records_read_diff_percentage":12.119999885559082},"stage_id":1234})"
      R"(,"stage_performance_standalone_insights":{"insufficient_shuffle_quota":true)"
      R"(,"slot_contention":true,"stage_id":1234}},"query_plan":[{)"
      R"("completed_parallel_inputs":1234,"compute_avg_time_spent":10)"
      R"(,"compute_max_time_spent":10,"compute_mode":{"value":"BIGQUERY"})"
      R"(,"compute_ratio_avg":1234.1234,"compute_ratio_max":1234.1234,"end_time":10)"
      R"(,"id":1234,"input_stages":[1234],"name":"test-explain")"
      R"(,"parallel_inputs":1234,"read_avg_time_spent":10,"read_max_time_spent":10)"
      R"(,"read_ratio_avg":1234.1234,"read_ratio_max":1234.1234,"records_read":1234)"
      R"(,"records_written":1234,"shuffle_output_bytes":1234)"
      R"(,"shuffle_output_bytes_spilled":1234,"slot_time":10)"
      R"(,"start_time":10,"status":"explain-status","steps":[{)"
      R"("kind":"sub-step-kind","substeps":["sub-step-1"]}])"
      R"(,"wait_avg_time_spent":10)"
      R"(,"wait_max_time_spent":10,"wait_ratio_avg":1234.1234)"
      R"(,"wait_ratio_max":1234.1234,"write_avg_time_spent":10)"
      R"(,"write_max_time_spent":10,"write_ratio_avg":1234.1234)"
      R"(,"write_ratio_max":1234.1234}],"referenced_routines":[{)"
      R"("dataset_id":"1","project_id":"2","routine_id":"3"}])"
      R"(,"referenced_tables":[{"dataset_id":"1","project_id":"2")"
      R"(,"table_id":"3"}],"schema":{"fields":[{"categories":{)"
      R"("names":[]},"collation":"","data_classification_tags":{)"
      R"("names":[]},"default_value_expression":"")"
      R"(,"description":"","fields":{"fields":[]})"
      R"(,"is_measure":true,"max_length":0,"mode":"fmode","name":"fname-1")"
      R"(,"policy_tags":{"names":[]},"precision":0,"range_element_type":{)"
      R"("type":""},"rounding_mode":{"value":""},"scale":0,"type":""}]})"
      R"(,"search_statistics":{"index_unused_reasons":[{"base_table":{)"
      R"("dataset_id":"1","project_id":"2","table_id":"3"},"code":{)"
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
      R"("name":"query-parameter-name","parameter_type":{"array_type":{)"
      R"("struct_types":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"struct_types":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"struct_types":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"struct_types":[])"
      R"(,"type":"qp-struct-type"}}],"type":"query-parameter-type"})"
      R"(,"parameter_value":{"array_values":[{"array_values":[{"array_values":[])"
      R"(,"struct_values":{"array-map-key":{"array_values":[],"struct_values":{})"
      R"(,"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"struct_values":{},"value":"array-val-1"}])"
      R"(,"struct_values":{"qp-map-key":{"array_values":[],"struct_values":{})"
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
      R"(,"transaction_id":"transaction-id-123"},"status":{"error_result":{)"
      R"("location":"","message":"","reason":""},"errors":[])"
      R"(,"state":"DONE"},"user_email":"a@b.com"})";

  auto const insert_job_response =
      InsertJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(insert_job_response);
  EXPECT_FALSE(insert_job_response->http_response.payload.empty());

  auto expected_job = MakeJob();
  auto actual_job = insert_job_response->job;

  bigquery_v2_minimal_testing::AssertEquals(expected_job, actual_job);
}

TEST(InsertJobResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response = InsertJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Empty payload in HTTP response")));
}

TEST(InsertJobResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const response = InsertJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(InsertJobResponseTest, InvalidJob) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail"})";
  auto const response = InsertJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json Job object")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
