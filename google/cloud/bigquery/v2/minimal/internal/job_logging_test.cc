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

#include "google/cloud/bigquery/v2/minimal/internal/job_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::
    MakeGetQueryResultsResponsePayload;
using ::google::cloud::bigquery_v2_minimal_testing::MakePartialJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryResponsePayload;
using ::google::cloud::bigquery_v2_minimal_testing::MockBigQueryJobRestStub;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

std::shared_ptr<BigQueryJobLogging> CreateMockJobLogging(
    std::shared_ptr<BigQueryJobRestStub> mock) {
  return std::make_shared<BigQueryJobLogging>(std::move(mock), TracingOptions{},
                                              std::set<std::string>{});
}

TEST(JobLoggingClientTest, GetJob) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
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

  EXPECT_CALL(*mock_stub, GetJob)
      .WillOnce([&](rest_internal::RestContext&,
                    GetJobRequest const& request) -> StatusOr<GetJobResponse> {
        EXPECT_THAT(request.project_id(), Not(IsEmpty()));
        EXPECT_THAT(request.job_id(), Not(IsEmpty()));
        BigQueryHttpResponse http_response;
        http_response.payload = kExpectedPayload;
        return GetJobResponse::BuildFromHttpResponse(std::move(http_response));
      });

  auto client = CreateMockJobLogging(std::move(mock_stub));
  GetJobRequest request("p123", "j123");
  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->GetJob(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(" << ")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetJobRequest)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetJobResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(state: "DONE")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "j123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "jkind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(etag: "jtag")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(JobLoggingClientTest, ListJobs) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-1",
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

  EXPECT_CALL(*mock_stub, ListJobs)
      .WillOnce(
          [&](rest_internal::RestContext&,
              ListJobsRequest const& request) -> StatusOr<ListJobsResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return ListJobsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockJobLogging(std::move(mock_stub));
  ListJobsRequest request("p123");
  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->ListJobs(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(" << ")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListJobsRequest)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(all_users: false)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(max_results: 0)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListJobsResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "kind-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(state: "DONE")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(JobLoggingClientTest, InsertJob) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
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

  EXPECT_CALL(*mock_stub, InsertJob)
      .WillOnce(
          [&](rest_internal::RestContext&,
              InsertJobRequest const& request) -> StatusOr<InsertJobResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return InsertJobResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockJobLogging(std::move(mock_stub));

  InsertJobRequest request("test-project-id", MakePartialJob());

  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->InsertJob(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(" << ")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(InsertJobRequest)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(InsertJobResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(state: "DONE")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "j123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "jkind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(etag: "jtag")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(JobLoggingClientTest, CancelJob) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind":"cancel-job",
          "job":{"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "selfLink": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "jobReference": {"projectId": "p123", "jobId": "j123"},
          "configuration": {
            "job_tjobTypeype": "QUERY",
            "query": {"query": "select 1;"}
          }}})";

  EXPECT_CALL(*mock_stub, CancelJob)
      .WillOnce(
          [&](rest_internal::RestContext&,
              CancelJobRequest const& request) -> StatusOr<CancelJobResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            EXPECT_THAT(request.job_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return CancelJobResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockJobLogging(std::move(mock_stub));

  CancelJobRequest request("p123", "j123");

  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->CancelJob(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(" << ")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(CancelJobRequest)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(CancelJobResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(state: "DONE")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "j123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "jkind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(etag: "jtag")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(JobLoggingClientTest, Query) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto expected_payload = MakeQueryResponsePayload();

  EXPECT_CALL(*mock_stub, Query)
      .WillOnce(
          [&](rest_internal::RestContext&,
              PostQueryRequest const& request) -> StatusOr<QueryResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = expected_payload;
            return QueryResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockJobLogging(std::move(mock_stub));

  PostQueryRequest job_request;
  job_request.set_project_id("p123");
  job_request.set_query_request(MakeQueryRequest());

  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->Query(context, job_request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(" << ")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(PostQueryRequest)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(QueryResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "query-kind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(page_token: "np123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(JobLoggingClientTest, GetQueryResults) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto expected_payload = MakeGetQueryResultsResponsePayload();

  EXPECT_CALL(*mock_stub, GetQueryResults)
      .WillOnce([&](rest_internal::RestContext&,
                    GetQueryResultsRequest const& request)
                    -> StatusOr<GetQueryResultsResponse> {
        EXPECT_THAT(request.project_id(), Not(IsEmpty()));
        EXPECT_THAT(request.job_id(), Not(IsEmpty()));
        BigQueryHttpResponse http_response;
        http_response.payload = expected_payload;
        return GetQueryResultsResponse::BuildFromHttpResponse(
            std::move(http_response));
      });

  auto client = CreateMockJobLogging(std::move(mock_stub));
  GetQueryResultsRequest request("p123", "j123");

  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->GetQueryResults(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(" << ")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetQueryResultsRequest)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetQueryResultsResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "query-kind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(etag: "query-etag")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(job_id: "j123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(page_token: "np123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
