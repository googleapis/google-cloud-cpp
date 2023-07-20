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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_dataset_connection.h"
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

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Return;

Dataset MakeTestDataset() {
  Dataset dataset;
  dataset.etag = "d-tag";
  dataset.kind = "d-kind";
  dataset.id = "d-id";
  dataset.self_link = "d-self-link";
  dataset.friendly_name = "d-friendly-name";
  dataset.dataset_reference.project_id = "p-id";
  dataset.dataset_reference.dataset_id = "d-id";
  return dataset;
}

TEST(DatasetClientTest, GetDatasetSuccess) {
  auto mock_dataset_connection = std::make_shared<MockDatasetConnection>();

  EXPECT_CALL(*mock_dataset_connection, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ("test-project-id", request.project_id());
        EXPECT_EQ("test-dataset-id", request.dataset_id());
        return make_status_or(MakeTestDataset());
      });

  DatasetClient dataset_client(mock_dataset_connection);

  GetDatasetRequest request;
  request.set_project_id("test-project-id");
  request.set_dataset_id("test-dataset-id");

  auto dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
  EXPECT_EQ(dataset->kind, "d-kind");
  EXPECT_EQ(dataset->etag, "d-tag");
  EXPECT_EQ(dataset->id, "d-id");
  EXPECT_EQ(dataset->self_link, "d-self-link");
  EXPECT_EQ(dataset->friendly_name, "d-friendly-name");
  EXPECT_EQ(dataset->dataset_reference.project_id, "p-id");
  EXPECT_EQ(dataset->dataset_reference.dataset_id, "d-id");
}

TEST(DatasetClientTest, GetDatasetFailure) {
  auto mock_dataset_connection = std::make_shared<MockDatasetConnection>();

  EXPECT_CALL(*mock_dataset_connection, GetDataset)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  DatasetClient dataset_client(mock_dataset_connection);

  GetDatasetRequest request;

  auto result = dataset_client.GetDataset(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

TEST(DatasetClientTest, ListDatasets) {
  auto mock = std::make_shared<MockDatasetConnection>();
  EXPECT_CALL(*mock, options);

  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([](ListDatasetsRequest const& request) {
        EXPECT_EQ(request.project_id(), "test-project-id");
        return mocks::MakeStreamRange<ListFormatDataset>(
            {}, internal::PermissionDeniedError("denied"));
        ;
      });

  DatasetClient client(std::move(mock));
  ListDatasetsRequest request;
  request.set_project_id("test-project-id");

  auto range = client.ListDatasets(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
