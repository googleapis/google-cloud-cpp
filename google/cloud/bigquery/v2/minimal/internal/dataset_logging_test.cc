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
#include "google/cloud/bigquery/v2/minimal/testing/mock_log_backend.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockDatasetRestStub;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

std::shared_ptr<DatasetLogging> CreateMockDatasetLogging(
    std::shared_ptr<DatasetRestStub> mock) {
  return std::make_shared<DatasetLogging>(std::move(mock), TracingOptions{},
                                          std::set<std::string>{});
}

class DatasetLoggingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend_ =
        std::make_shared<bigquery_v2_minimal_testing::MockLogBackend>();
    log_backend_id_ =
        google::cloud::LogSink::Instance().AddBackend(log_backend_);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id_);
    log_backend_id_ = 0;
    log_backend_.reset();
  }

  std::shared_ptr<bigquery_v2_minimal_testing::MockLogBackend> log_backend_ =
      nullptr;
  long log_backend_id_ = 0;  // NOLINT(google-runtime-int)
};

TEST_F(DatasetLoggingClientTest, GetDataset) {
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

  // Not checking all fields exhaustively since this has already been tested.
  // Just ensuring here that the request and response are being logged.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(GetDatasetRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p-id")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "d-id")"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(GetDatasetResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p-id")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "d-id")"));
      });

  auto client = CreateMockDatasetLogging(std::move(mock_stub));
  GetDatasetRequest request("p-id", "d-id");
  rest_internal::RestContext context;

  client->GetDataset(context, request);
}

TEST_F(DatasetLoggingClientTest, ListDatasets) {
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

  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(ListDatasetsRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(all_datasets: false)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(max_results: 0)"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(ListDatasetsResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "d123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(next_page_token: "npt-123")"));
      });

  auto client = CreateMockDatasetLogging(std::move(mock_stub));
  ListDatasetsRequest request("p123");
  rest_internal::RestContext context;

  client->ListDatasets(context, request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
