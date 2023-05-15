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

#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
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
using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Return;

ListProjectsRequest MakeListProjectsRequest() {
  ListProjectsRequest request;
  request.set_max_results(10).set_page_token("pg-123");
  return request;
}
TEST(ProjectStubTest, ListProjectsSuccess) {
  std::string payload =
      bigquery_v2_minimal_testing::MakeListProjectsResponseJsonText();
  auto mock_response = std::make_unique<MockRestResponse>();

  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(payload))));

  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(Return(ByMove(
          std::unique_ptr<rest::RestResponse>(std::move(mock_response)))));

  auto request = MakeListProjectsRequest();

  rest_internal::RestContext context;
  DefaultProjectRestStub rest_stub(std::move(mock_rest_client));

  auto result = rest_stub.ListProjects(context, std::move(request));
  ASSERT_STATUS_OK(result);

  auto const projects = result->projects;
  ASSERT_EQ(projects.size(), 1);

  auto expected = bigquery_v2_minimal_testing::MakeProject();

  EXPECT_EQ(result->http_response.http_status_code, HttpStatusCode::kOk);
  bigquery_v2_minimal_testing::AssertEquals(expected, projects[0]);
}

TEST(ProjectStubTest, ListProjectsRestClientError) {
  auto mock_rest_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(_, An<rest::RestRequest const&>()))
      .WillOnce(
          Return(rest::AsStatus(HttpStatusCode::kInternalServerError, "")));

  auto request = MakeListProjectsRequest();

  rest_internal::RestContext context;
  DefaultProjectRestStub rest_stub(std::move(mock_rest_client));

  auto response = rest_stub.ListProjects(context, std::move(request));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(ProjectStubTest, ListProjectsRestResponseError) {
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

  auto request = MakeListProjectsRequest();

  rest_internal::RestContext context;
  DefaultProjectRestStub rest_stub(std::move(mock_rest_client));

  auto response = rest_stub.ListProjects(context, std::move(request));

  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
