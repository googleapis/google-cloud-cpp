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

#include "google/cloud/bigquery/v2/minimal/internal/job_metadata.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/metadata_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::GetMetadataOptions;
using ::google::cloud::bigquery_v2_minimal_testing::MakePartialJob;
using ::google::cloud::bigquery_v2_minimal_testing::MockBigQueryJobRestStub;
using ::google::cloud::bigquery_v2_minimal_testing::VerifyMetadataContext;

using ::testing::IsEmpty;

std::shared_ptr<BigQueryJobMetadata> CreateMockJobMetadata(
    std::shared_ptr<BigQueryJobRestStub> mock) {
  return std::make_shared<BigQueryJobMetadata>(std::move(mock));
}

TEST(JobMetadataTest, GetJob) {
  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "reference": {"project_id": "p123", "job_id": "j123"},
          "configuration": {
            "job_type": "QUERY",
            "query_config": {"query": "select 1;"}
          }})";

  EXPECT_CALL(*mock_stub, GetJob)
      .WillOnce([&](rest_internal::RestContext&,
                    GetJobRequest const& request) -> StatusOr<GetJobResponse> {
        EXPECT_THAT(request.project_id(), Not(IsEmpty()));
        EXPECT_THAT(request.job_id(), Not(IsEmpty()));
        BigQueryHttpResponse http_response;
        http_response.payload = kExpectedPayload;
        return GetJobResponse::BuildFromHttpResponse(std::move(http_response));
      });

  auto metadata = CreateMockJobMetadata(std::move(mock_stub));
  GetJobRequest request;
  rest_internal::RestContext context;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->GetJob(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context, "bigquery_v2_job");
}

TEST(JobMetadataTest, ListJobs) {
  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "next_page_token": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "reference": {"project_id": "p123", "job_id": "j123"},
                "state": "DONE",
                "configuration": {
                   "job_type": "QUERY",
                   "query_config": {"query": "select 1;"}
                },
                "status": {"state": "DONE"},
                "user_email": "user-email",
                "principal_subject": "principal-subj"
              }
  ]})";

  EXPECT_CALL(*mock_stub, ListJobs)
      .WillOnce(
          [&](rest_internal::RestContext&,
              ListJobsRequest const& request) -> StatusOr<ListJobsResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return ListJobsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto metadata = CreateMockJobMetadata(std::move(mock_stub));
  ListJobsRequest request;
  rest_internal::RestContext context;
  request.set_project_id("test-project-id");

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->ListJobs(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context, "bigquery_v2_job");
}

TEST(JobMetadataTest, InsertJob) {
  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "reference": {"project_id": "p123", "job_id": "j123"},
          "configuration": {
            "job_type": "QUERY",
            "query_config": {"query": "select 1;"}
          }})";

  EXPECT_CALL(*mock_stub, InsertJob)
      .WillOnce(
          [&](rest_internal::RestContext&,
              InsertJobRequest const& request) -> StatusOr<InsertJobResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return InsertJobResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto metadata = CreateMockJobMetadata(std::move(mock_stub));

  rest_internal::RestContext context;
  InsertJobRequest request("test-project-id", MakePartialJob());

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->InsertJob(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context, "bigquery_v2_job");
}

TEST(JobMetadataTest, CancelJob) {
  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind":"cancel-job",
          "job":{"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "reference": {"project_id": "p123", "job_id": "j123"},
          "configuration": {
            "job_type": "QUERY",
            "query_config": {"query": "select 1;"}
          }}})";

  EXPECT_CALL(*mock_stub, CancelJob)
      .WillOnce(
          [&](rest_internal::RestContext&,
              CancelJobRequest const& request) -> StatusOr<CancelJobResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            EXPECT_THAT(request.job_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return CancelJobResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto metadata = CreateMockJobMetadata(std::move(mock_stub));

  rest_internal::RestContext context;
  CancelJobRequest request("test-project-id", "test-job-id");

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->CancelJob(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context, "bigquery_v2_job");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
