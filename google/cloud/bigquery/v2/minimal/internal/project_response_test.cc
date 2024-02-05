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

#include "google/cloud/bigquery/v2/minimal/internal/project_response.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(ListProjectsResponseTest, SuccessMultiplePages) {
  BigQueryHttpResponse http_response;
  auto projects_json_txt = bigquery_v2_minimal_testing::MakeProjectJsonText();
  http_response.payload =
      bigquery_v2_minimal_testing::MakeListProjectsResponseJsonText();

  auto const list_projects_response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_projects_response);

  auto expected = bigquery_v2_minimal_testing::MakeProject();

  EXPECT_FALSE(list_projects_response->http_response.payload.empty());
  EXPECT_EQ(list_projects_response->kind, "kind-1");
  EXPECT_EQ(list_projects_response->etag, "tag-1");
  EXPECT_EQ(list_projects_response->next_page_token, "npt-123");
  EXPECT_EQ(list_projects_response->total_items, 1);

  auto const projects = list_projects_response->projects;
  ASSERT_EQ(projects.size(), 1);

  bigquery_v2_minimal_testing::AssertEquals(expected, projects[0]);
}

TEST(ListProjectsResponseTest, SuccessSinglePage) {
  BigQueryHttpResponse http_response;
  auto projects_json_txt = bigquery_v2_minimal_testing::MakeProjectJsonText();
  http_response.payload = bigquery_v2_minimal_testing::
      MakeListProjectsResponseNoPageTokenJsonText();

  auto const list_projects_response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_projects_response);

  auto expected = bigquery_v2_minimal_testing::MakeProject();

  EXPECT_FALSE(list_projects_response->http_response.payload.empty());
  EXPECT_EQ(list_projects_response->kind, "kind-1");
  EXPECT_EQ(list_projects_response->etag, "tag-1");
  EXPECT_THAT(list_projects_response->next_page_token, IsEmpty());
  EXPECT_EQ(list_projects_response->total_items, 1);

  auto const projects = list_projects_response->projects;
  ASSERT_EQ(projects.size(), 1);

  bigquery_v2_minimal_testing::AssertEquals(expected, projects[0]);
}

TEST(ListProjectsResponseTest, SuccessNoProjects) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "totalItems": 0})";
  auto const response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->total_items, 0);
}

TEST(ListProjectsResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListProjectsResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Invalid";
  auto const response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListProjectsResponseTest, InvalidProjectList) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "dkind",
          "etag": "dtag"})";
  auto const response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Not a valid Json ProjectList object")));
}

TEST(ListProjectsResponseTest, InvalidProject) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "totalItems": 1,
          "projects": [
              {
                "id": "1",
                "kind": "kind-2"
              }
  ]})";
  auto const response =
      ListProjectsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json Project object")));
}

TEST(ListProjectsResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      bigquery_v2_minimal_testing::MakeListProjectsResponseJsonText();

  auto response = ListProjectsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(response->DebugString("ListProjectsResponse", TracingOptions{}),
            R"(ListProjectsResponse {)"
            R"( kind: "kind-1")"
            R"( etag: "tag-1")"
            R"( next_page_token: "npt-123")"
            R"( total_items: 1)"
            R"( projects {)"
            R"( kind: "p-kind")"
            R"( id: "p-id")"
            R"( friendly_name: "p-friendly-name")"
            R"( project_reference {)"
            R"( project_id: "p-project-id")"
            R"( })"
            R"( numeric_id: 123)"
            R"( })"
            R"( http_response {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED)"
            R"( })"
            R"( })");

  EXPECT_EQ(response->DebugString("ListProjectsResponse",
                                  TracingOptions{}.SetOptions(
                                      "truncate_string_field_longer_than=7")),
            R"(ListProjectsResponse {)"
            R"( kind: "kind-1")"
            R"( etag: "tag-1")"
            R"( next_page_token: "npt-123")"
            R"( total_items: 1)"
            R"( projects {)"
            R"( kind: "p-kind")"
            R"( id: "p-id")"
            R"( friendly_name: "p-frien...<truncated>...")"
            R"( project_reference {)"
            R"( project_id: "p-proje...<truncated>...")"
            R"( })"
            R"( numeric_id: 123)"
            R"( })"
            R"( http_response {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED)"
            R"( })"
            R"( })");

  EXPECT_EQ(
      response->DebugString("ListProjectsResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListProjectsResponse {
  kind: "kind-1"
  etag: "tag-1"
  next_page_token: "npt-123"
  total_items: 1
  projects {
    kind: "p-kind"
    id: "p-id"
    friendly_name: "p-friendly-name"
    project_reference {
      project_id: "p-project-id"
    }
    numeric_id: 123
  }
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
