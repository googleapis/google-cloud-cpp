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
using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(BigQueryJobStubTest, GetJobSuccess) {
  std::string job_response_payload = R"({"kind": "jkind",
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
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  GetJobRequest job_request("p123", "j123");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client), opts);

  auto result = rest_stub.GetJob(context, std::move(job_request));
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->http_response.http_status_code, Eq(HttpStatusCode::kOk));
  EXPECT_THAT(result->http_response.payload, Eq(job_response_payload));
  EXPECT_THAT(result->job.id, Eq("j123"));
  EXPECT_THAT(result->job.status.state, Eq("DONE"));
}

TEST(BigQueryJobStubTest, ProjectIdEmpty) {
  auto mock_rest_client = std::make_unique<MockRestClient>();
  GetJobRequest job_request("", "j123");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client), opts);
  auto result = rest_stub.GetJob(context, std::move(job_request));
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Invalid GetJobRequest: Project Id is empty")));
}

TEST(BigQueryJobStubTest, JobIdEmpty) {
  auto mock_rest_client = std::make_unique<MockRestClient>();
  GetJobRequest job_request("p123", "");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client), opts);
  auto result = rest_stub.GetJob(context, std::move(job_request));
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid GetJobRequest: Job Id is empty")));
}

TEST(BigQueryJobStubTest, RestClientError) {
  // Get() fails.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(An<rest::RestRequest const&>()))
      .WillOnce(
          Return(rest::AsStatus(HttpStatusCode::kInternalServerError, "")));

  GetJobRequest job_request("p123", "j123");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client), opts);
  auto result = rest_stub.GetJob(context, std::move(job_request));
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().code(), Eq(StatusCode::kUnavailable));
}

TEST(BigQueryJobStubTest, BuildRestResponseError) {
  // Invalid Rest response.
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));

  // Get() is successful.
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  GetJobRequest job_request("p123", "j123");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  rest_internal::RestContext context;
  DefaultBigQueryJobRestStub rest_stub(std::move(mock_rest_client), opts);
  auto result = rest_stub.GetJob(context, std::move(job_request));
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().code(), Eq(StatusCode::kInvalidArgument));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
