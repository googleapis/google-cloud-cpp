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

#include "google/cloud/bigquery/v2/minimal/internal/project_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockProjectRestStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Return;

std::shared_ptr<ProjectConnection> CreateTestingConnection(
    std::shared_ptr<ProjectRestStub> mock) {
  ProjectLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  auto options = ProjectDefaultOptions(
      Options{}
          .set<ProjectRetryPolicyOption>(retry.clone())
          .set<ProjectBackoffPolicyOption>(backoff.clone()));
  return std::make_shared<ProjectRestConnectionImpl>(std::move(mock),
                                                     std::move(options));
}

TEST(ProjectConnectionTest, ListProjectsSuccess) {
  auto mock = std::make_shared<MockProjectRestStub>();

  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          [&](rest_internal::RestContext&, ListProjectsRequest const& request) {
            EXPECT_TRUE(request.page_token().empty());
            EXPECT_EQ(1, request.max_results());
            ListProjectsResponse page;
            page.next_page_token = "page-1";
            Project project;
            project.id = "project1";
            page.projects.push_back(project);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListProjectsRequest const& request) {
            EXPECT_EQ("page-1", request.page_token());
            EXPECT_EQ(1, request.max_results());
            ListProjectsResponse page;
            page.next_page_token = "page-2";
            Project project;
            project.id = "project2";
            page.projects.push_back(project);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListProjectsRequest const& request) {
            EXPECT_EQ("page-2", request.page_token());
            EXPECT_EQ(1, request.max_results());
            ListProjectsResponse page;
            page.next_page_token = "";
            Project project;
            project.id = "project3";
            page.projects.push_back(project);
            return make_status_or(page);
          });

  auto conn = CreateTestingConnection(std::move(mock));

  std::vector<std::string> actual_project_ids;
  ListProjectsRequest request;
  request.set_max_results(1);

  google::cloud::internal::OptionsSpan span(conn->options());
  for (auto const& project : conn->ListProjects(request)) {
    ASSERT_STATUS_OK(project);
    actual_project_ids.push_back(project->id);
  }
  EXPECT_THAT(actual_project_ids,
              ElementsAre("project1", "project2", "project3"));
}

TEST(ProjectConnectionTest, ListProjectsPermanentError) {
  auto mock = std::make_shared<MockProjectRestStub>();
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListProjectsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListProjects(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(ProjectConnectionTest, ListProjectsTooManyTransients) {
  auto mock = std::make_shared<MockProjectRestStub>();
  EXPECT_CALL(*mock, ListProjects)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kResourceExhausted, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListProjectsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListProjects(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kResourceExhausted));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
