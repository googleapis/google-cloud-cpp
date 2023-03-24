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

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

ListJobsRequest GetListJobsRequest() {
  ListJobsRequest request("1");
  auto const min = std::chrono::system_clock::now();
  auto const duration = std::chrono::milliseconds(100);
  auto const max = min + duration;
  request.set_all_users(true)
      .set_max_results(10)
      .set_min_creation_time(min)
      .set_max_creation_time(max)
      .set_parent_job_id("1")
      .set_page_token("123")
      .set_projection(Projection::Full())
      .set_state_filter(StateFilter::Running());
  return request;
}

TEST(GetJobRequestTest, SuccessWithLocation) {
  GetJobRequest request("1", "2");
  request.set_location("useast");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  auto actual = BuildRestRequest(request, opts);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2");
  expected.AddQueryParameter("location", "useast");
  EXPECT_EQ(expected, *actual);
}

TEST(GetJobRequestTest, SuccessWithoutLocation) {
  GetJobRequest request("1", "2");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  auto actual = BuildRestRequest(request, opts);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2");
  EXPECT_EQ(expected, *actual);
}

TEST(GetJobRequestTest, SuccessWithEndpoint) {
  GetJobRequest request("1", "2");

  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
      {"http://myendpoint.google.com",
       "http://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
      {"myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
      {"https://myendpoint.google.com/",
       "https://myendpoint.google.com/bigquery/v2/projects/1/jobs/2"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for endpoint: " + test.endpoint +
                 ", expected: " + test.expected);
    Options opts;
    opts.set<EndpointOption>(test.endpoint);
    auto actual = BuildRestRequest(request, opts);
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ(test.expected, actual->path());
  }
}

TEST(GetJobRequestTest, EmptyProjectId) {
  GetJobRequest request("", "test-job-id");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  auto rest_request = BuildRestRequest(request, opts);
  EXPECT_THAT(rest_request, StatusIs(StatusCode::kInvalidArgument,
                                     HasSubstr("Project Id is empty")));
}

TEST(GetJobRequest, EmptyJobId) {
  GetJobRequest request("test-project-id", "");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  auto rest_request = BuildRestRequest(request, opts);
  EXPECT_THAT(rest_request, StatusIs(StatusCode::kInvalidArgument,
                                     HasSubstr("Job Id is empty")));
}

TEST(GetJobRequest, OutputStream) {
  GetJobRequest request("test-project-id", "test-job-id");
  request.set_location("test-location");
  std::string expected = absl::StrCat(
      "GetJobRequest{project_id=", request.project_id(),
      ", job_id=", request.job_id(), ", location=", request.location(), "}");
  std::ostringstream os;
  os << request;
  EXPECT_EQ(expected, os.str());
}

TEST(ListJobsRequestTest, Success) {
  auto const request = GetListJobsRequest();
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  auto actual = BuildRestRequest(request, opts);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs");
  expected.AddQueryParameter("allUsers", "true");
  expected.AddQueryParameter("maxResults", "10");
  expected.AddQueryParameter(
      "minCreationTime", internal::FormatRfc3339(request.min_creation_time()));
  expected.AddQueryParameter(
      "maxCreationTime", internal::FormatRfc3339(request.max_creation_time()));
  expected.AddQueryParameter("pageToken", "123");
  expected.AddQueryParameter("projection", "FULL");
  expected.AddQueryParameter("stateFilter", "RUNNING");
  expected.AddQueryParameter("parentJobId", "1");

  EXPECT_EQ(expected, *actual);
}

TEST(ListJobsRequestTest, EmptyProjectId) {
  ListJobsRequest request("");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  auto rest_request = BuildRestRequest(request, opts);
  EXPECT_THAT(rest_request, StatusIs(StatusCode::kInvalidArgument,
                                     HasSubstr("Project Id is empty")));
}

TEST(ListJobsRequestTest, OutputStream) {
  auto const request = GetListJobsRequest();
  std::string all_users = request.all_users() ? "true" : "false";
  std::string expected = absl::StrCat(
      "ListJobsRequest{project_id=", request.project_id(),
      ", all_users=", all_users, ", max_results=", request.max_results(),
      ", page_token=", request.page_token(),
      ", projection=", request.projection().value,
      ", state_filter=", request.state_filter().value,
      ", parent_job_id=", request.parent_job_id(), "}");
  std::ostringstream os;
  os << request;
  EXPECT_EQ(expected, os.str());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
