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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_request.h"
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

TEST(GetDatasetRequestTest, Success) {
  GetDatasetRequest request("1", "2");
  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/datasets/2");
  EXPECT_EQ(expected, *actual);
}

TEST(GetDatasetRequestTest, DatasetEndpoints) {
  GetDatasetRequest request("1", "2");

  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/datasets/2"},
      {"http://myendpoint.google.com",
       "http://myendpoint.google.com/bigquery/v2/projects/1/datasets/2"},
      {"myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/datasets/2"},
      {"https://myendpoint.google.com/",
       "https://myendpoint.google.com/bigquery/v2/projects/1/datasets/2"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for endpoint: " + test.endpoint +
                 ", expected: " + test.expected);
    Options opts;
    opts.set<EndpointOption>(test.endpoint);
    internal::OptionsSpan span(opts);

    auto actual = BuildRestRequest(request);
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ(test.expected, actual->path());
  }
}

TEST(ListDatasetsRequestTest, Success) {
  auto const* filter = "labels.filter1";
  auto const* page_token = "123";

  ListDatasetsRequest request("1");
  request.set_all_datasets(true)
      .set_max_results(10)
      .set_filter(filter)
      .set_page_token(page_token);

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);
  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/datasets");
  expected.AddQueryParameter("all", "true");
  expected.AddQueryParameter("maxResults", "10");
  expected.AddQueryParameter("pageToken", page_token);
  expected.AddQueryParameter("filter", filter);

  EXPECT_EQ(expected, *actual);
}

TEST(GetDatasetRequest, DebugString) {
  GetDatasetRequest request("test-project-id", "test-dataset-id");

  EXPECT_EQ(request.DebugString("GetDatasetRequest", TracingOptions{}),
            R"(GetDatasetRequest {)"
            R"( project_id: "test-project-id")"
            R"( dataset_id: "test-dataset-id")"
            R"( })");
  EXPECT_EQ(request.DebugString("GetDatasetRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(GetDatasetRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( dataset_id: "test-da...<truncated>...")"
            R"( })");
  EXPECT_EQ(
      request.DebugString("GetDatasetRequest",
                          TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(GetDatasetRequest {
  project_id: "test-project-id"
  dataset_id: "test-dataset-id"
})");
}

TEST(ListDatasetsRequestTest, DebugString) {
  ListDatasetsRequest request("test-project-id");
  request.set_all_datasets(true)
      .set_max_results(10)
      .set_page_token("test-page-token")
      .set_filter("labels-filter1");

  EXPECT_EQ(request.DebugString("ListDatasetsRequest", TracingOptions{}),
            R"(ListDatasetsRequest {)"
            R"( project_id: "test-project-id")"
            R"( all_datasets: true)"
            R"( max_results: 10)"
            R"( page_token: "test-page-token")"
            R"( filter: "labels-filter1")"
            R"( })");
  EXPECT_EQ(request.DebugString("ListDatasetsRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(ListDatasetsRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( all_datasets: true)"
            R"( max_results: 10)"
            R"( page_token: "test-pa...<truncated>...")"
            R"( filter: "labels-...<truncated>...")"
            R"( })");
  EXPECT_EQ(
      request.DebugString("ListDatasetsRequest",
                          TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListDatasetsRequest {
  project_id: "test-project-id"
  all_datasets: true
  max_results: 10
  page_token: "test-page-token"
  filter: "labels-filter1"
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
