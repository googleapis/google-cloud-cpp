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

#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_project_connection.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;

TEST(ProjectClientTest, ListProjectsSuccess) {
  auto mock = std::make_shared<MockProjectConnection>();
  EXPECT_CALL(*mock, options);

  Project list_format_project = bigquery_v2_minimal_testing::MakeProject();

  EXPECT_CALL(*mock, ListProjects)
      .WillOnce([list_format_project](ListProjectsRequest const& request) {
        EXPECT_TRUE(request.max_results() > 0);
        return mocks::MakeStreamRange<Project>({list_format_project});
        ;
      });

  ProjectClient client(std::move(mock));
  ListProjectsRequest request;
  request.set_max_results(1);

  auto range = client.ListProjects(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kOk));
}

TEST(ProjectClientTest, ListProjectsFailure) {
  auto mock = std::make_shared<MockProjectConnection>();
  EXPECT_CALL(*mock, options);

  EXPECT_CALL(*mock, ListProjects)
      .WillOnce([](ListProjectsRequest const& request) {
        EXPECT_TRUE(request.max_results() > 0);
        return mocks::MakeStreamRange<Project>(
            {}, internal::PermissionDeniedError("denied"));
        ;
      });

  ProjectClient client(std::move(mock));
  ListProjectsRequest request;
  request.set_max_results(1);

  auto range = client.ListProjects(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
