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
      R"(GetJobResponse { http_response {)"
      R"( status_code: 200)"
      R"( http_headers {)"
      R"( key: "header1" value: "value1")"
      R"( })"
      R"( payload: REDACTED)"
      R"( })"
      R"( job {)"
      R"( etag: "jtag" kind: "jkind" id: "j123")"
      R"( job_configuration {)"
      R"( job_type: "QUERY" dry_run: false job_timeout_ms: 0)"
      R"( query_config {)"
      R"( query: "select 1;" create_disposition: "")"
      R"( write_disposition: "" priority: "" parameter_mode: "")"
      R"( preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false)"
      R"( use_legacy_sql: false create_session: false)"
      R"( continuous: false maximum_bytes_billed: 0)"
      R"( default_dataset { project_id: "" dataset_id: "" })"
      R"( destination_table { project_id: "" dataset_id: "")"
      R"( table_id: "" } time_partitioning { type: "")"
      R"( expiration_time { "0" } field: "" })"
      R"( range_partitioning { field: "")"
      R"( range { start: "" end: "" interval: "" } })"
      R"( clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout_ms: 0)"
      R"( statement_byte_budget: 0 key_result_statement { value: "" } })"
      R"( system_variables { values { } } } })"
      R"( job_reference { project_id: "p123" job_id: "j123" location: "" })"
      R"( job_status: "DONE" error_result: "" } })");

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
    id: "j123"
    job_configuration {
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
    job_reference {
      project_id: "p123"
      job_id: "j123"
      location: ""
    }
    job_status: "DONE"
    error_result: ""
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
      R"( job_configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout_ms: 0 query_config { query: "select 1;")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false continuous: false maximum_bytes_billed: 0)"
      R"( default_dataset { project_id: "" dataset_id: "" })"
      R"( destination_table { project_id: "" dataset_id: "" table_id: "" })"
      R"( time_partitioning { type: "" expiration_time { "0" } field: "" })"
      R"( range_partitioning { field: "" range { start: "" end: "" interval: "")"
      R"( } } clustering { })"
      R"( destination_encryption_configuration { kms_key_name: "" })"
      R"( script_options { statement_timeout_ms: 0 statement_byte_budget: 0)"
      R"( key_result_statement { value: "" } } system_variables {)"
      R"( values { } } } } job_reference {)"
      R"( project_id: "p123" job_id: "j123" location: "" })"
      R"( job_status: "DONE" error_result: "" })"
      R"( next_page_token: "npt-123" kind: "kind-1" etag: "tag-1")"
      R"( http_response { status_code: 200)"
      R"( http_headers { key: "header1" value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString("ListJobsResponse",
                            TracingOptions{}.SetOptions(
                                "truncate_string_field_longer_than=1024")),
      R"(ListJobsResponse { jobs { id: "1" kind: "kind-2" state: "DONE")"
      R"( job_configuration { job_type: "QUERY" dry_run: false)"
      R"( job_timeout_ms: 0 query_config { query: "select 1;")"
      R"( create_disposition: "" write_disposition: "" priority: "")"
      R"( parameter_mode: "" preserve_nulls: false allow_large_results: false)"
      R"( use_query_cache: false flatten_results: false use_legacy_sql: false)"
      R"( create_session: false continuous: false maximum_bytes_billed: 0)"
      R"( default_dataset { project_id: "" dataset_id: "" })"
      R"( destination_table { project_id: "" dataset_id: "" table_id: "" })"
      R"( time_partitioning { type: "" expiration_time { "0" } field: "" })"
      R"( range_partitioning { field: "")"
      R"( range { start: "" end: "" interval: "" } })"
      R"( clustering { } destination_encryption_configuration {)"
      R"( kms_key_name: "" } script_options { statement_timeout_ms: 0)"
      R"( statement_byte_budget: 0 key_result_statement { value: "" } })"
      R"( system_variables { values { } } } } job_reference {)"
      R"( project_id: "p123" job_id: "j123" location: "" })"
      R"( job_status: "DONE" error_result: "" } next_page_token: "npt-123")"
      R"( kind: "kind-1" etag: "tag-1" http_response { status_code: 200)"
      R"( http_headers { key: "header1" value: "value1" } payload: REDACTED } })");

  EXPECT_EQ(
      response->DebugString("ListJobsResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListJobsResponse {
  jobs {
    id: "1"
    kind: "kind-2"
    state: "DONE"
    job_configuration {
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
    job_reference {
      project_id: "p123"
      job_id: "j123"
      location: ""
    }
    job_status: "DONE"
    error_result: ""
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
