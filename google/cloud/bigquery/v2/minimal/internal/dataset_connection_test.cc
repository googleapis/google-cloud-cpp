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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_dataset_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockDatasetRestStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

std::shared_ptr<DatasetConnection> CreateTestingConnection(
    std::shared_ptr<DatasetRestStub> mock) {
  DatasetLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  auto options = DatasetDefaultOptions(
      Options{}
          .set<DatasetRetryPolicyOption>(retry.clone())
          .set<DatasetBackoffPolicyOption>(backoff.clone()));
  return std::make_shared<DatasetRestConnectionImpl>(std::move(mock),
                                                     std::move(options));
}

// Verify that the successful case works.
TEST(DatasetConnectionTest, GetDatasetSuccess) {
  auto mock = std::make_shared<MockDatasetRestStub>();

  auto constexpr kExpectedPayload =
      R"({"kind": "d-kind",
          "etag": "d-tag",
          "id": "d-id",
          "selfLink": "d-selfLink",
          "friendlyName": "d-friendly-name",
          "datasetReference": {"projectId": "p-id", "datasetId": "d-id"}
    })";

  EXPECT_CALL(*mock, GetDataset)
      .WillOnce(
          [&](rest_internal::RestContext&, GetDatasetRequest const& request)
              -> StatusOr<GetDatasetResponse> {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("test-dataset-id", request.dataset_id());
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return GetDatasetResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto conn = CreateTestingConnection(std::move(mock));

  GetDatasetRequest request;
  request.set_project_id("test-project-id");
  request.set_dataset_id("test-dataset-id");

  google::cloud::internal::OptionsSpan span(conn->options());
  auto dataset_result = conn->GetDataset(request);

  ASSERT_STATUS_OK(dataset_result);
  EXPECT_EQ(dataset_result->kind, "d-kind");
  EXPECT_EQ(dataset_result->etag, "d-tag");
  EXPECT_EQ(dataset_result->id, "d-id");
  EXPECT_EQ(dataset_result->self_link, "d-selfLink");
  EXPECT_EQ(dataset_result->friendly_name, "d-friendly-name");
  EXPECT_EQ(dataset_result->dataset_reference.project_id, "p-id");
  EXPECT_EQ(dataset_result->dataset_reference.dataset_id, "d-id");
}

TEST(DatasetConnectionTest, ListDatasetsSuccess) {
  auto mock = std::make_shared<MockDatasetRestStub>();

  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce(
          [&](rest_internal::RestContext&, ListDatasetsRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_TRUE(request.page_token().empty());
            ListDatasetsResponse page;
            page.next_page_token = "page-1";
            ListFormatDataset dataset;
            dataset.id = "dataset1";
            page.datasets.push_back(dataset);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListDatasetsRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("page-1", request.page_token());
            ListDatasetsResponse page;
            page.next_page_token = "page-2";
            ListFormatDataset dataset;
            dataset.id = "dataset2";
            page.datasets.push_back(dataset);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListDatasetsRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("page-2", request.page_token());
            ListDatasetsResponse page;
            page.next_page_token = "";
            ListFormatDataset dataset;
            dataset.id = "dataset3";
            page.datasets.push_back(dataset);
            return make_status_or(page);
          });

  auto conn = CreateTestingConnection(std::move(mock));

  std::vector<std::string> actual_dataset_ids;
  ListDatasetsRequest request;
  request.set_project_id("test-project-id");

  google::cloud::internal::OptionsSpan span(conn->options());
  for (auto const& dataset : conn->ListDatasets(request)) {
    ASSERT_STATUS_OK(dataset);
    actual_dataset_ids.push_back(dataset->id);
  }
  EXPECT_THAT(actual_dataset_ids,
              ElementsAre("dataset1", "dataset2", "dataset3"));
}

// Verify that permanent errors are reported immediately.
TEST(DatasetConnectionTest, GetDatasetPermanentError) {
  auto mock = std::make_shared<MockDatasetRestStub>();
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetDatasetRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->GetDataset(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(DatasetConnectionTest, ListDatasetsPermanentError) {
  auto mock = std::make_shared<MockDatasetRestStub>();
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListDatasetsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListDatasets(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

// Verify that too many transients errors are reported correctly.
TEST(DatasetConnectionTest, GetDatasetTooManyTransients) {
  auto mock = std::make_shared<MockDatasetRestStub>();
  EXPECT_CALL(*mock, GetDataset)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kResourceExhausted, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetDatasetRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->GetDataset(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kResourceExhausted, HasSubstr("try-again")));
}

TEST(DatasetConnectionTest, ListDatasetsTooManyTransients) {
  auto mock = std::make_shared<MockDatasetRestStub>();
  EXPECT_CALL(*mock, ListDatasets)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kResourceExhausted, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListDatasetsRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListDatasets(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kResourceExhausted));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
