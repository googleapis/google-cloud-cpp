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
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockBigQueryJobRestStub;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

static auto const kUserProject = "test-only-project";
static auto const kQuotaUser = "test-quota-user";

std::shared_ptr<BigQueryJobMetadata> CreateMockJobMetadata(
    std::shared_ptr<BigQueryJobRestStub> mock) {
  return std::make_shared<BigQueryJobMetadata>(std::move(mock));
}

void VerifyMetadataContext(rest_internal::RestContext& context) {
  EXPECT_THAT(context.GetHeader("x-goog-api-client"),
              Contains(HasSubstr("bigquery_v2_job")));
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
  VerifyMetadataContext(context);
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
  VerifyMetadataContext(context);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
