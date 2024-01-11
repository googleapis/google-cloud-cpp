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

#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace rest = ::google::cloud::rest_internal;

using ::google::cloud::bigquery_v2_minimal_testing::
    MakeFullGetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeGetQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::
    MakeGetQueryResultsResponsePayload;
using ::google::cloud::bigquery_v2_minimal_testing::MakePartialJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakePostQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryResponsePayload;
using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Return;

Matcher<std::vector<absl::Span<char const>> const&> ExpectedPayload() {
  return An<std::vector<absl::Span<char const>> const&>();
}

Status InternalError() {
  return Status(StatusCode::kInternal, "Internal Error");
}

Status InvalidArgumentError() {
  return Status(StatusCode::kInvalidArgument, "");
}

ListJobsRequest GetListJobsRequest() {
  ListJobsRequest list_jobs_request("p123");
  auto const min = std::chrono::system_clock::now();
  auto const duration = std::chrono::milliseconds(100);
  auto const max = min + duration;
  list_jobs_request.set_all_users(true)
      .set_max_results(10)
      .set_min_creation_time(min)
      .set_max_creation_time(max)
      .set_parent_job_id("1")
      .set_projection(Projection::Full())
      .set_state_filter(StateFilter::Running());
  return list_jobs_request;
}

TEST(BigQueryJobStubTest, GetJobSuccess) {
  std::string job_response_payload = R"({"kind": "jkind",
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
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  GetJobRequest job_request("p123", "j123");

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto result = rest_stub.GetJob(context, std::move(job_request));
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->http_response.http_status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->http_response.payload, Eq(job_response_payload));
  EXPECT_THAT(result->job.id, Eq("j123"));
  EXPECT_THAT(result->job.status.state, Eq("DONE"));
}

TEST(BigQueryJobStubTest, GetJobRestClientError) {
  // Get() fails.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(
          Return(rest::AsStatus(HttpStatusCode::kInternalServerError, "")));

  GetJobRequest job_request("p123", "j123");

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto response = rest_stub.GetJob(context, std::move(job_request));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(BigQueryJobStubTest, GetJobRestResponseError) {
  // Invalid Rest response.
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  // Get() is successful.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  GetJobRequest job_request("p123", "j123");

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto response = rest_stub.GetJob(context, std::move(job_request));
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BigQueryJobStubTest, ListJobsSuccess) {
  std::string job_response_payload = R"({"etag": "tag-1",
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
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  auto list_jobs_request = GetListJobsRequest();

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto result = rest_stub.ListJobs(context, std::move(list_jobs_request));
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->http_response.http_status_code, Eq(HttpStatusCode::kOk));
}

TEST(BigQueryJobStubTest, ListJobsRestClientError) {
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(
          Return(rest::AsStatus(HttpStatusCode::kInternalServerError, "")));

  auto list_jobs_request = GetListJobsRequest();

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto response = rest_stub.ListJobs(context, std::move(list_jobs_request));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(BigQueryJobStubTest, ListJobsRestResponseError) {
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  auto list_jobs_request = GetListJobsRequest();

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto response = rest_stub.ListJobs(context, std::move(list_jobs_request));

  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
}

TEST(BigQueryJobStubTest, InsertJobSuccess) {
  std::string job_response_payload = R"({"kind": "jkind",
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
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  // Post() is successful.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  auto job = MakePartialJob();
  InsertJobRequest job_request("p123", job);

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto result = rest_stub.InsertJob(context, std::move(job_request));
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->http_response.http_status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->http_response.payload, Eq(job_response_payload));

  bigquery_v2_minimal_testing::AssertEqualsPartial(job, result->job);
}

TEST(BigQueryJobStubTest, InsertJobRestClientError) {
  // Post() fails with error code.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(InternalError()));

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  InsertJobRequest job_request("p123", MakePartialJob());

  auto status = rest_stub.InsertJob(context, std::move(job_request));
  EXPECT_THAT(status,
              StatusIs(InternalError().code(), InternalError().message()));
}

TEST(BigQueryJobStubTest, InsertJobRestResponseError) {
  // Setup invalid Rest response.
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  // Post() is successful but returns invalid Rest response.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  InsertJobRequest job_request("p123", MakePartialJob());

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto status = rest_stub.InsertJob(context, std::move(job_request));

  EXPECT_THAT(status, StatusIs(InvalidArgumentError().code()));
}

TEST(BigQueryJobStubTest, CancelJobSuccess) {
  std::string job_response_payload = R"({"kind":"cancel-job",
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
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  // Post() is successful.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  auto job = MakePartialJob();
  CancelJobRequest job_request("p123", "j123");

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto result = rest_stub.CancelJob(context, std::move(job_request));
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result->http_response.http_status_code, HttpStatusCode::kOk);
  EXPECT_EQ(result->http_response.payload, job_response_payload);
  EXPECT_EQ(result->kind, "cancel-job");
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, result->job);
}

TEST(BigQueryJobStubTest, CancelJobRestClientError) {
  // Post() fails with error code.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(InternalError()));

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  CancelJobRequest job_request("p123", "j123");

  auto status = rest_stub.CancelJob(context, std::move(job_request));
  EXPECT_THAT(status,
              StatusIs(InternalError().code(), InternalError().message()));
}

TEST(BigQueryJobStubTest, CancelJobRestResponseError) {
  // Setup invalid Rest response.
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  // Post() is successful but returns invalid Rest response.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  CancelJobRequest job_request("p123", "j123");

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto status = rest_stub.CancelJob(context, std::move(job_request));

  EXPECT_THAT(status, StatusIs(InvalidArgumentError().code()));
}

TEST(BigQueryJobStubTest, QuerySuccess) {
  std::string job_response_payload = MakeQueryResponsePayload();
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  // Post() is successful.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  PostQueryRequest job_request;
  job_request.set_project_id("p123");
  job_request.set_query_request(MakeQueryRequest());

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto result = rest_stub.Query(context, std::move(job_request));
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->http_response.http_status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->http_response.payload, Eq(job_response_payload));

  auto expected_query_results = MakePostQueryResults();

  bigquery_v2_minimal_testing::AssertEquals(expected_query_results,
                                            result->post_query_results);
}

TEST(BigQueryJobStubTest, QueryRestClientError) {
  // Post() fails with error code.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(InternalError()));

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  PostQueryRequest job_request;
  job_request.set_project_id("p123");
  job_request.set_query_request(MakeQueryRequest());

  auto status = rest_stub.Query(context, std::move(job_request));
  EXPECT_THAT(status,
              StatusIs(InternalError().code(), InternalError().message()));
}

TEST(BigQueryJobStubTest, QueryRestResponseError) {
  // Setup invalid Rest response.
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  // Post() is successful but returns invalid Rest response.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client,
              Post(_, An<rest::RestRequest const&>(), ExpectedPayload()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  PostQueryRequest job_request;
  job_request.set_project_id("p123");
  job_request.set_query_request(MakeQueryRequest());

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto status = rest_stub.Query(context, std::move(job_request));

  EXPECT_THAT(status, StatusIs(InvalidArgumentError().code()));
}

TEST(BigQueryJobStubTest, GetQueryResultsSuccess) {
  std::string response_payload = MakeGetQueryResultsResponsePayload();
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response_payload))));

  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  GetQueryResultsRequest request = MakeFullGetQueryResultsRequest();

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  auto expected = MakeGetQueryResults();

  auto actual_result = rest_stub.GetQueryResults(context, std::move(request));
  ASSERT_STATUS_OK(actual_result);
  EXPECT_THAT(actual_result->http_response.http_status_code,
              Eq(HttpStatusCode::kOk));
  EXPECT_THAT(actual_result->http_response.payload, Eq(response_payload));
  bigquery_v2_minimal_testing::AssertEquals(expected,
                                            actual_result->get_query_results);
}

TEST(BigQueryJobStubTest, GetQueryResultsRestClientError) {
  // Get() fails.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(
          Return(rest::AsStatus(HttpStatusCode::kInternalServerError, "")));

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  GetQueryResultsRequest request = MakeFullGetQueryResultsRequest();

  auto response = rest_stub.GetQueryResults(context, request);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(BigQueryJobStubTest, GetQueryResultsRestResponseError) {
  // Invalid Rest response.
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  // Get() is successful.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client));

  GetQueryResultsRequest request = MakeFullGetQueryResultsRequest();

  auto response = rest_stub.GetQueryResults(context, std::move(request));
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
