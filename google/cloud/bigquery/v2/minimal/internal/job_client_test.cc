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

TEST(JobClientTest, GetJobSuccess) {
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

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
        return make_status_or(job_response->job);
      });

  JobClient job_client(mock_job_connection);

  GetJobRequest request;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");

  auto job = job_client.GetJob(request);

  ASSERT_STATUS_OK(job);
  EXPECT_EQ(job->kind, "jkind");
  EXPECT_EQ(job->etag, "jtag");
  EXPECT_EQ(job->id, "j123");
  EXPECT_EQ(job->self_link, "jselfLink");
  EXPECT_EQ(job->user_email, "juserEmail");
  EXPECT_EQ(job->status.state, "DONE");
  EXPECT_EQ(job->reference.project_id, "p123");
  EXPECT_EQ(job->reference.job_id, "j123");
  EXPECT_EQ(job->configuration.job_type, "QUERY");
  EXPECT_EQ(job->configuration.query_config.query, "select 1;");
}

TEST(JobClientTest, GetJobFailure) {
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, GetJob)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  JobClient job_client(mock_job_connection);

  GetJobRequest request;

  auto result = job_client.GetJob(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

TEST(JobClientTest, ListJobs) {
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);

  EXPECT_CALL(*mock, ListJobs).WillOnce([](ListJobsRequest const& request) {
    EXPECT_EQ(request.project_id(), "test-project-id");
    return google::cloud::internal::MakePaginationRange<
        StreamRange<ListFormatJob>>(
        ListJobsRequest{},
        [](ListJobsRequest const&) {
          return StatusOr<ListJobsResponse>(
              Status(StatusCode::kPermissionDenied, "denied"));
        },
        [](ListJobsResponse const&) { return std::vector<ListFormatJob>{}; });
  });

  JobClient client(std::move(mock));
  ListJobsRequest request;
  request.set_project_id("test-project-id");

  auto range = client.ListJobs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
