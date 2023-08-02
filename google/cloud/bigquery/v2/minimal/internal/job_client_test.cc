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
#include "google/cloud/bigquery/v2/minimal/testing/job_query_test_utils.h"
#include "google/cloud/bigquery/v2/minimal/testing/job_test_utils.h"
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

using ::google::cloud::bigquery_v2_minimal_testing::
    MakeFullGetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_testing::MakeGetQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::MakePartialJob;
using ::google::cloud::bigquery_v2_minimal_testing::MakePostQueryResults;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryRequest;
using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

TEST(JobClientTest, GetJobSuccess) {
  auto job = MakePartialJob();
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, GetJob)
      .WillOnce([&](GetJobRequest const& request) {
        EXPECT_EQ("test-project-id", request.project_id());
        EXPECT_EQ("test-job-id", request.job_id());
        return make_status_or(job);
      });

  JobClient job_client(mock_job_connection);

  GetJobRequest request;
  request.set_project_id("test-project-id");
  request.set_job_id("test-job-id");

  auto actual_job = job_client.GetJob(request);

  ASSERT_STATUS_OK(actual_job);
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, *actual_job);
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
    return mocks::MakeStreamRange<ListFormatJob>(
        {}, internal::PermissionDeniedError("denied"));
    ;
  });

  JobClient client(std::move(mock));
  ListJobsRequest request;
  request.set_project_id("test-project-id");

  auto range = client.ListJobs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(JobClientTest, InsertJobSuccess) {
  auto job = MakePartialJob();
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, InsertJob)
      .WillOnce([&](InsertJobRequest const& request) {
        EXPECT_EQ("test-project-id", request.project_id());
        return make_status_or(job);
      });

  JobClient job_client(mock_job_connection);

  InsertJobRequest request("test-project-id", job);

  auto actual_job = job_client.InsertJob(request);

  ASSERT_STATUS_OK(actual_job);
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, *actual_job);
}

TEST(JobClientTest, InsertJobFailure) {
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, InsertJob)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  JobClient job_client(mock_job_connection);

  auto result = job_client.InsertJob(InsertJobRequest());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

TEST(JobClientTest, CancelJobSuccess) {
  auto job = MakePartialJob();
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, CancelJob)
      .WillOnce([&](CancelJobRequest const& request) {
        EXPECT_EQ("p123", request.project_id());
        EXPECT_EQ("j123", request.job_id());
        return make_status_or(job);
      });

  JobClient job_client(mock_job_connection);

  CancelJobRequest request("p123", "j123");

  auto actual_job = job_client.CancelJob(request);

  ASSERT_STATUS_OK(actual_job);
  bigquery_v2_minimal_testing::AssertEqualsPartial(job, *actual_job);
}

TEST(JobClientTest, CancelJobFailure) {
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, CancelJob)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  JobClient job_client(mock_job_connection);

  auto result = job_client.CancelJob(CancelJobRequest());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

TEST(JobClientTest, QuerySuccess) {
  auto query_results = MakePostQueryResults();
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, Query)
      .WillOnce([&](PostQueryRequest const& request) {
        EXPECT_EQ("p123", request.project_id());
        return make_status_or(query_results);
      });

  JobClient job_client(mock_job_connection);

  PostQueryRequest job_request;
  job_request.set_project_id("p123");
  job_request.set_query_request(MakeQueryRequest());

  auto actual_query_results = job_client.Query(job_request);

  ASSERT_STATUS_OK(actual_query_results);
  bigquery_v2_minimal_testing::AssertEquals(query_results,
                                            *actual_query_results);
}

TEST(JobClientTest, QueryFailure) {
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, Query)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  JobClient job_client(mock_job_connection);

  auto result = job_client.Query(PostQueryRequest());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

TEST(JobClientTest, QueryResultsSuccess) {
  auto get_query_results = MakeGetQueryResults();
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_THAT(request.project_id(), Not(IsEmpty()));
        return make_status_or(get_query_results);
      });

  JobClient job_client(mock_job_connection);

  GetQueryResultsRequest job_request = MakeFullGetQueryResultsRequest();

  auto actual_query_results = job_client.QueryResults(job_request);

  ASSERT_STATUS_OK(actual_query_results);
  bigquery_v2_minimal_testing::AssertEquals(get_query_results,
                                            *actual_query_results);
}

TEST(JobClientTest, QueryResultsFailure) {
  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();

  EXPECT_CALL(*mock_job_connection, QueryResults)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  JobClient job_client(mock_job_connection);

  auto result = job_client.QueryResults(GetQueryResultsRequest());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
