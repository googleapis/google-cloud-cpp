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

#include "google/cloud/bigquery/v2/minimal/internal/project_metadata.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/metadata_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::GetMetadataOptions;
using ::google::cloud::bigquery_v2_minimal_testing::MockProjectRestStub;
using ::google::cloud::bigquery_v2_minimal_testing::VerifyMetadataContext;
using ::testing::IsEmpty;

std::shared_ptr<ProjectMetadata> CreateMockProjectMetadata(
    std::shared_ptr<ProjectRestStub> mock) {
  return std::make_shared<ProjectMetadata>(std::move(mock));
}

TEST(ProjectMetadataTest, ListProjects) {
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

  auto metadata = CreateMockProjectMetadata(std::move(mock_stub));
  ListProjectsRequest request;
  request.set_max_results(10).set_page_token("pg-123");
  rest_internal::RestContext context;

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->ListProjects(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
