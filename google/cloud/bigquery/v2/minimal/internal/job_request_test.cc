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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

TEST(JobRequestTest, SuccessWithLocation) {
  GetJobRequest request("1", "2");
  request.set_location("useast");
  Options opts;
  auto actual = BuildRestRequest(request, opts);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2");
  expected.AddQueryParameter("location", "useast");
  EXPECT_EQ(expected, *actual);
}

TEST(JobRequestTest, SuccessWithoutLocation) {
  GetJobRequest request("1", "2");
  Options opts;
  auto actual = BuildRestRequest(request, opts);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/jobs/2");
  EXPECT_EQ(expected, *actual);
}

TEST(JobRequestTest, SuccessWithEndpoint) {
  GetJobRequest request("1", "2");
  Options opts;

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
    opts.set<EndpointOption>(test.endpoint);
    auto actual = BuildRestRequest(request, opts);
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ(test.expected, actual.value().path());
  }
}

TEST(JobRequestTest, EmptyProjectId) {
  GetJobRequest request("", "job_id");
  Options opts;
  auto rest_request = BuildRestRequest(request, opts);
  EXPECT_THAT(rest_request, StatusIs(StatusCode::kInvalidArgument,
                                     HasSubstr("Project Id is empty")));
}

TEST(GetJobRequest, EmptyJobId) {
  GetJobRequest request("project_id", "");
  Options opts;
  auto rest_request = BuildRestRequest(request, opts);
  EXPECT_THAT(rest_request, StatusIs(StatusCode::kInvalidArgument,
                                     HasSubstr("Job Id is empty")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
