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

#include "google/cloud/bigquery/v2/minimal/internal/job_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockBigQueryJobRestStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;

std::shared_ptr<BigQueryJobConnection> CreateTestingConnection(
    std::shared_ptr<BigQueryJobRestStub> mock) {
  BigQueryJobLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  auto options = BigQueryJobDefaultOptions(
      Options{}
          .set<BigQueryJobRetryPolicyOption>(retry.clone())
          .set<BigQueryJobBackoffPolicyOption>(backoff.clone()));
  return std::make_shared<BigQueryJobRestConnectionImpl>(std::move(mock),
                                                         std::move(options));
}

// Verify that the successful case works.
TEST(JobConnectionTest, GetJobSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

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

  EXPECT_CALL(*mock, GetJob)
      .WillOnce([&](rest_internal::RestContext&,
                    GetJobRequest const& request) -> StatusOr<GetJobResponse> {
        EXPECT_EQ("test-project-id", request.project_id());
        EXPECT_EQ("test-job-id", request.job_id());
        BigQueryHttpResponse http_response;
        http_response.payload = kExpectedPayload;
        return GetJobResponse::BuildFromHttpResponse(std::move(http_response));
      });

  auto conn = CreateTestingConnection(std::move(mock));

  GetJobRequest request;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");

  auto result = conn->GetJob(request);

  ASSERT_STATUS_OK(result);
  EXPECT_FALSE(result->http_response.payload.empty());
  EXPECT_EQ(result->job.kind, "jkind");
  EXPECT_EQ(result->job.etag, "jtag");
  EXPECT_EQ(result->job.id, "j123");
  EXPECT_EQ(result->job.self_link, "jselfLink");
  EXPECT_EQ(result->job.user_email, "juserEmail");
  EXPECT_EQ(result->job.status.state, "DONE");
  EXPECT_EQ(result->job.reference.project_id, "p123");
  EXPECT_EQ(result->job.reference.job_id, "j123");
  EXPECT_EQ(result->job.configuration.job_type, "QUERY");
  EXPECT_EQ(result->job.configuration.query_config.query, "select 1;");
}

TEST(JobConnectionTest, ListJobsSuccess) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();

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

  EXPECT_CALL(*mock, ListJobs)
      .WillOnce(
          [&](rest_internal::RestContext&,
              ListJobsRequest const& request) -> StatusOr<ListJobsResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return ListJobsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto conn = CreateTestingConnection(std::move(mock));

  ListJobsRequest request;
  request.set_project_id("test-project-id");

  auto list_jobs_response = conn->ListJobs(request);

  ASSERT_STATUS_OK(list_jobs_response);
  EXPECT_EQ(list_jobs_response->kind, "kind-1");
  EXPECT_EQ(list_jobs_response->etag, "tag-1");
  EXPECT_EQ(list_jobs_response->next_page_token, "npt-123");

  auto const jobs = list_jobs_response->jobs;
  ASSERT_EQ(jobs.size(), 1);
  EXPECT_EQ(jobs[0].id, "1");
  EXPECT_EQ(jobs[0].kind, "kind-2");
  EXPECT_EQ(jobs[0].status.state, "DONE");
  EXPECT_EQ(jobs[0].state, "DONE");
  EXPECT_EQ(jobs[0].user_email, "user-email");
  EXPECT_EQ(jobs[0].reference.project_id, "p123");
  EXPECT_EQ(jobs[0].reference.job_id, "j123");
  EXPECT_EQ(jobs[0].configuration.job_type, "QUERY");
  EXPECT_EQ(jobs[0].configuration.query_config.query, "select 1;");
}

// Verify that permanent errors are reported immediately.
TEST(JobConnectionTest, GetJobPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, GetJob)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));
  GetJobRequest request;
  auto result = conn->GetJob(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(JobConnectionTest, ListJobsPermanentError) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, ListJobs)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));
  ListJobsRequest request;
  auto result = conn->ListJobs(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

// Verify that too many transients errors are reported correctly.
TEST(JobConnectionTest, GetJobTooManyTransients) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, GetJob)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  GetJobRequest request;
  auto result = conn->GetJob(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

TEST(JobConnectionTest, ListJobsTooManyTransients) {
  auto mock = std::make_shared<MockBigQueryJobRestStub>();
  EXPECT_CALL(*mock, ListJobs)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ListJobsRequest request;
  auto result = conn->ListJobs(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try-again")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
