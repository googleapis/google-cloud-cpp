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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_dataset_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockDatasetRestStub;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

std::shared_ptr<DatasetLogging> CreateMockDatasetLogging(
    std::shared_ptr<DatasetRestStub> mock) {
  return std::make_shared<DatasetLogging>(std::move(mock), TracingOptions{},
                                          std::set<std::string>{});
}

TEST(DatasetLoggingClientTest, GetDataset) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockDatasetRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind": "d-kind",
          "etag": "d-tag",
          "id": "d-id",
          "selfLink": "d-selfLink",
          "friendlyName": "d-friendly-name",
          "datasetReference": {"projectId": "p-id", "datasetId": "d-id"}
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

  auto client = CreateMockDatasetLogging(std::move(mock_stub));
  GetDatasetRequest request("p-id", "d-id");
  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->GetDataset(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetDatasetRequest)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p-id")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(dataset_id: "d-id")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetDatasetResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "d-id")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "d-kind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(etag: "d-tag")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(DatasetLoggingClientTest, ListDatasets) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockDatasetRestStub>();
  auto constexpr kExpectedPayload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "datasets": [
              {
                "id": "1",
                "kind": "kind-1",
                "datasetReference": {"projectId": "p123", "datasetId": "d123"},
                "friendlyName": "friendly-name",
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

  auto client = CreateMockDatasetLogging(std::move(mock_stub));
  ListDatasetsRequest request("p123");
  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->ListDatasets(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListDatasetsRequest)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "p123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(all_datasets: false)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(max_results: 0)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListDatasetsResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(dataset_id: "d123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "kind-1")")));
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
