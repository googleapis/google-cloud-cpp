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

#include "google/cloud/bigquery/v2/minimal/internal/project_request.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(ListProjectsRequestTest, Success) {
  auto const* page_token = "123";

  ListProjectsRequest request;
  request.set_max_results(10).set_page_token(page_token);

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);
  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath("https://bigquery.googleapis.com/bigquery/v2/projects");
  expected.AddQueryParameter("maxResults", "10");
  expected.AddQueryParameter("pageToken", page_token);

  EXPECT_EQ(expected, *actual);
}

TEST(ListProjectsRequestTest, DebugString) {
  ListProjectsRequest request;
  request.set_max_results(10).set_page_token("test-page-token");

  EXPECT_EQ(request.DebugString("ListProjectsRequest", TracingOptions{}),
            R"(ListProjectsRequest {)"
            R"( max_results: 10)"
            R"( page_token: "test-page-token")"
            R"( })");

  EXPECT_EQ(request.DebugString("ListProjectsRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(ListProjectsRequest {)"
            R"( max_results: 10)"
            R"( page_token: "test-pa...<truncated>...")"
            R"( })");

  EXPECT_EQ(
      request.DebugString("ListProjectsRequest",
                          TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListProjectsRequest {
  max_results: 10
  page_token: "test-page-token"
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
