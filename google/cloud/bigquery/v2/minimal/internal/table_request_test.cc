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

#include "google/cloud/bigquery/v2/minimal/internal/table_request.h"
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

TEST(GetTableRequestTest, SingleSelectedField) {
  std::vector<std::string> fields;
  fields.emplace_back("f1");
  auto view = TableMetadataView::Basic();

  GetTableRequest request("1", "2", "3");
  request.set_selected_fields(fields);
  request.set_view(view);

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");

  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/datasets/2/"
      "tables/3");
  expected.AddQueryParameter("selectedFields", "f1");
  expected.AddQueryParameter("view", "BASIC");

  EXPECT_EQ(expected, *actual);
}

TEST(GetTableRequestTest, MultipleSelectedFields) {
  std::vector<std::string> fields;
  fields.emplace_back("f1");
  fields.emplace_back("f2");
  fields.emplace_back("f3");
  auto view = TableMetadataView::Basic();

  GetTableRequest request("1", "2", "3");
  request.set_selected_fields(fields);
  request.set_view(view);

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");

  internal::OptionsSpan span(opts);

  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/datasets/2/"
      "tables/3");
  expected.AddQueryParameter("selectedFields", "f1,f2,f3");
  expected.AddQueryParameter("view", "BASIC");

  EXPECT_EQ(expected, *actual);
}

TEST(GetTableRequestTest, TableEndpoints) {
  GetTableRequest request("1", "2", "3");

  struct EndpointTest {
    std::string endpoint;
    std::string expected;
  } cases[] = {
      {"https://myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/datasets/2/tables/"
       "3"},
      {"http://myendpoint.google.com",
       "http://myendpoint.google.com/bigquery/v2/projects/1/datasets/2/tables/"
       "3"},
      {"myendpoint.google.com",
       "https://myendpoint.google.com/bigquery/v2/projects/1/datasets/2/tables/"
       "3"},
      {"https://myendpoint.google.com/",
       "https://myendpoint.google.com/bigquery/v2/projects/1/datasets/2/tables/"
       "3"},
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

TEST(ListTablesRequestTest, Success) {
  auto const* page_token = "123";

  ListTablesRequest request("1", "2");
  request.set_max_results(10).set_page_token(page_token);

  Options opts;
  opts.set<EndpointOption>("bigquery.googleapis.com");
  internal::OptionsSpan span(opts);
  auto actual = BuildRestRequest(request);
  ASSERT_STATUS_OK(actual);

  rest_internal::RestRequest expected;
  expected.SetPath(
      "https://bigquery.googleapis.com/bigquery/v2/projects/1/datasets/2/"
      "tables");
  expected.AddQueryParameter("maxResults", "10");
  expected.AddQueryParameter("pageToken", page_token);

  EXPECT_EQ(expected, *actual);
}

TEST(GetTableRequest, DebugString) {
  GetTableRequest request("test-project-id", "test-dataset-id",
                          "test-table-id");
  std::vector<std::string> fields;
  fields.emplace_back("f1");
  fields.emplace_back("f2");
  auto view = TableMetadataView::Basic();

  request.set_selected_fields(fields);
  request.set_view(view);

  EXPECT_EQ(request.DebugString("GetTableRequest", TracingOptions{}),
            R"(GetTableRequest {)"
            R"( project_id: "test-project-id")"
            R"( dataset_id: "test-dataset-id")"
            R"( table_id: "test-table-id")"
            R"( selected_fields: "f1")"
            R"( selected_fields: "f2")"
            R"( view { value: "BASIC" })"
            R"( })");

  EXPECT_EQ(request.DebugString("GetTableRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(GetTableRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( dataset_id: "test-da...<truncated>...")"
            R"( table_id: "test-ta...<truncated>...")"
            R"( selected_fields: "f1")"
            R"( selected_fields: "f2")"
            R"( view { value: "BASIC" })"
            R"( })");

  EXPECT_EQ(request.DebugString("GetTableRequest", TracingOptions{}.SetOptions(
                                                       "single_line_mode=F")),
            R"(GetTableRequest {
  project_id: "test-project-id"
  dataset_id: "test-dataset-id"
  table_id: "test-table-id"
  selected_fields: "f1"
  selected_fields: "f2"
  view {
    value: "BASIC"
  }
})");
}

TEST(ListTablesRequestTest, DebugString) {
  ListTablesRequest request("test-project-id", "test-dataset-id");
  request.set_max_results(10).set_page_token("test-page-token");

  EXPECT_EQ(request.DebugString("ListTablesRequest", TracingOptions{}),
            R"(ListTablesRequest {)"
            R"( project_id: "test-project-id")"
            R"( dataset_id: "test-dataset-id")"
            R"( max_results: 10)"
            R"( page_token: "test-page-token" })");

  EXPECT_EQ(request.DebugString("ListTablesRequest",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=7")),
            R"(ListTablesRequest {)"
            R"( project_id: "test-pr...<truncated>...")"
            R"( dataset_id: "test-da...<truncated>...")"
            R"( max_results: 10)"
            R"( page_token: "test-pa...<truncated>..." })");

  EXPECT_EQ(
      request.DebugString("ListTablesRequest",
                          TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListTablesRequest {
  project_id: "test-project-id"
  dataset_id: "test-dataset-id"
  max_results: 10
  page_token: "test-page-token"
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
