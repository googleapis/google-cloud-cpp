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

#include "google/cloud/bigquery/v2/minimal/internal/project_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockProjectRestStub;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

std::shared_ptr<ProjectLogging> CreateMockProjectLogging(
    std::shared_ptr<ProjectRestStub> mock) {
  return std::make_shared<ProjectLogging>(std::move(mock), TracingOptions{},
                                          std::set<std::string>{});
}

TEST(ProjectLoggingClientTest, ListProjects) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockProjectRestStub>();

  EXPECT_CALL(*mock_stub, ListProjects)
      .WillOnce(
          [&](rest_internal::RestContext&, ListProjectsRequest const& request)
              -> StatusOr<ListProjectsResponse> {
            EXPECT_THAT(request.page_token(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload =
                bigquery_v2_minimal_testing::MakeListProjectsResponseJsonText();
            return ListProjectsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockProjectLogging(std::move(mock_stub));
  ListProjectsRequest request;
  request.set_max_results(10).set_page_token("pt-123");
  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->ListProjects(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListProjectsRequest)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(max_results: 10)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(page_token: "pt-123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListProjectsResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "p-id")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "kind-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(etag: "tag-1")")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p-project-id")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(total_items: 1)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(next_page_token: "npt-123")")));
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
