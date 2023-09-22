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

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/testing/common_v2_test_utils.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::AssertEquals;
using ::google::cloud::bigquery_v2_minimal_testing::AssertParamTypeEquals;
using ::google::cloud::bigquery_v2_minimal_testing::AssertParamValueEquals;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryParameter;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryParameterType;
using ::google::cloud::bigquery_v2_minimal_testing::MakeQueryParameterValue;
using ::google::cloud::bigquery_v2_minimal_testing::MakeSystemVariables;

TEST(CommonV2ResourcesTest, QueryParameterTypeFromJson) {
  std::string text =
      R"({
          "type": "query-parameter-type",
          "arrayType": {"type": "array-type", "structTypes": [{
                            "name": "array-struct-name",
                            "type": {"type": "array-struct-type"},
                            "description": "array-struct-description"
                          }]},
          "structTypes": [{
              "name": "qp-struct-name",
              "type": {"type": "qp-struct-type"},
              "description": "qp-struct-description"
              }]
      })";

  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  QueryParameterType actual;
  from_json(json, actual);

  auto expected = MakeQueryParameterType();

  AssertParamTypeEquals(expected, actual);
}

TEST(CommonV2ResourcesTest, QueryParameterTypeToJson) {
  auto const expected_text =
      R"({
        "arrayType":{
            "structTypes":[{
                "description":"array-struct-description",
                "name":"array-struct-name",
                "type":{
                    "structTypes":[],
                    "type":"array-struct-type"
                }
            }],
        "type":"array-type"},
        "structTypes":[{
            "description":"qp-struct-description",
            "name":"qp-struct-name",
            "type":{"structTypes":[],"type":"qp-struct-type"}
        }],
        "type":"query-parameter-type"})"_json;

  auto expected = MakeQueryParameterType();

  nlohmann::json j;
  to_json(j, expected);

  EXPECT_EQ(j, expected_text);
}

TEST(CommonV2ResourcesTest, QueryParameterValueFromJson) {
  std::string text =
      R"({
          "value": "query-parameter-value",
          "arrayValues": [{"value": "array-val-1", "arrayValues": [{
                            "value": "array-val-2",
                            "structValues": {"array-map-key": {"value":"array-map-value"}}
                          }]}],
          "structValues": {"qp-map-key": {"value": "qp-map-value"}}
      })";
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  QueryParameterValue actual_qp;
  from_json(json, actual_qp);

  auto expected_qp = MakeQueryParameterValue();

  AssertParamValueEquals(expected_qp, actual_qp);
}

TEST(CommonV2ResourcesTest, QueryParameterValueToJson) {
  auto const expected_text =
      R"({
        "arrayValues":[{
            "arrayValues":[{
                "arrayValues":[],
                "structValues":{"array-map-key":{"arrayValues":[],"structValues":{},"value":"array-map-value"}},
                "value":"array-val-2"
            }],
            "structValues":{},
            "value":"array-val-1"
        }],
        "structValues":{"qp-map-key":{"arrayValues":[],"structValues":{},"value":"qp-map-value"}},
        "value":"query-parameter-value"})"_json;
  auto expected = MakeQueryParameterValue();
  nlohmann::json j;
  to_json(j, expected);

  EXPECT_EQ(j, expected_text);
}

TEST(CommonV2ResourcesTest, QueryParameterFromJson) {
  std::string text =
      R"({
        "name": "query-parameter-name",
        "parameterType": {
          "type": "query-parameter-type",
          "arrayType": {"type": "array-type", "structTypes": [{
                            "name": "array-struct-name",
                            "type": {"type": "array-struct-type"},
                            "description": "array-struct-description"
                          }]},
          "structTypes": [{
              "name": "qp-struct-name",
              "type": {"type": "qp-struct-type"},
              "description": "qp-struct-description"
              }]
       },
        "parameterValue": {
          "value": "query-parameter-value",
          "arrayValues": [{"value": "array-val-1", "arrayValues": [{
                            "value": "array-val-2",
                            "structValues": {"array-map-key": {"value":"array-map-value"}}
                          }]}],
          "structValues": {"qp-map-key": {"value": "qp-map-value"}}
      }})";
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  QueryParameter expected = MakeQueryParameter();

  QueryParameter actual;
  from_json(json, actual);

  EXPECT_EQ(expected.name, actual.name);
  AssertParamTypeEquals(expected.parameter_type, actual.parameter_type);
  AssertParamValueEquals(expected.parameter_value, actual.parameter_value);
}

TEST(CommonV2ResourcesTest, QueryParameterToJson) {
  auto const expected_text =
      R"({
        "name":"query-parameter-name",
        "parameterType":{
            "arrayType":{
                "structTypes":[{
                    "description":"array-struct-description",
                    "name":"array-struct-name",
                    "type":{"structTypes":[],"type":"array-struct-type"}
                }],
                "type":"array-type"
            },
            "structTypes":[{
                "description":"qp-struct-description",
                "name":"qp-struct-name",
                "type":{"structTypes":[],"type":"qp-struct-type"}
            }],
            "type":"query-parameter-type"
        },
        "parameterValue":{
            "arrayValues":[{
                "arrayValues":[{
                    "arrayValues":[],
                    "structValues":{"array-map-key":{"arrayValues":[],"structValues":{},"value":"array-map-value"}},
                    "value":"array-val-2"
                }],
                "structValues":{},
                "value":"array-val-1"
            }],
            "structValues":{"qp-map-key":{"arrayValues":[],"structValues":{},"value":"qp-map-value"}},
            "value":"query-parameter-value"
        }})"_json;
  auto expected = MakeQueryParameter();
  nlohmann::json j;
  to_json(j, expected);

  EXPECT_EQ(j, expected_text);
}

TEST(CommonV2ResourcesTest, DatasetReferenceFromJson) {
  std::string text =
      R"({
          "datasetId":"d123",
          "projectId":"p123"
      })";
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  DatasetReference actual;
  from_json(json, actual);

  DatasetReference expected;
  expected.dataset_id = "d123";
  expected.project_id = "p123";

  EXPECT_EQ(expected.dataset_id, actual.dataset_id);
  EXPECT_EQ(expected.project_id, actual.project_id);
}

TEST(CommonV2ResourcesTest, DatasetReferenceToJson) {
  auto const expected_json =
      R"({
          "datasetId":"d123",
          "projectId":"p123"
      })"_json;

  DatasetReference expected;
  expected.dataset_id = "d123";
  expected.project_id = "p123";

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(CommonV2ResourcesTest, TableReferenceFromJson) {
  std::string text =
      R"({
          "datasetId":"d123",
          "projectId":"p123",
          "tableId":"t123"
      })";
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  TableReference actual;
  from_json(json, actual);

  TableReference expected;
  expected.dataset_id = "d123";
  expected.project_id = "p123";
  expected.table_id = "t123";

  EXPECT_EQ(expected.dataset_id, actual.dataset_id);
  EXPECT_EQ(expected.project_id, actual.project_id);
  EXPECT_EQ(expected.table_id, actual.table_id);
}

TEST(CommonV2ResourcesTest, TableReferenceToJson) {
  auto const expected_json =
      R"({
          "datasetId":"d123",
          "projectId":"p123",
          "tableId":"t123"
      })"_json;

  TableReference expected;
  expected.dataset_id = "d123";
  expected.project_id = "p123";
  expected.table_id = "t123";

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);
}

TEST(CommonV2ResourcesTest, DatasetReferenceDebugString) {
  DatasetReference dataset;
  dataset.dataset_id = "d123";
  dataset.project_id = "p123";

  EXPECT_EQ(dataset.DebugString("DatasetReference", TracingOptions{}),
            R"(DatasetReference {)"
            R"( project_id: "p123")"
            R"( dataset_id: "d123")"
            R"( })");

  EXPECT_EQ(dataset.DebugString("DatasetReference",
                                TracingOptions{}.SetOptions(
                                    "truncate_string_field_longer_than=2")),
            R"(DatasetReference {)"
            R"( project_id: "p1...<truncated>...")"
            R"( dataset_id: "d1...<truncated>...")"
            R"( })");

  EXPECT_EQ(dataset.DebugString("DatasetReference", TracingOptions{}.SetOptions(
                                                        "single_line_mode=F")),
            R"(DatasetReference {
  project_id: "p123"
  dataset_id: "d123"
})");
}

TEST(CommonV2ResourcesTest, TableReferenceDebugString) {
  TableReference table;
  table.dataset_id = "d123";
  table.project_id = "p123";
  table.table_id = "t123";

  EXPECT_EQ(table.DebugString("TableReference", TracingOptions{}),
            R"(TableReference {)"
            R"( project_id: "p123")"
            R"( dataset_id: "d123")"
            R"( table_id: "t123")"
            R"( })");

  EXPECT_EQ(table.DebugString("TableReference",
                              TracingOptions{}.SetOptions(
                                  "truncate_string_field_longer_than=2")),
            R"(TableReference {)"
            R"( project_id: "p1...<truncated>...")"
            R"( dataset_id: "d1...<truncated>...")"
            R"( table_id: "t1...<truncated>...")"
            R"( })");

  EXPECT_EQ(table.DebugString("TableReference", TracingOptions{}.SetOptions(
                                                    "single_line_mode=F")),
            R"(TableReference {
  project_id: "p123"
  dataset_id: "d123"
  table_id: "t123"
})");
}

TEST(CommonV2ResourcesTest, ErrorProtoDebugString) {
  ErrorProto error;
  error.reason = "e-reason";
  error.location = "e-loc";
  error.message = "e-mesg";

  EXPECT_EQ(error.DebugString("ErrorProto", TracingOptions{}),
            R"(ErrorProto {)"
            R"( reason: "e-reason")"
            R"( location: "e-loc")"
            R"( message: "e-mesg")"
            R"( })");

  EXPECT_EQ(error.DebugString("ErrorProto",
                              TracingOptions{}.SetOptions(
                                  "truncate_string_field_longer_than=2")),
            R"(ErrorProto {)"
            R"( reason: "e-...<truncated>...")"
            R"( location: "e-...<truncated>...")"
            R"( message: "e-...<truncated>...")"
            R"( })");

  EXPECT_EQ(error.DebugString("ErrorProto", TracingOptions{}.SetOptions(
                                                "single_line_mode=F")),
            R"(ErrorProto {
  reason: "e-reason"
  location: "e-loc"
  message: "e-mesg"
})");
}

TEST(CommonV2ResourcesTest, SystemVariablesToFromJson) {
  auto const* const expected_text =
      R"({"types":{"sql-struct-type-key-1":{)"
      R"("structType":{)"
      R"("fields":[{)"
      R"("name":"f1-sql-struct-type-int64")"
      R"(}]})"
      R"(,"sub_type_index":2)"
      R"(,"typeKind":"INT64")"
      R"(})"
      R"(,"sql-struct-type-key-2":{)"
      R"("structType":{)"
      R"("fields":[{)"
      R"("name":"f2-sql-struct-type-string")"
      R"(}]})"
      R"(,"sub_type_index":2)"
      R"(,"typeKind":"STRING"})"
      R"(,"sql-struct-type-key-3":{)"
      R"("arrayElementType":{)"
      R"("structType":{)"
      R"("fields":[{)"
      R"("name":"f2-sql-struct-type-string")"
      R"(}]})"
      R"(,"sub_type_index":2)"
      R"(,"typeKind":"STRING"})"
      R"(,"sub_type_index":1)"
      R"(,"typeKind":"STRING")"
      R"(}})"
      R"(,"values":{)"
      R"("fields":{)"
      R"("bool-key":{"valueKind":true,"kind_index":3})"
      R"(,"double-key":{"valueKind":3.4,"kind_index":1})"
      R"(,"string-key":{"valueKind":"val3","kind_index":2})"
      R"(}}})";
  auto expected_json = nlohmann::json::parse(expected_text, nullptr, false);
  EXPECT_TRUE(expected_json.is_object());

  auto expected = MakeSystemVariables();

  nlohmann::json actual_json;
  to_json(actual_json, expected);

  EXPECT_EQ(expected_json, actual_json);

  SystemVariables actual;
  from_json(actual_json, actual);

  AssertEquals(expected, actual);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
