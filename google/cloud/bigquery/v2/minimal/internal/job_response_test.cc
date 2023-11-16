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
#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MakeGetQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::
    MakeGetQueryResultsResponsePayload;
using ::google::cloud::bigquery_v2_minimal_testing::MakeJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakePostQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryResponsePayload;

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
          "selfLink": "jselfLink",
          "user_email": "juserEmail",
          "status": {},
          "jobReference": {},
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
  EXPECT_THAT(job_response->job.job_reference.project_id, IsEmpty());
  EXPECT_THAT(job_response->job.job_reference.job_id, IsEmpty());
  EXPECT_THAT(job_response->job.configuration.job_type, IsEmpty());
}

TEST(GetJobResponseTest, SuccessNestedFields) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "jobReference": {"projectId": "p123", "jobId": "j123"},
          "configuration": {
            "jobType": "QUERY",
            "query": {"query": "select 1;"}
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
  EXPECT_THAT(job_response->job.job_reference.project_id, Eq("p123"));
  EXPECT_THAT(job_response->job.job_reference.job_id, Eq("j123"));
  EXPECT_THAT(job_response->job.configuration.job_type, Eq("QUERY"));
  EXPECT_THAT(job_response->job.configuration.query.query, Eq("select 1;"));
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
          "selfLink": "jselfLink",
          "user_email": "juserEmail"})";
  auto const job_response =
      GetJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(job_response, StatusIs(StatusCode::kInternal,
                                     HasSubstr("Not a valid Json Job object")));
}

TEST(ListJobsResponseTest, SuccessMultiplePages) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "jobReference": {"projectId": "p123", "jobId": "j123"},
                "state": "DONE",
                "configuration": {
                   "jobType": "QUERY",
                   "query": {"query": "select 1;"}
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
  EXPECT_EQ(jobs[0].job_reference.project_id, "p123");
  EXPECT_EQ(jobs[0].job_reference.job_id, "j123");
  EXPECT_EQ(jobs[0].configuration.job_type, "QUERY");
  EXPECT_EQ(jobs[0].configuration.query.query, "select 1;");
}

TEST(ListJobsResponseTest, SuccessSinglePage) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "jobReference": {"projectId": "p123", "jobId": "j123"},
                "state": "DONE",
                "configuration": {
                   "jobType": "QUERY",
                   "query": {"query": "select 1;"}
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
  EXPECT_THAT(list_jobs_response->next_page_token, IsEmpty());

  auto const jobs = list_jobs_response->jobs;
  ASSERT_EQ(jobs.size(), 1);
  EXPECT_EQ(jobs[0].id, "1");
  EXPECT_EQ(jobs[0].kind, "kind-2");
  EXPECT_EQ(jobs[0].status.state, "DONE");
  EXPECT_EQ(jobs[0].state, "DONE");
  EXPECT_EQ(jobs[0].user_email, "user-email");
  EXPECT_EQ(jobs[0].job_reference.project_id, "p123");
  EXPECT_EQ(jobs[0].job_reference.job_id, "j123");
  EXPECT_EQ(jobs[0].configuration.job_type, "QUERY");
  EXPECT_EQ(jobs[0].configuration.query.query, "select 1;");
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
          "nextPageToken": "npt-123",
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

TEST(ListJobsResponseTest, MinimalProjectionJson) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "jobReference": {"projectId": "p123", "jobId": "j123"},
                "state": "DONE",
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
  EXPECT_THAT(list_jobs_response->next_page_token, IsEmpty());

  auto const jobs = list_jobs_response->jobs;
  ASSERT_EQ(jobs.size(), 1);
  EXPECT_EQ(jobs[0].id, "1");
  EXPECT_EQ(jobs[0].kind, "kind-2");
  EXPECT_EQ(jobs[0].status.state, "DONE");
  EXPECT_EQ(jobs[0].state, "DONE");
  EXPECT_EQ(jobs[0].user_email, "user-email");
  EXPECT_EQ(jobs[0].job_reference.project_id, "p123");
  EXPECT_EQ(jobs[0].job_reference.job_id, "j123");
}

TEST(GetJobResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "jobReference": {"projectId": "p123", "jobId": "j123"},
          "configuration": {
            "jobType": "QUERY",
            "query": {"query": "select 1;"}
          }})";
  auto response = GetJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("GetJobResponse", TracingOptions{}),
      R"(GetJobResponse { http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLink" id: "j123")"
      R"( configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout { "0" } query_config { query: "select 1;")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning {)"
      R"( type: "" expiration_time { "0" } field: "" } range_partitioning {)"
      R"( field: "" range { start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables {)"
      R"( values { } } } } reference { project_id: "p123" job_id: "j123")"
      R"( location: "" } status { state: "DONE" error_result { reason: "")"
      R"( location: "" message: "" } } statistics { creation_time { "0" })"
      R"( start_time { "0" } end_time { "0" } total_slot_time { "0" })"
      R"( final_execution_duration { "0" } total_bytes_processed: 0)"
      R"( num_child_jobs: 0)"
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
      R"( job_timeout { "0" } query_config { query: "select ...<truncated>...")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning { field: "" range {)"
      R"( start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables { values { } } } })"
      R"( reference { project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "0" } start_time { "0" } end_time { "0" })"
      R"( total_slot_time { "0" } final_execution_duration { "0" })"
      R"( total_bytes_processed: 0 num_child_jobs: 0)"
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
      job_timeout {
        "0"
      }
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
        maximum_bytes_billed: -1
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
          statement_timeout {
            "0"
          }
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
          "nextPageToken": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "jobReference": {"projectId": "p123", "jobId": "j123"},
                "state": "DONE",
                "configuration": {
                   "jobType": "QUERY",
                   "query": {"query": "select 1;"}
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
      R"( configuration { job_type: "QUERY" dry_run: false job_timeout { "0" })"
      R"( query_config { query: "select 1;" create_disposition: "")"
      R"( write_disposition: "" priority: "" parameter_mode: "")"
      R"( preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false)"
      R"( use_legacy_sql: false create_session: false)"
      R"( maximum_bytes_billed: -1 default_dataset { project_id: "")"
      R"( dataset_id: "" } destination_table { project_id: "")"
      R"( dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning {)"
      R"( field: "" range { start: "" end: "" interval: "" } })"
      R"( clustering { } destination_encryption_configuration {)"
      R"( kms_key_name: "" } script_options { statement_timeout { "0" })"
      R"( statement_byte_budget: 0 key_result_statement { value: "" } })"
      R"( system_variables { values { } } } } reference {)"
      R"( project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "")"
      R"( message: "" } } statistics { creation_time { "0" })"
      R"( start_time { "0" } end_time { "0" } total_slot_time { "0" })"
      R"( final_execution_duration { "0" } total_bytes_processed: 0)"
      R"( num_child_jobs: 0)"
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
      R"( configuration { job_type: "QUERY" dry_run: false job_timeout { "0" })"
      R"( query_config { query: "select ...<truncated>..." create_disposition: "")"
      R"( write_disposition: "" priority: "" parameter_mode: "")"
      R"( preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning { field: "" range {)"
      R"( start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables { values { } } } })"
      R"( reference { project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "0" } start_time { "0" } end_time { "0" })"
      R"( total_slot_time { "0" } final_execution_duration { "0" })"
      R"( total_bytes_processed: 0 num_child_jobs: 0)"
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
      R"( dataset_id: "" table_id: "" })"
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
      job_timeout {
        "0"
      }
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
        maximum_bytes_billed: -1
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
          statement_timeout {
            "0"
          }
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
      R"({"configuration":{"dryRun":true,"jobTimeoutMs":"10","jobType":"QUERY")"
      R"(,"labels":{"label-key1":"label-val1"},"query":{"allowLargeResults":true)"
      R"(,"clustering":{"fields":["clustering-field-1","clustering-field-2"]})"
      R"(,"connectionProperties":[{"key":"conn-prop-key","value":"conn-prop-val"}])"
      R"(,"createDisposition":"job-create-disposition","createSession":true)"
      R"(,"defaultDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"destinationEncryptionConfiguration":{"kmsKeyName":"encryption-key-name"})"
      R"(,"destinationTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"flattenResults":true,"maximumBytesBilled":"0","parameterMode":"job-param-mode")"
      R"(,"preserveNulls":true,"priority":"job-priority","query":"select 1;")"
      R"(,"queryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"structTypes":[{"description":"qp-struct-description","name":"qp-struct-name")"
      R"(,"type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{"arrayValues":[{"arrayValues":[])"
      R"(,"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}])"
      R"(,"structValues":{"qp-map-key":{"arrayValues":[],"structValues":{})"
      R"(,"value":"qp-map-value"}},"value":"query-parameter-value"}}])"
      R"(,"rangePartitioning":{"field":"rp-field-1","range":{"end":"range-end")"
      R"(,"interval":"range-interval","start":"range-start"}})"
      R"(,"schemaUpdateOptions":["job-update-options"])"
      R"(,"scriptOptions":{"keyResultStatement":"FIRST_SELECT")"
      R"(,"statementByteBudget":"10","statementTimeoutMs":"10"})"
      R"(,"systemVariables":{"types":{"sql-struct-type-key-1":{)"
      R"("structType":{"fields":[{"name":"f1-sql-struct-type-int64"}]})"
      R"(,"sub_type_index":2,"typeKind":"INT64"})"
      R"(,"sql-struct-type-key-2":{"structType":{"fields":[{"name":"f2-sql-struct-type-string"}]})"
      R"(,"sub_type_index":2,"typeKind":"STRING"})"
      R"(,"sql-struct-type-key-3":{"arrayElementType":{"structType":{)"
      R"("fields":[{"name":"f2-sql-struct-type-string"}]},"sub_type_index":2)"
      R"(,"typeKind":"STRING"},"sub_type_index":1,"typeKind":"STRING"}})"
      R"(,"values":{"fields":{"bool-key":{"kind_index":3,"valueKind":true})"
      R"(,"double-key":{"kind_index":1,"valueKind":3.4},"string-key":{"kind_index":2,"valueKind":"val3"}}}})"
      R"(,"timePartitioning":{"expirationTime":"0","field":"tp-field-1","type":"tp-field-type"})"
      R"(,"useLegacySql":true,"useQueryCache":true,"writeDisposition":"job-write-disposition"}})"
      R"(,"etag":"etag","id":"1","jobReference":{"jobId":"2","location":"us-east","projectId":"1"})"
      R"(,"kind":"Job","selfLink":"self-link","statistics":{"completionRatio":1234.1234)"
      R"(,"creationTime":"10","dataMaskingStatistics":{"dataMaskingApplied":true},"endTime":"10")"
      R"(,"finalExecutionDurationMs":"10","numChildJobs":"1234","parentJobId":"parent-job-123")"
      R"(,"query":{"billingTier":1234,"cacheHit":true,"dclTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"dclTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dclTargetView":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"ddlAffectedRowAccessPolicyCount":"1234","ddlOperationPerformed":"ddl_operation_performed")"
      R"(,"ddlTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"ddlTargetRoutine":{"datasetId":"1","projectId":"2","routineId":"3"})"
      R"(,"ddlTargetRowAccessPolicy":{"datasetId":"1","policyId":"3","projectId":"1234","tableId":"2"})"
      R"(,"ddlTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dmlStats":{"deletedRowCount":"1234","insertedRowCount":"1234","updatedRowCount":"1234"})"
      R"(,"estimatedBytesProcessed":"1234","materializedViewStatistics":{"materializedView":[{"chosen":true)"
      R"(,"estimatedBytesSaved":"1234","rejectedReason":"BASE_TABLE_DATA_CHANGE")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"}}]})"
      R"(,"metadataCacheStatistics":{"tableMetadataCacheUsage":[{"explanation":"test-table-metadata")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"unusedReason":"EXCEEDED_MAX_STALENESS"}]})"
      R"(,"numDmlAffectedRows":"1234","performanceInsights":{"avgPreviousExecutionMs":"10")"
      R"(,"stagePerformanceChangeInsights":{"inputDataChange":{"recordsReadDiffPercentage":12.119999885559082})"
      R"(,"stageId":"1234"},"stagePerformanceStandaloneInsights":{"insufficientShuffleQuota":true)"
      R"(,"slotContention":true,"stageId":"1234"}},"queryPlan":[{"completedParallelInputs":"1234")"
      R"(,"computeMode":"BIGQUERY","computeMsAvg":"10","computeMsMax":"10")"
      R"(,"computeRatioAvg":1234.1234,"computeRatioMax":1234.1234,"endMs":"10")"
      R"(,"id":"1234","inputStages":["1234"],"name":"test-explain","parallelInputs":"1234")"
      R"(,"readMsAvg":"10","readMsMax":"10","readRatioAvg":1234.1234,"readRatioMax":1234.1234)"
      R"(,"recordsRead":"1234","recordsWritten":"1234","shuffleOutputBytes":"1234")"
      R"(,"shuffleOutputBytesSpilled":"1234","slotMs":"10","startMs":"10","status":"explain-status")"
      R"(,"steps":[{"kind":"sub-step-kind","substeps":["sub-step-1"]}])"
      R"(,"waitMsAvg":"10","waitMsMax":"10","waitRatioAvg":1234.1234)"
      R"(,"waitRatioMax":1234.1234,"writeMsAvg":"10","writeMsMax":"10")"
      R"(,"writeRatioAvg":1234.1234,"writeRatioMax":1234.1234}])"
      R"(,"referencedRoutines":[{"datasetId":"1","projectId":"2","routineId":"3"}])"
      R"(,"referencedTables":[{"datasetId":"1","projectId":"2","tableId":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]},"collation":"")"
      R"(,"defaultValueExpression":"","description":"","fields":{"fields":[]})"
      R"(,"maxLength":0,"mode":"fmode","name":"fname-1","policyTags":{"names":[]})"
      R"(,"precision":0,"rangeElementType":{"type":""},"roundingMode":"")"
      R"(,"scale":0,"type":""}]},"searchStatistics":{"indexUnusedReasons":[{)"
      R"("baseTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"code":"BASE_TABLE_TOO_SMALL","indexName":"test-index","message":""}])"
      R"(,"indexUsageMode":"PARTIALLY_USED"},"statementType":"statement_type")"
      R"(,"timeline":[{"activeUnits":"1234","completedUnits":"1234","elapsedMs":"10")"
      R"(,"estimatedRunnableUnits":"1234","pendingUnits":"1234","totalSlotMs":"10"}])"
      R"(,"totalBytesBilled":"1234","totalBytesProcessed":"1234",)"
      R"("totalBytesProcessedAccuracy":"total_bytes_processed_accuracy","totalPartitionsProcessed":"1234")"
      R"(,"totalSlotMs":"10","transferredBytes":"1234")"
      R"(,"undeclaredQueryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[],"type":"array-struct-type"}}])"
      R"(,"type":"array-type"},"structTypes":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{)"
      R"("arrayValues":[{"arrayValues":[],"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}],"structValues":{"qp-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"qp-map-value"}},"value":"query-parameter-value"}}]})"
      R"(,"quotaDeferments":["quota-defer-1"],"reservation_id":"reservation-id-123")"
      R"(,"rowLevelSecurityStatistics":{"rowLevelSecurityApplied":true})"
      R"(,"scriptStatistics":{"evaluationKind":"STATEMENT")"
      R"(,"stackFrames":[{"endColumn":1234,"endLine":1234,"procedureId":"proc-id")"
      R"(,"startColumn":1234,"startLine":1234,"text":"stack-frame-text"}]})"
      R"(,"sessionInfo":{"sessionId":"session-id-123"},"startTime":"10")"
      R"(,"totalBytesProcessed":"1234","totalSlotMs":"10","transactionInfo":{"transactionId":"transaction-id-123"}})"
      R"(,"status":{"errorResult":{"location":"","message":"","reason":""})"
      R"(,"errors":[],"state":"DONE"},"user_email":"a@b.com"})";

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
          "selfLink": "jselfLink",
          "user_email": "juserEmail"})";
  auto const response = InsertJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json Job object")));
}

TEST(InsertJobResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "jobReference": {"projectId": "p123", "jobId": "j123"},
          "configuration": {
            "jobType": "QUERY",
            "query": {"query": "select 1;"}
          }})";
  auto response = InsertJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("InsertJobResponse", TracingOptions{}),
      R"(InsertJobResponse { http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLink" id: "j123")"
      R"( configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout { "0" } query_config { query: "select 1;")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning {)"
      R"( type: "" expiration_time { "0" } field: "" } range_partitioning {)"
      R"( field: "" range { start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables {)"
      R"( values { } } } } reference { project_id: "p123" job_id: "j123")"
      R"( location: "" } status { state: "DONE" error_result { reason: "")"
      R"( location: "" message: "" } } statistics { creation_time { "0" })"
      R"( start_time { "0" } end_time { "0" } total_slot_time { "0" })"
      R"( final_execution_duration { "0" } total_bytes_processed: 0)"
      R"( num_child_jobs: 0)"
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
          "InsertJobResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(InsertJobResponse { http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLi...<truncated>...")"
      R"( id: "j123" configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout { "0" } query_config { query: "select ...<truncated>...")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning { field: "" range {)"
      R"( start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables { values { } } } })"
      R"( reference { project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "0" } start_time { "0" } end_time { "0" })"
      R"( total_slot_time { "0" } final_execution_duration { "0" })"
      R"( total_bytes_processed: 0 num_child_jobs: 0)"
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

  EXPECT_EQ(
      response->DebugString("InsertJobResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(InsertJobResponse {
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
      job_timeout {
        "0"
      }
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
        maximum_bytes_billed: -1
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
          statement_timeout {
            "0"
          }
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

TEST(CancelJobResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind":"cancel-kind")"
      R"(,"job":{"configuration":{"dryRun":true,"jobTimeoutMs":"10","jobType":"QUERY")"
      R"(,"labels":{"label-key1":"label-val1"},"query":{"allowLargeResults":true)"
      R"(,"clustering":{"fields":["clustering-field-1","clustering-field-2"]})"
      R"(,"connectionProperties":[{"key":"conn-prop-key","value":"conn-prop-val"}])"
      R"(,"createDisposition":"job-create-disposition","createSession":true)"
      R"(,"defaultDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"destinationEncryptionConfiguration":{"kmsKeyName":"encryption-key-name"})"
      R"(,"destinationTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"flattenResults":true,"maximumBytesBilled":"0","parameterMode":"job-param-mode")"
      R"(,"preserveNulls":true,"priority":"job-priority","query":"select 1;")"
      R"(,"queryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[])"
      R"(,"type":"array-struct-type"}}],"type":"array-type"})"
      R"(,"structTypes":[{"description":"qp-struct-description","name":"qp-struct-name")"
      R"(,"type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{"arrayValues":[{"arrayValues":[])"
      R"(,"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}])"
      R"(,"structValues":{"qp-map-key":{"arrayValues":[],"structValues":{})"
      R"(,"value":"qp-map-value"}},"value":"query-parameter-value"}}])"
      R"(,"rangePartitioning":{"field":"rp-field-1","range":{"end":"range-end")"
      R"(,"interval":"range-interval","start":"range-start"}})"
      R"(,"schemaUpdateOptions":["job-update-options"])"
      R"(,"scriptOptions":{"keyResultStatement":"FIRST_SELECT")"
      R"(,"statementByteBudget":"10","statementTimeoutMs":"10"})"
      R"(,"systemVariables":{"types":{"sql-struct-type-key-1":{)"
      R"("structType":{"fields":[{"name":"f1-sql-struct-type-int64"}]})"
      R"(,"sub_type_index":2,"typeKind":"INT64"})"
      R"(,"sql-struct-type-key-2":{"structType":{"fields":[{"name":"f2-sql-struct-type-string"}]})"
      R"(,"sub_type_index":2,"typeKind":"STRING"})"
      R"(,"sql-struct-type-key-3":{"arrayElementType":{"structType":{)"
      R"("fields":[{"name":"f2-sql-struct-type-string"}]},"sub_type_index":2)"
      R"(,"typeKind":"STRING"},"sub_type_index":1,"typeKind":"STRING"}})"
      R"(,"values":{"fields":{"bool-key":{"kind_index":3,"valueKind":true})"
      R"(,"double-key":{"kind_index":1,"valueKind":3.4},"string-key":{"kind_index":2,"valueKind":"val3"}}}})"
      R"(,"timePartitioning":{"expirationTime":"0","field":"tp-field-1","type":"tp-field-type"})"
      R"(,"useLegacySql":true,"useQueryCache":true,"writeDisposition":"job-write-disposition"}})"
      R"(,"etag":"etag","id":"1","jobReference":{"jobId":"2","location":"us-east","projectId":"1"})"
      R"(,"kind":"Job","selfLink":"self-link","statistics":{"completionRatio":1234.1234)"
      R"(,"creationTime":"10","dataMaskingStatistics":{"dataMaskingApplied":true},"endTime":"10")"
      R"(,"finalExecutionDurationMs":"10","numChildJobs":"1234","parentJobId":"parent-job-123")"
      R"(,"query":{"billingTier":1234,"cacheHit":true,"dclTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"dclTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dclTargetView":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"ddlAffectedRowAccessPolicyCount":"1234","ddlOperationPerformed":"ddl_operation_performed")"
      R"(,"ddlTargetDataset":{"datasetId":"1","projectId":"2"})"
      R"(,"ddlTargetRoutine":{"datasetId":"1","projectId":"2","routineId":"3"})"
      R"(,"ddlTargetRowAccessPolicy":{"datasetId":"1","policyId":"3","projectId":"1234","tableId":"2"})"
      R"(,"ddlTargetTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"dmlStats":{"deletedRowCount":"1234","insertedRowCount":"1234","updatedRowCount":"1234"})"
      R"(,"estimatedBytesProcessed":"1234","materializedViewStatistics":{"materializedView":[{"chosen":true)"
      R"(,"estimatedBytesSaved":"1234","rejectedReason":"BASE_TABLE_DATA_CHANGE")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"}}]})"
      R"(,"metadataCacheStatistics":{"tableMetadataCacheUsage":[{"explanation":"test-table-metadata")"
      R"(,"tableReference":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"unusedReason":"EXCEEDED_MAX_STALENESS"}]})"
      R"(,"numDmlAffectedRows":"1234","performanceInsights":{"avgPreviousExecutionMs":"10")"
      R"(,"stagePerformanceChangeInsights":{"inputDataChange":{"recordsReadDiffPercentage":12.119999885559082})"
      R"(,"stageId":"1234"},"stagePerformanceStandaloneInsights":{"insufficientShuffleQuota":true)"
      R"(,"slotContention":true,"stageId":"1234"}},"queryPlan":[{"completedParallelInputs":"1234")"
      R"(,"computeMode":"BIGQUERY","computeMsAvg":"10","computeMsMax":"10")"
      R"(,"computeRatioAvg":1234.1234,"computeRatioMax":1234.1234,"endMs":"10")"
      R"(,"id":"1234","inputStages":["1234"],"name":"test-explain","parallelInputs":"1234")"
      R"(,"readMsAvg":"10","readMsMax":"10","readRatioAvg":1234.1234,"readRatioMax":1234.1234)"
      R"(,"recordsRead":"1234","recordsWritten":"1234","shuffleOutputBytes":"1234")"
      R"(,"shuffleOutputBytesSpilled":"1234","slotMs":"10","startMs":"10","status":"explain-status")"
      R"(,"steps":[{"kind":"sub-step-kind","substeps":["sub-step-1"]}])"
      R"(,"waitMsAvg":"10","waitMsMax":"10","waitRatioAvg":1234.1234)"
      R"(,"waitRatioMax":1234.1234,"writeMsAvg":"10","writeMsMax":"10")"
      R"(,"writeRatioAvg":1234.1234,"writeRatioMax":1234.1234}])"
      R"(,"referencedRoutines":[{"datasetId":"1","projectId":"2","routineId":"3"}])"
      R"(,"referencedTables":[{"datasetId":"1","projectId":"2","tableId":"3"}])"
      R"(,"schema":{"fields":[{"categories":{"names":[]},"collation":"")"
      R"(,"defaultValueExpression":"","description":"","fields":{"fields":[]})"
      R"(,"maxLength":0,"mode":"fmode","name":"fname-1","policyTags":{"names":[]})"
      R"(,"precision":0,"rangeElementType":{"type":""},"roundingMode":"")"
      R"(,"scale":0,"type":""}]},"searchStatistics":{"indexUnusedReasons":[{)"
      R"("baseTable":{"datasetId":"1","projectId":"2","tableId":"3"})"
      R"(,"code":"BASE_TABLE_TOO_SMALL","indexName":"test-index","message":""}])"
      R"(,"indexUsageMode":"PARTIALLY_USED"},"statementType":"statement_type")"
      R"(,"timeline":[{"activeUnits":"1234","completedUnits":"1234","elapsedMs":"10")"
      R"(,"estimatedRunnableUnits":"1234","pendingUnits":"1234","totalSlotMs":"10"}])"
      R"(,"totalBytesBilled":"1234","totalBytesProcessed":"1234",)"
      R"("totalBytesProcessedAccuracy":"total_bytes_processed_accuracy","totalPartitionsProcessed":"1234")"
      R"(,"totalSlotMs":"10","transferredBytes":"1234")"
      R"(,"undeclaredQueryParameters":[{"name":"query-parameter-name")"
      R"(,"parameterType":{"arrayType":{"structTypes":[{"description":"array-struct-description")"
      R"(,"name":"array-struct-name","type":{"structTypes":[],"type":"array-struct-type"}}])"
      R"(,"type":"array-type"},"structTypes":[{"description":"qp-struct-description")"
      R"(,"name":"qp-struct-name","type":{"structTypes":[],"type":"qp-struct-type"}}])"
      R"(,"type":"query-parameter-type"},"parameterValue":{"arrayValues":[{)"
      R"("arrayValues":[{"arrayValues":[],"structValues":{"array-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"array-map-value"}},"value":"array-val-2"}])"
      R"(,"structValues":{},"value":"array-val-1"}],"structValues":{"qp-map-key":{"arrayValues":[])"
      R"(,"structValues":{},"value":"qp-map-value"}},"value":"query-parameter-value"}}]})"
      R"(,"quotaDeferments":["quota-defer-1"],"reservation_id":"reservation-id-123")"
      R"(,"rowLevelSecurityStatistics":{"rowLevelSecurityApplied":true})"
      R"(,"scriptStatistics":{"evaluationKind":"STATEMENT")"
      R"(,"stackFrames":[{"endColumn":1234,"endLine":1234,"procedureId":"proc-id")"
      R"(,"startColumn":1234,"startLine":1234,"text":"stack-frame-text"}]})"
      R"(,"sessionInfo":{"sessionId":"session-id-123"},"startTime":"10")"
      R"(,"totalBytesProcessed":"1234","totalSlotMs":"10","transactionInfo":{"transactionId":"transaction-id-123"}})"
      R"(,"status":{"errorResult":{"location":"","message":"","reason":""})"
      R"(,"errors":[],"state":"DONE"},"user_email":"a@b.com"}})";

  auto const cancel_job_response =
      CancelJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_FALSE(cancel_job_response->http_response.payload.empty());

  auto expected_job = MakeJob();
  auto actual_job = cancel_job_response->job;

  EXPECT_EQ(cancel_job_response->kind, "cancel-kind");
  bigquery_v2_minimal_testing::AssertEquals(expected_job, actual_job);
}

TEST(CancelJobResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response = CancelJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Empty payload in HTTP response")));
}

TEST(CancelJobResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const response = CancelJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(CancelJobResponseTest, InvalidCancelJobResponse) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail"})";
  auto const response = CancelJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Not a valid CancelJobResponse object")));
}

TEST(CancelJobResponseTest, InvalidJob) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind":"cancel-kind",
          "job": {"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail"}})";
  auto const response = CancelJobResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json Job object")));
}

TEST(CancelJobResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"kind": "cancel-kind",
          "job": {"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "jobReference": {"projectId": "p123", "jobId": "j123"},
          "configuration": {
            "jobType": "QUERY",
            "query": {"query": "select 1;"}
          }}})";
  auto response = CancelJobResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("CancelJobResponse", TracingOptions{}),
      R"(CancelJobResponse {)"
      R"( kind: "cancel-kind")"
      R"( http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLink" id: "j123")"
      R"( configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout { "0" } query_config { query: "select 1;")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning {)"
      R"( type: "" expiration_time { "0" } field: "" } range_partitioning {)"
      R"( field: "" range { start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables {)"
      R"( values { } } } } reference { project_id: "p123" job_id: "j123")"
      R"( location: "" } status { state: "DONE" error_result { reason: "")"
      R"( location: "" message: "" } } statistics { creation_time { "0" })"
      R"( start_time { "0" } end_time { "0" } total_slot_time { "0" })"
      R"( final_execution_duration { "0" } total_bytes_processed: 0)"
      R"( num_child_jobs: 0)"
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
          "CancelJobResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(CancelJobResponse {)"
      R"( kind: "cancel-...<truncated>...")"
      R"( http_response { status_code: 200 http_headers {)"
      R"( key: "header1" value: "value1" } payload: REDACTED } job {)"
      R"( etag: "jtag" kind: "jkind" self_link: "jselfLi...<truncated>...")"
      R"( id: "j123" configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout { "0" } query_config { query: "select ...<truncated>...")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false maximum_bytes_billed: -1)"
      R"( default_dataset { project_id: "" dataset_id: "" } destination_table {)"
      R"( project_id: "" dataset_id: "" table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" } range_partitioning { field: "" range {)"
      R"( start: "" end: "" interval: "" } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout { "0" } statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables { values { } } } })"
      R"( reference { project_id: "p123" job_id: "j123" location: "" } status {)"
      R"( state: "DONE" error_result { reason: "" location: "" message: "" } })"
      R"( statistics { creation_time { "0" } start_time { "0" } end_time { "0" })"
      R"( total_slot_time { "0" } final_execution_duration { "0" })"
      R"( total_bytes_processed: 0 num_child_jobs: 0)"
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

  EXPECT_EQ(
      response->DebugString("CancelJobResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(CancelJobResponse {
  kind: "cancel-kind"
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
      job_timeout {
        "0"
      }
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
        maximum_bytes_billed: -1
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
          statement_timeout {
            "0"
          }
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

TEST(QueryResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload = MakeQueryResponsePayload();
  auto const response = QueryResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->http_response.payload.empty());

  auto expected_equery_results = MakePostQueryResults();
  auto actual_query_results = response->post_query_results;

  bigquery_v2_minimal_testing::AssertEquals(expected_equery_results,
                                            actual_query_results);
}

TEST(QueryResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response = QueryResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Empty payload in HTTP response")));
}

TEST(QueryResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const response = QueryResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(QueryResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.payload = MakeQueryResponsePayload();
  auto const response = QueryResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("QueryResponse", TracingOptions{}),
      R"(QueryResponse {)"
      R"( http_response {)"
      R"( status_code: 200)"
      R"( payload: REDACTED)"
      R"( })"
      R"( query_results {)"
      R"( kind: "query-kind")"
      R"( page_token: "np123")"
      R"( total_rows: 1000)"
      R"( total_bytes_processed: 1000)"
      R"( num_dml_affected_rows: 5)"
      R"( job_complete: true)"
      R"( cache_hit: true)"
      R"( rows { fields {)"
      R"( key: "bool-key")"
      R"( value { value_kind: true } })"
      R"( fields { key: "double-key")"
      R"( value { value_kind: 3.4 })"
      R"( } fields { key: "string-key" value { value_kind: "val3" } } })"
      R"( schema { fields {)"
      R"( name: "fname-1" type: "" mode: "fmode" description: "")"
      R"( collation: "" default_value_expression: "" max_length: 0)"
      R"( precision: 0 scale: 0)"
      R"( categories { } policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( job_reference { project_id: "p123" job_id: "j123" location: "useast" })"
      R"( session_info { session_id: "123" })"
      R"( dml_stats { inserted_row_count: 10 deleted_row_count: 10 updated_row_count: 10 } } })");

  EXPECT_EQ(
      response->DebugString(
          "QueryResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(QueryResponse { http_response {)"
      R"( status_code: 200 payload: REDACTED })"
      R"( query_results { kind: "query-k...<truncated>...")"
      R"( page_token: "np123" total_rows: 1000)"
      R"( total_bytes_processed: 1000 num_dml_affected_rows: 5)"
      R"( job_complete: true cache_hit: true)"
      R"( rows { fields { key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode")"
      R"( description: "" collation: "" default_value_expression: "")"
      R"( max_length: 0 precision: 0 scale: 0 categories { })"
      R"( policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( job_reference { project_id: "p123" job_id: "j123" location: "useast" })"
      R"( session_info { session_id: "123" } dml_stats { inserted_row_count: 10)"
      R"( deleted_row_count: 10 updated_row_count: 10 } } })");

  EXPECT_EQ(response->DebugString("QueryResponse", TracingOptions{}.SetOptions(
                                                       "single_line_mode=F")),
            R"(QueryResponse {
  http_response {
    status_code: 200
    payload: REDACTED
  }
  query_results {
    kind: "query-kind"
    page_token: "np123"
    total_rows: 1000
    total_bytes_processed: 1000
    num_dml_affected_rows: 5
    job_complete: true
    cache_hit: true
    rows {
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
    job_reference {
      project_id: "p123"
      job_id: "j123"
      location: "useast"
    }
    session_info {
      session_id: "123"
    }
    dml_stats {
      inserted_row_count: 10
      deleted_row_count: 10
      updated_row_count: 10
    }
  }
})");
}

TEST(GetQueryResultsResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload = MakeGetQueryResultsResponsePayload();
  auto const response =
      GetQueryResultsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->http_response.payload.empty());

  auto expected_get_equery_results = MakeGetQueryResults();
  auto actual_get_query_results = response->get_query_results;

  bigquery_v2_minimal_testing::AssertEquals(expected_get_equery_results,
                                            actual_get_query_results);
}

TEST(GetQueryResultsResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response =
      GetQueryResultsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Empty payload in HTTP response")));
}

TEST(GetQueryResultsResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const response =
      GetQueryResultsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(GetQueryResultsResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.payload = MakeGetQueryResultsResponsePayload();
  auto const response =
      GetQueryResultsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(
      response->DebugString("GetQueryResultsResponse", TracingOptions{}),
      R"(GetQueryResultsResponse {)"
      R"( http_response {)"
      R"( status_code: 200)"
      R"( payload: REDACTED)"
      R"( })"
      R"( get_query_results {)"
      R"( kind: "query-kind")"
      R"( etag: "query-etag")"
      R"( page_token: "np123")"
      R"( total_rows: 1000)"
      R"( total_bytes_processed: 1000)"
      R"( num_dml_affected_rows: 5)"
      R"( job_complete: true)"
      R"( cache_hit: true)"
      R"( rows { fields {)"
      R"( key: "bool-key")"
      R"( value { value_kind: true } })"
      R"( fields { key: "double-key")"
      R"( value { value_kind: 3.4 })"
      R"( } fields { key: "string-key" value { value_kind: "val3" } } })"
      R"( schema { fields {)"
      R"( name: "fname-1" type: "" mode: "fmode" description: "")"
      R"( collation: "" default_value_expression: "" max_length: 0)"
      R"( precision: 0 scale: 0)"
      R"( categories { } policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( job_reference { project_id: "p123" job_id: "j123" location: "useast" } } })");

  EXPECT_EQ(
      response->DebugString(
          "GetQueryResultsResponse",
          TracingOptions{}.SetOptions("truncate_string_field_longer_than=7")),
      R"(GetQueryResultsResponse { http_response {)"
      R"( status_code: 200 payload: REDACTED })"
      R"( get_query_results { kind: "query-k...<truncated>...")"
      R"( etag: "query-e...<truncated>...")"
      R"( page_token: "np123" total_rows: 1000)"
      R"( total_bytes_processed: 1000 num_dml_affected_rows: 5)"
      R"( job_complete: true cache_hit: true)"
      R"( rows { fields { key: "bool-key" value { value_kind: true } })"
      R"( fields { key: "double-key" value { value_kind: 3.4 } })"
      R"( fields { key: "string-key" value { value_kind: "val3" } } })"
      R"( schema { fields { name: "fname-1" type: "" mode: "fmode")"
      R"( description: "" collation: "" default_value_expression: "")"
      R"( max_length: 0 precision: 0 scale: 0 categories { })"
      R"( policy_tags { })"
      R"( rounding_mode { value: "" } range_element_type { type: "" } } })"
      R"( job_reference { project_id: "p123" job_id: "j123" location: "useast" } } })");

  EXPECT_EQ(
      response->DebugString("GetQueryResultsResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(GetQueryResultsResponse {
  http_response {
    status_code: 200
    payload: REDACTED
  }
  get_query_results {
    kind: "query-kind"
    etag: "query-etag"
    page_token: "np123"
    total_rows: 1000
    total_bytes_processed: 1000
    num_dml_affected_rows: 5
    job_complete: true
    cache_hit: true
    rows {
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
    job_reference {
      project_id: "p123"
      job_id: "j123"
      location: "useast"
    }
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
