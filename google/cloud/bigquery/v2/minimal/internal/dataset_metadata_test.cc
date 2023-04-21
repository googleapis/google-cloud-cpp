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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_metadata.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_dataset_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockDatasetRestStub;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

static auto const kUserProject = "test-only-project";
static auto const kQuotaUser = "test-quota-user";

std::shared_ptr<DatasetMetadata> CreateMockDatasetMetadata(
    std::shared_ptr<DatasetRestStub> mock) {
  return std::make_shared<DatasetMetadata>(std::move(mock));
}

void VerifyMetadataContext(rest_internal::RestContext& context) {
  EXPECT_THAT(context.GetHeader("x-goog-api-client"),
              Contains(HasSubstr("bigquery_v2_dataset")));
  EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
  EXPECT_THAT(context.GetHeader("x-goog-user-project"),
              ElementsAre(kUserProject));
  EXPECT_THAT(context.GetHeader("x-goog-quota-user"), ElementsAre(kQuotaUser));
  EXPECT_THAT(context.GetHeader("x-server-timeout"), ElementsAre("3.141"));
}

Options GetMetadataOptions() {
  return Options{}
      .set<UserProjectOption>(kUserProject)
      .set<QuotaUserOption>(kQuotaUser)
      .set<ServerTimeoutOption>(std::chrono::milliseconds(3141));
}

TEST(DatasetMetadataTest, GetDataset) {
  auto mock_stub = std::make_shared<MockDatasetRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind": "d-kind",
          "etag": "d-tag",
          "id": "d-id",
          "self_link": "d-selfLink",
          "friendly_name": "d-friendly-name",
          "dataset_reference": {"project_id": "p-id", "dataset_id": "d-id"}
    })";

  EXPECT_CALL(*mock_stub, GetDataset)
      .WillOnce(
          [&](rest_internal::RestContext&, GetDatasetRequest const& request)
              -> StatusOr<GetDatasetResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            EXPECT_THAT(request.dataset_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return GetDatasetResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto metadata = CreateMockDatasetMetadata(std::move(mock_stub));
  GetDatasetRequest request;
  rest_internal::RestContext context;
  request.set_project_id("test-project-id");
  request.set_dataset_id("test-dataset-id");

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->GetDataset(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context);
}

TEST(DatasetMetadataTest, ListDatasets) {
  auto mock_stub = std::make_shared<MockDatasetRestStub>();
  auto constexpr kExpectedPayload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "next_page_token": "npt-123",
          "datasets": [
              {
                "id": "1",
                "kind": "kind-2",
                "dataset_reference": {"project_id": "p123", "dataset_id": "d123"},

                "friendly_name": "friendly-name",
                "location": "location",
                "type": "DEFAULT"
              }
  ]})";

  EXPECT_CALL(*mock_stub, ListDatasets)
      .WillOnce(
          [&](rest_internal::RestContext&, ListDatasetsRequest const& request)
              -> StatusOr<ListDatasetsResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return ListDatasetsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto metadata = CreateMockDatasetMetadata(std::move(mock_stub));
  ListDatasetsRequest request;
  rest_internal::RestContext context;
  request.set_project_id("test-project-id");

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->ListDatasets(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
