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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
