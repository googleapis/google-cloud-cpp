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

#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_job_connection.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::StatusOr;
using ::google::cloud::bigquery_v2_minimal_mocks::MockBigQueryJobConnection;
using ::google::cloud::rest_internal::HttpStatusCode;
using ::testing::Eq;

TEST(JobConnectionTest, GetJobSuccess) {
  // Create Job mock connection.
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  // Setup expectations on the mock.
  std::string expected_payload =
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
  EXPECT_CALL(*mock_job_connection, GetJob)
      .WillOnce([&](GetJobRequest const& request) {
        EXPECT_EQ("test-project-id", request.project_id());
        EXPECT_EQ("test-job-id", request.job_id());
        BigQueryHttpResponse http_response;
        http_response.payload = expected_payload;
        auto const& job_response =
            GetJobResponse::BuildFromHttpResponse(http_response);
        return google::cloud::StatusOr<GetJobResponse>(job_response);
      });

  // Create Job client with mock connection.
  JobClient job_client(mock_job_connection);

  // Call the Job client.
  GetJobRequest request;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");
  Options opts;

  auto result = job_client.GetJob(request, opts);

  // Verify results.
  ASSERT_STATUS_OK(result);
  EXPECT_FALSE(result->http_response.payload.empty());
  EXPECT_THAT(result->job.kind, Eq("jkind"));
  EXPECT_THAT(result->job.etag, Eq("jtag"));
  EXPECT_THAT(result->job.id, Eq("j123"));
  EXPECT_THAT(result->job.self_link, Eq("jselfLink"));
  EXPECT_THAT(result->job.user_email, Eq("juserEmail"));
  EXPECT_THAT(result->job.status.state, Eq("DONE"));
  EXPECT_THAT(result->job.reference.project_id, Eq("p123"));
  EXPECT_THAT(result->job.reference.job_id, Eq("j123"));
  EXPECT_THAT(result->job.configuration.job_type, Eq("QUERY"));
  EXPECT_THAT(result->job.configuration.query_config.query, Eq("select 1;"));
}

TEST(JobConnectionTest, GetJobFailureInvalidArgument) {
  // Create Job mock connection.
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();
  // Setup Failure expectations
  EXPECT_CALL(*mock_job_connection, GetJob)
      .WillOnce([&](GetJobRequest const& request) {
        if (request.project_id().empty() || request.job_id().empty()) {
          return ::google::cloud::rest_internal::AsStatus(
              HttpStatusCode::kBadRequest, "");
        }
        return ::google::cloud::rest_internal::AsStatus(
            HttpStatusCode::kInternalServerError, "");
      });
  // Create Job client with mock connection.
  JobClient job_client(mock_job_connection);

  // Call the Job client.
  GetJobRequest request;
  Options opts;

  auto result = job_client.GetJob(request, opts);
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().code(), Eq(StatusCode::kInvalidArgument));
}

TEST(JobConnectionTest, GetJobFailureInternalError) {
  // Create Job mock connection.
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();
  // Setup Failure expectations
  EXPECT_CALL(*mock_job_connection, GetJob)
      .WillOnce([&](GetJobRequest const& request) {
        if (request.project_id().empty() || request.job_id().empty()) {
          return ::google::cloud::rest_internal::AsStatus(
              HttpStatusCode::kBadRequest, "");
        }
        return ::google::cloud::rest_internal::AsStatus(
            HttpStatusCode::kInternalServerError, "");
      });
  // Create Job client with mock connection.
  JobClient job_client(mock_job_connection);

  // Call the Job client.
  GetJobRequest request;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");
  Options opts;

  auto result = job_client.GetJob(request, opts);
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().code(), Eq(StatusCode::kUnavailable));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
