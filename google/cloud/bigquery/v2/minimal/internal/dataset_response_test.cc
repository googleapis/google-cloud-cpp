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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(GetDatasetResponseTest, Success) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "d-kind",
          "etag": "d-tag",
          "id": "d-id",
          "selfLink": "d-selfLink",
          "friendlyName": "d-friendly-name",
          "datasetReference": {"projectId": "p-id", "datasetId": "d-id"}
    })";
  auto const response =
      GetDatasetResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->http_response.payload.empty());
  EXPECT_EQ(response->dataset.kind, "d-kind");
  EXPECT_EQ(response->dataset.etag, "d-tag");
  EXPECT_EQ(response->dataset.id, "d-id");
  EXPECT_EQ(response->dataset.self_link, "d-selfLink");
  EXPECT_EQ(response->dataset.friendly_name, "d-friendly-name");
  EXPECT_EQ(response->dataset.dataset_reference.project_id, "p-id");
  EXPECT_EQ(response->dataset.dataset_reference.dataset_id, "d-id");
}

TEST(GetDatasetResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response =
      GetDatasetResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(GetDatasetResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Help! I am not json";
  auto const response =
      GetDatasetResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(GetDatasetResponseTest, InvalidDataset) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "dkind",
          "etag": "dtag",
          "id": "jd123",
          "selfLink": "dselfLink"})";
  auto const response =
      GetDatasetResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("Not a valid Json Dataset object")));
}

TEST(ListDatasetsResponseTest, SuccessMultiplePages) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "datasets": [
              {
                "id": "1",
                "kind": "kind-2",
                "datasetReference": {"projectId": "p123", "datasetId": "d123"},
                "friendlyName": "friendly-name",
                "location": "location",
                "type": "DEFAULT"
              }
  ]})";
  auto const list_datasets_response =
      ListDatasetsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_datasets_response);
  EXPECT_FALSE(list_datasets_response->http_response.payload.empty());
  EXPECT_EQ(list_datasets_response->kind, "kind-1");
  EXPECT_EQ(list_datasets_response->etag, "tag-1");
  EXPECT_EQ(list_datasets_response->next_page_token, "npt-123");

  auto const datasets = list_datasets_response->datasets;
  ASSERT_EQ(datasets.size(), 1);
  EXPECT_EQ(datasets[0].id, "1");
  EXPECT_EQ(datasets[0].kind, "kind-2");
  EXPECT_EQ(datasets[0].friendly_name, "friendly-name");
  EXPECT_EQ(datasets[0].dataset_reference.project_id, "p123");
  EXPECT_EQ(datasets[0].dataset_reference.dataset_id, "d123");
  EXPECT_EQ(datasets[0].location, "location");
  EXPECT_EQ(datasets[0].type, "DEFAULT");
}

TEST(ListDatasetsResponseTest, SuccessSinglePage) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "datasets": [
              {
                "id": "1",
                "kind": "kind-2",
                "datasetReference": {"projectId": "p123", "datasetId": "d123"},
                "friendlyName": "friendly-name",
                "location": "location",
                "type": "DEFAULT"
              }
  ]})";
  auto const list_datasets_response =
      ListDatasetsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(list_datasets_response);
  EXPECT_FALSE(list_datasets_response->http_response.payload.empty());
  EXPECT_EQ(list_datasets_response->kind, "kind-1");
  EXPECT_EQ(list_datasets_response->etag, "tag-1");
  EXPECT_THAT(list_datasets_response->next_page_token, IsEmpty());

  auto const datasets = list_datasets_response->datasets;
  ASSERT_EQ(datasets.size(), 1);
  EXPECT_EQ(datasets[0].id, "1");
  EXPECT_EQ(datasets[0].kind, "kind-2");
  EXPECT_EQ(datasets[0].friendly_name, "friendly-name");
  EXPECT_EQ(datasets[0].dataset_reference.project_id, "p123");
  EXPECT_EQ(datasets[0].dataset_reference.dataset_id, "d123");
  EXPECT_EQ(datasets[0].location, "location");
  EXPECT_EQ(datasets[0].type, "DEFAULT");
}

TEST(ListDatasetsResponseTest, EmptyPayload) {
  BigQueryHttpResponse http_response;
  auto const response =
      ListDatasetsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListDatasetsResponseTest, InvalidJson) {
  BigQueryHttpResponse http_response;
  http_response.payload = "Invalid";
  auto const response =
      ListDatasetsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Error parsing Json from response payload")));
}

TEST(ListDatasetsResponseTest, EmptyDatasetList) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"kind": "dkind",
          "etag": "dtag"})";
  auto const response =
      ListDatasetsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(response->http_response.payload, Not(IsEmpty()));
  EXPECT_EQ(response->kind, "dkind");
  EXPECT_EQ(response->etag, "dtag");
  EXPECT_THAT(response->datasets, IsEmpty());
}

TEST(ListDatasetsResponseTest, InvalidListFormatDataset) {
  BigQueryHttpResponse http_response;
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "datasets": [
              {
                "id": "1",
                "kind": "kind-2"
              }
  ]})";
  auto const response =
      ListDatasetsResponse::BuildFromHttpResponse(http_response);
  EXPECT_THAT(response,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Not a valid Json ListFormatDataset object")));
}

TEST(GetDatasetResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"kind": "d-kind",
          "etag": "d-tag",
          "id": "d-id",
          "selfLink": "d-selfLink",
          "friendlyName": "d-friendly-name",
          "datasetReference": {"projectId": "p-id", "datasetId": "d-id"}
    })";
  auto response = GetDatasetResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(response->DebugString("GetDatasetResponse", TracingOptions{}),
            R"(GetDatasetResponse {)"
            R"( dataset {)"
            R"( kind: "d-kind")"
            R"( etag: "d-tag")"
            R"( id: "d-id")"
            R"( self_link: "d-selfLink")"
            R"( friendly_name: "d-friendly-name")"
            R"( description: "")"
            R"( type: "")"
            R"( location: "")"
            R"( default_collation: "")"
            R"( published: false)"
            R"( is_case_insensitive: false)"
            R"( default_table_expiration { "0" })"
            R"( default_partition_expiration { "0" })"
            R"( creation_time { "1970-01-01T00:00:00Z" })"
            R"( last_modified_time { "1970-01-01T00:00:00Z" })"
            R"( max_time_travel { "0" })"
            R"( dataset_reference {)"
            R"( project_id: "p-id")"
            R"( dataset_id: "d-id")"
            R"( })"
            R"( linked_dataset_source {)"
            R"( source_dataset {)"
            R"( project_id: "")"
            R"( dataset_id: "")"
            R"( })"
            R"( })"
            R"( default_rounding_mode {)"
            R"( value: "")"
            R"( })"
            R"( storage_billing_model {)"
            R"( storage_billing_model_value: "")"
            R"( })"
            R"( })"
            R"( http_response {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED })"
            R"( })");

  EXPECT_EQ(response->DebugString("GetDatasetResponse",
                                  TracingOptions{}.SetOptions(
                                      "truncate_string_field_longer_than=7")),
            R"(GetDatasetResponse {)"
            R"( dataset {)"
            R"( kind: "d-kind")"
            R"( etag: "d-tag")"
            R"( id: "d-id")"
            R"( self_link: "d-selfL...<truncated>...")"
            R"( friendly_name: "d-frien...<truncated>...")"
            R"( description: "")"
            R"( type: "")"
            R"( location: "")"
            R"( default_collation: "")"
            R"( published: false)"
            R"( is_case_insensitive: false)"
            R"( default_table_expiration { "0" })"
            R"( default_partition_expiration { "0" })"
            R"( creation_time { "1970-01-01T00:00:00Z" })"
            R"( last_modified_time { "1970-01-01T00:00:00Z" })"
            R"( max_time_travel { "0" })"
            R"( dataset_reference {)"
            R"( project_id: "p-id")"
            R"( dataset_id: "d-id")"
            R"( })"
            R"( linked_dataset_source {)"
            R"( source_dataset {)"
            R"( project_id: "")"
            R"( dataset_id: "")"
            R"( })"
            R"( })"
            R"( default_rounding_mode {)"
            R"( value: "")"
            R"( })"
            R"( storage_billing_model {)"
            R"( storage_billing_model_value: "")"
            R"( })"
            R"( })"
            R"( http_response {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED })"
            R"( })");

  EXPECT_EQ(
      response->DebugString("GetDatasetResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(GetDatasetResponse {
  dataset {
    kind: "d-kind"
    etag: "d-tag"
    id: "d-id"
    self_link: "d-selfLink"
    friendly_name: "d-friendly-name"
    description: ""
    type: ""
    location: ""
    default_collation: ""
    published: false
    is_case_insensitive: false
    default_table_expiration {
      "0"
    }
    default_partition_expiration {
      "0"
    }
    creation_time {
      "1970-01-01T00:00:00Z"
    }
    last_modified_time {
      "1970-01-01T00:00:00Z"
    }
    max_time_travel {
      "0"
    }
    dataset_reference {
      project_id: "p-id"
      dataset_id: "d-id"
    }
    linked_dataset_source {
      source_dataset {
        project_id: ""
        dataset_id: ""
      }
    }
    default_rounding_mode {
      value: ""
    }
    storage_billing_model {
      storage_billing_model_value: ""
    }
  }
  http_response {
    status_code: 200
    http_headers {
      key: "header1"
      value: "value1"
    }
    payload: REDACTED
  }
})");
}

TEST(ListDatasetsResponseTest, DebugString) {
  BigQueryHttpResponse http_response;
  http_response.http_status_code = HttpStatusCode::kOk;
  http_response.http_headers.insert({{"header1", "value1"}});
  http_response.payload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "nextPageToken": "npt-123",
          "datasets": [
              {
                "id": "1",
                "kind": "kind-2",
                "datasetReference": {"projectId": "p123", "datasetId": "d123"},

                "friendlyName": "friendly-name",
                "location": "loc",
                "type": "DEFAULT"
              }
  ]})";
  auto response = ListDatasetsResponse::BuildFromHttpResponse(http_response);
  ASSERT_STATUS_OK(response);

  EXPECT_EQ(response->DebugString("ListDatasetsResponse", TracingOptions{}),
            R"(ListDatasetsResponse {)"
            R"( kind: "kind-1")"
            R"( etag: "tag-1")"
            R"( next_page_token: "npt-123")"
            R"( datasets {)"
            R"( kind: "kind-2")"
            R"( id: "1")"
            R"( friendly_name: "friendly-name")"
            R"( location: "loc")"
            R"( type: "DEFAULT")"
            R"( dataset_reference {)"
            R"( project_id: "p123")"
            R"( dataset_id: "d123")"
            R"( })"
            R"( })"
            R"( http_response {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED)"
            R"( })"
            R"( })");

  EXPECT_EQ(response->DebugString("ListDatasetsResponse",
                                  TracingOptions{}.SetOptions(
                                      "truncate_string_field_longer_than=7")),
            R"(ListDatasetsResponse {)"
            R"( kind: "kind-1")"
            R"( etag: "tag-1")"
            R"( next_page_token: "npt-123")"
            R"( datasets {)"
            R"( kind: "kind-2")"
            R"( id: "1")"
            R"( friendly_name: "friendl...<truncated>...")"
            R"( location: "loc")"
            R"( type: "DEFAULT")"
            R"( dataset_reference {)"
            R"( project_id: "p123")"
            R"( dataset_id: "d123")"
            R"( })"
            R"( })"
            R"( http_response {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED)"
            R"( })"
            R"( })");

  EXPECT_EQ(
      response->DebugString("ListDatasetsResponse",
                            TracingOptions{}.SetOptions("single_line_mode=F")),
      R"(ListDatasetsResponse {
  kind: "kind-1"
  etag: "tag-1"
  next_page_token: "npt-123"
  datasets {
    kind: "kind-2"
    id: "1"
    friendly_name: "friendly-name"
    location: "loc"
    type: "DEFAULT"
    dataset_reference {
      project_id: "p123"
      dataset_id: "d123"
    }
  }
  http_response {
    status_code: 200
    http_headers {
      key: "header1"
      value: "value1"
    }
    payload: REDACTED
  }
})");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
