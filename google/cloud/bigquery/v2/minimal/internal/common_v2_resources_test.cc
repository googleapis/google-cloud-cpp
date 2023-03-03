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

using ::testing::IsEmpty;
using ::testing::Not;

// Helper functions.

QueryParameterType GetQueryParameterType() {
  QueryParameterType expected_qp_type;
  QueryParameterStructType array_st;
  QueryParameterStructType qp_st;

  array_st.name = "array-struct-name";
  array_st.type = std::make_shared<QueryParameterType>();
  array_st.type->type = "array-struct-type";

  array_st.description = "array-struct-description";
  qp_st.name = "qp-struct-name";
  qp_st.type = std::make_shared<QueryParameterType>();
  qp_st.type->type = "qp-struct-type";
  qp_st.description = "qp-struct-description";

  expected_qp_type.type = "query-parameter-type";
  expected_qp_type.array_type = std::make_shared<QueryParameterType>();
  expected_qp_type.array_type->type = "array-type";
  expected_qp_type.array_type->struct_types.push_back(array_st);
  expected_qp_type.struct_types.push_back(qp_st);

  return expected_qp_type;
}

QueryParameterValue GetQueryParameterValue() {
  QueryParameterValue expected_qp_val;
  QueryParameterValue qp_struct_val;
  QueryParameterValue array_val;
  QueryParameterValue nested_array_val;
  QueryParameterValue array_struct_val;

  expected_qp_val.value = "query-parameter-value";
  qp_struct_val.value = "qp-map-value";

  nested_array_val.value = "array-val-2";
  array_struct_val.value = "array-map-value";
  nested_array_val.struct_values.insert({"array-map-key", array_struct_val});

  array_val.value = "array-val-1";
  array_val.array_values.push_back(nested_array_val);

  expected_qp_val.array_values.push_back(array_val);
  expected_qp_val.struct_values.insert({"qp-map-key", qp_struct_val});

  return expected_qp_val;
}

QueryParameter GetQueryParameter() {
  QueryParameter expected;
  expected.name = "query-parameter-name";
  expected.parameter_type = GetQueryParameterType();
  expected.parameter_value = GetQueryParameterValue();

  return expected;
}

void AssertParamValueEquals(QueryParameterValue& expected,
                            QueryParameterValue& actual) {
  EXPECT_EQ(expected.value, actual.value);

  ASSERT_THAT(expected.array_values, Not(IsEmpty()));
  ASSERT_THAT(actual.array_values, Not(IsEmpty()));
  EXPECT_EQ(expected.array_values.size(), actual.array_values.size());
  EXPECT_EQ(expected.array_values[0].value, actual.array_values[0].value);

  ASSERT_THAT(expected.array_values[0].array_values, Not(IsEmpty()));
  ASSERT_THAT(actual.array_values[0].array_values, Not(IsEmpty()));
  EXPECT_EQ(expected.array_values[0].array_values.size(),
            actual.array_values[0].array_values.size());
  EXPECT_EQ(expected.array_values[0].array_values[0].value,
            actual.array_values[0].array_values[0].value);

  EXPECT_EQ(expected.array_values[0].struct_values["array-map-key"].value,
            actual.array_values[0].struct_values["array-map-key"].value);
  EXPECT_EQ(expected.struct_values["qp-map-key"].value,
            actual.struct_values["qp-map-key"].value);
}

void AssertParamTypeEquals(QueryParameterType& expected,
                           QueryParameterType& actual) {
  EXPECT_EQ(expected.type, actual.type);

  EXPECT_EQ(expected.array_type->type, actual.array_type->type);

  ASSERT_THAT(expected.array_type->struct_types, Not(IsEmpty()));
  ASSERT_THAT(actual.array_type->struct_types, Not(IsEmpty()));
  EXPECT_EQ(expected.array_type->struct_types.size(),
            actual.array_type->struct_types.size());
  EXPECT_EQ(expected.array_type->struct_types[0].name,
            actual.array_type->struct_types[0].name);
  EXPECT_EQ(expected.array_type->struct_types[0].type->type,
            actual.array_type->struct_types[0].type->type);
  EXPECT_EQ(expected.array_type->struct_types[0].description,
            actual.array_type->struct_types[0].description);

  ASSERT_THAT(expected.struct_types, Not(IsEmpty()));
  ASSERT_THAT(actual.struct_types, Not(IsEmpty()));
  EXPECT_EQ(expected.struct_types.size(), actual.struct_types.size());
  EXPECT_EQ(expected.struct_types[0].name, actual.struct_types[0].name);
  EXPECT_EQ(expected.struct_types[0].type->type,
            actual.struct_types[0].type->type);
  EXPECT_EQ(expected.struct_types[0].description,
            actual.struct_types[0].description);
}

TEST(CommonV2ResourcesTest, QueryParameterTypeFromJson) {
  std::string text =
      R"({
          "type": "query-parameter-type",
          "array_type": {"type": "array-type", "struct_types": [{
                            "name": "array-struct-name",
                            "type": {"type": "array-struct-type"},
                            "description": "array-struct-description"
                          }]},
          "struct_types": [{
              "name": "qp-struct-name",
              "type": {"type": "qp-struct-type"},
              "description": "qp-struct-description"
              }]
      })";

  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  QueryParameterType actual;
  from_json(json, actual);

  auto expected = GetQueryParameterType();

  AssertParamTypeEquals(expected, actual);
}

TEST(CommonV2ResourcesTest, QueryParameterTypeToJson) {
  auto const expected_text =
      R"({
        "array_type":{
            "struct_types":[{
                "description":"array-struct-description",
                "name":"array-struct-name",
                "type":{
                    "struct_types":[],
                    "type":"array-struct-type"
                }
            }],
        "type":"array-type"},
        "struct_types":[{
            "description":"qp-struct-description",
            "name":"qp-struct-name",
            "type":{"struct_types":[],"type":"qp-struct-type"}
        }],
        "type":"query-parameter-type"})"_json;

  auto expected = GetQueryParameterType();

  nlohmann::json j;
  to_json(j, expected);

  EXPECT_EQ(j, expected_text);
}

TEST(CommonV2ResourcesTest, QueryParameterValueFromJson) {
  std::string text =
      R"({
          "value": "query-parameter-value",
          "array_values": [{"value": "array-val-1", "array_values": [{
                            "value": "array-val-2",
                            "struct_values": {"array-map-key": {"value":"array-map-value"}}
                          }]}],
          "struct_values": {"qp-map-key": {"value": "qp-map-value"}}
      })";
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  QueryParameterValue actual_qp;
  from_json(json, actual_qp);

  auto expected_qp = GetQueryParameterValue();

  AssertParamValueEquals(expected_qp, actual_qp);
}

TEST(CommonV2ResourcesTest, QueryParameterValueToJson) {
  auto const expected_text =
      R"({
        "array_values":[{
            "array_values":[{
                "array_values":[],
                "struct_values":{"array-map-key":{"array_values":[],"struct_values":{},"value":"array-map-value"}},
                "value":"array-val-2"
            }],
            "struct_values":{},
            "value":"array-val-1"
        }],
        "struct_values":{"qp-map-key":{"array_values":[],"struct_values":{},"value":"qp-map-value"}},
        "value":"query-parameter-value"})"_json;
  auto expected = GetQueryParameterValue();
  nlohmann::json j;
  to_json(j, expected);

  EXPECT_EQ(j, expected_text);
}

TEST(CommonV2ResourcesTest, QueryParameterFromJson) {
  std::string text =
      R"({
        "name": "query-parameter-name",
        "parameter_type": {
          "type": "query-parameter-type",
          "array_type": {"type": "array-type", "struct_types": [{
                            "name": "array-struct-name",
                            "type": {"type": "array-struct-type"},
                            "description": "array-struct-description"
                          }]},
          "struct_types": [{
              "name": "qp-struct-name",
              "type": {"type": "qp-struct-type"},
              "description": "qp-struct-description"
              }]
       },
        "parameter_value": {
          "value": "query-parameter-value",
          "array_values": [{"value": "array-val-1", "array_values": [{
                            "value": "array-val-2",
                            "struct_values": {"array-map-key": {"value":"array-map-value"}}
                          }]}],
          "struct_values": {"qp-map-key": {"value": "qp-map-value"}}
      }})";
  auto json = nlohmann::json::parse(text, nullptr, false);
  EXPECT_TRUE(json.is_object());

  QueryParameter expected = GetQueryParameter();

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
        "parameter_type":{
            "array_type":{
                "struct_types":[{
                    "description":"array-struct-description",
                    "name":"array-struct-name",
                    "type":{"struct_types":[],"type":"array-struct-type"}
                }],
                "type":"array-type"
            },
            "struct_types":[{
                "description":"qp-struct-description",
                "name":"qp-struct-name",
                "type":{"struct_types":[],"type":"qp-struct-type"}
            }],
            "type":"query-parameter-type"
        },
        "parameter_value":{
            "array_values":[{
                "array_values":[{
                    "array_values":[],
                    "struct_values":{"array-map-key":{"array_values":[],"struct_values":{},"value":"array-map-value"}},
                    "value":"array-val-2"
                }],
                "struct_values":{},
                "value":"array-val-1"
            }],
            "struct_values":{"qp-map-key":{"array_values":[],"struct_values":{},"value":"qp-map-value"}},
            "value":"query-parameter-value"
        }})"_json;
  auto expected = GetQueryParameter();
  nlohmann::json j;
  to_json(j, expected);

  EXPECT_EQ(j, expected_text);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
