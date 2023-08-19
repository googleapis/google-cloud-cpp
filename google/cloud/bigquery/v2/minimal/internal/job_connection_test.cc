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

#include "google/cloud/bigquery/v2/minimal/internal/job_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::
    MakeFullGetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeGetQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::
    MakeGetQueryResultsResponsePayload;
using ::google::cloud::bigquery_v2_minimal_testing::MakePartialJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakePostQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryResponsePayload;
using ::google::cloud::bigquery_v2_minimal_testing::MockBigQueryJobRestStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

std::shared_ptr<BigQueryJobConnection> CreateTestingConnection(
    std::shared_ptr<BigQueryJobRestStub> mock) {
  BigQueryJobLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  auto options = BigQueryJobDefaultOptions(
      Options{}
          .set<BigQueryJobRetryPolicyOption>(retry.clone())
          .set<BigQueryJobBackoffPolicyOption>(backoff.clone()));
  return std::make_shared<BigQueryJobRestConnectionImpl>(std::move(mock),
                                                         std::move(options));
}

// Verify that the successful case works.
TEST(JobConnectionTest, GetJobSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

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

  EXPECT_CALL(*mock, GetJob)
      .WillOnce([&](rest_internal::RestContext&,
                    GetJobRequest const& request) -> StatusOr<GetJobResponse> {
        EXPECT_EQ("test-project-id", request.project_id());
        EXPECT_EQ("test-job-id", request.job_id());
        BigQueryHttpResponse http_response;
        http_response.payload = kExpectedPayload;
        return GetJobResponse::BuildFromHttpResponse(std::move(http_response));
      });

  auto conn = CreateTestingConnection(std::move(mock));

  GetJobRequest request;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");

  auto job = MakePartialJob();
  google::cloud::internal::OptionsSpan span(conn->options());
  auto job_result = conn->GetJob(request);

  ASSERT_STATUS_OK(job_result);
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, *job_result);
}

TEST(JobConnectionTest, ListJobsSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

  EXPECT_CALL(*mock, ListJobs)
      .WillOnce(
          [&](rest_internal::RestContext&, ListJobsRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_TRUE(request.page_token().empty());
            ListJobsResponse page;
            page.next_page_token = "page-1";
            ListFormatJob job;
            job.id = "job1";
            page.jobs.push_back(job);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListJobsRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("page-1", request.page_token());
            ListJobsResponse page;
            page.next_page_token = "page-2";
            ListFormatJob job;
            job.id = "job2";
            page.jobs.push_back(job);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListJobsRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("page-2", request.page_token());
            ListJobsResponse page;
            page.next_page_token = "";
            ListFormatJob job;
            job.id = "job3";
            page.jobs.push_back(job);
            return make_status_or(page);
          });

  auto conn = CreateTestingConnection(std::move(mock));

  std::vector<std::string> actual_job_ids;
  ListJobsRequest request;
  request.set_project_id("test-project-id");

  google::cloud::internal::OptionsSpan span(conn->options());
  for (auto const& job : conn->ListJobs(request)) {
    ASSERT_STATUS_OK(job);
    actual_job_ids.push_back(job->id);
  }
  EXPECT_THAT(actual_job_ids, ElementsAre("job1", "job2", "job3"));
}

TEST(JobConnectionTest, InsertJobSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

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

  EXPECT_CALL(*mock, InsertJob)
      .WillOnce(
          [&](rest_internal::RestContext&,
              InsertJobRequest const& request) -> StatusOr<InsertJobResponse> {
            EXPECT_EQ("test-project-id", request.project_id());
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return InsertJobResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto conn = CreateTestingConnection(std::move(mock));

  auto job = MakePartialJob();
  InsertJobRequest request("test-project-id", job);

  google::cloud::internal::OptionsSpan span(conn->options());
  auto job_result = conn->InsertJob(request);

  ASSERT_STATUS_OK(job_result);
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, *job_result);
}

TEST(JobConnectionTest, CancelJobSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

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
            "jobType": "QUERY",
            "query": {"query": "select 1;"}
          }}})";

  EXPECT_CALL(*mock, CancelJob)
      .WillOnce(
          [&](rest_internal::RestContext&,
              CancelJobRequest const& request) -> StatusOr<CancelJobResponse> {
            EXPECT_EQ("p123", request.project_id());
            EXPECT_EQ("j123", request.job_id());
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return CancelJobResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto conn = CreateTestingConnection(std::move(mock));

  auto job = MakePartialJob();
  CancelJobRequest request("p123", "j123");

  google::cloud::internal::OptionsSpan span(conn->options());
  auto job_result = conn->CancelJob(request);

  ASSERT_STATUS_OK(job_result);
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, *job_result);
}

TEST(JobConnectionTest, QuerySuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

  auto expected_payload = MakeQueryResponsePayload();

  EXPECT_CALL(*mock, Query)
      .WillOnce(
          [&](rest_internal::RestContext&,
              PostQueryRequest const& request) -> StatusOr<QueryResponse> {
            EXPECT_EQ("p123", request.project_id());
            BigQueryHttpResponse http_response;
            http_response.payload = expected_payload;
            return QueryResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto conn = CreateTestingConnection(std::move(mock));

  PostQueryRequest job_request;
  job_request.set_project_id("p123");
  job_request.set_query_request(MakeQueryRequest());

  google::cloud::internal::OptionsSpan span(conn->options());
  auto job_result = conn->Query(job_request);
  ASSERT_STATUS_OK(job_result);

  auto expected_query_results = MakePostQueryResults();

  bigquery_v2_minimal_testing::AssertEquals(expected_query_results,
                                            *job_result);
}

TEST(JobConnectionTest, QueryResultsSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

  auto expected_payload = MakeGetQueryResultsResponsePayload();

  EXPECT_CALL(*mock, GetQueryResults)
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

  auto conn = CreateTestingConnection(std::move(mock));

  GetQueryResultsRequest job_request = MakeFullGetQueryResultsRequest();

  google::cloud::internal::OptionsSpan span(conn->options());
  auto job_result = conn->QueryResults(job_request);
  ASSERT_STATUS_OK(job_result);

  auto expected_query_results = MakeGetQueryResults();

  bigquery_v2_minimal_testing::AssertEquals(expected_query_results,
                                            *job_result);
}

// Verify that permanent errors are reported immediately.
TEST(JobConnectionTest, GetJobPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, GetJob)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetJobRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->GetJob(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(JobConnectionTest, ListJobsPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, ListJobs)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListJobsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListJobs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(JobConnectionTest, InsertJobPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, InsertJob)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  InsertJobRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->InsertJob(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(JobConnectionTest, CancelJobPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, CancelJob)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  CancelJobRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->CancelJob(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(JobConnectionTest, QueryPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, Query)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  PostQueryRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->Query(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(JobConnectionTest, QueryResultsPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, GetQueryResults)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetQueryResultsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->QueryResults(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

// Verify that too many transients errors are reported correctly.
TEST(JobConnectionTest, GetJobTooManyTransients) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, GetJob)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetJobRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->GetJob(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

TEST(JobConnectionTest, ListJobsTooManyTransients) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, ListJobs)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListJobsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListJobs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kDeadlineExceeded));
}

TEST(JobConnectionTest, InsertJobNonIdempotentCalledOnce) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, InsertJob)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  InsertJobRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->InsertJob(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

TEST(JobConnectionTest, CancelJobNonIdempotentCalledOnce) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, CancelJob)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  CancelJobRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->CancelJob(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

TEST(JobConnectionTest, QueryWithoutRequestIdNonIdempotentCalledOnce) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, Query)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  PostQueryRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->Query(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

TEST(JobConnectionTest, QueryWithRequestIdIdempotentTooManyTransients) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, Query)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  QueryRequest query_request;
  query_request.set_request_id("123");

  PostQueryRequest request;
  request.set_query_request(query_request);

  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->Query(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

TEST(JobConnectionTest, QueryResultsTooManyTransients) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, GetQueryResults)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetQueryResultsRequest request;

  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->QueryResults(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
