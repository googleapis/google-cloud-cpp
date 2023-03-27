// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/discovery_type_vertex.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

TEST(DiscoveryTypeVertexTest, DetermineIntroducerEmpty) {
  auto constexpr kOptionalEmptyFieldJson = R"""({})""";
  auto json = nlohmann::json::parse(kOptionalEmptyFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::DetermineIntroducer(json), Eq(""));
}

TEST(DiscoveryTypeVertexTest, DetermineIntroducerRequiredRef) {
  auto constexpr kRequiredRefFieldJson = R"""(
{
  "$ref": "Foo",
  "required": true
}
)""";
  auto json = nlohmann::json::parse(kRequiredRefFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::DetermineIntroducer(json), Eq(""));
}

TEST(DiscoveryTypeVertexTest, DetermineIntroducerRepeated) {
  auto constexpr kArrayFieldJson = R"""(
{
  "type": "array"
}
)""";
  auto json = nlohmann::json::parse(kArrayFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::DetermineIntroducer(json), Eq("repeated "));
}

TEST(DiscoveryTypeVertexTest, DetermineIntroducerMap) {
  auto constexpr kMapFieldJson = R"""(
{
  "type": "object",
  "additionalProperties": {}
}
)""";
  auto json = nlohmann::json::parse(kMapFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::DetermineIntroducer(json), Eq(""));
}

TEST(DiscoveryTypeVertexTest, DetermineIntroducerOptionalString) {
  auto constexpr kOptionalStringFieldJson = R"""(
{
  "type": "string"
}
)""";
  auto json = nlohmann::json::parse(kOptionalStringFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::DetermineIntroducer(json), Eq("optional "));
}

TEST(DiscoveryTypeVertexTest, FormatFieldOptionsEmpty) {
  auto constexpr kOptionalEmptyFieldJson = R"""({})""";
  auto json = nlohmann::json::parse(kOptionalEmptyFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::FormatFieldOptions("test_field", json),
              Eq(""));
}

TEST(DiscoveryTypeVertexTest, FormatFieldOptionsRequired) {
  auto constexpr kOptionalRequiredFieldJson = R"""(
{
  "type": "string",
  "required": true
}
)""";
  auto json = nlohmann::json::parse(kOptionalRequiredFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::FormatFieldOptions("test_field", json),
              Eq(" [(google.api.field_behavior) = REQUIRED]"));
}

TEST(DiscoveryTypeVertexTest, FormatFieldOptionsOperationRequestField) {
  auto constexpr kOptionalOperationRequestFieldJson = R"""(
{
  "type": "string",
  "operation_request_field": true
}
)""";
  auto json =
      nlohmann::json::parse(kOptionalOperationRequestFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::FormatFieldOptions("test_field", json),
              Eq(" [(google.cloud.operation_request_field) = \"test_field\"]"));
}

TEST(DiscoveryTypeVertexTest, FormatFieldOptionsRequiredOperationRequestField) {
  auto constexpr kOptionalOperationRequestFieldJson = R"""(
{
  "type": "string",
  "required": true,
  "operation_request_field": true
}
)""";
  auto json =
      nlohmann::json::parse(kOptionalOperationRequestFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(
      DiscoveryTypeVertex::FormatFieldOptions("test_field", json),
      Eq(" [(google.api.field_behavior) = "
         "REQUIRED,(google.cloud.operation_request_field) = \"test_field\"]"));
}

struct DetermineTypesSuccess {
  std::string json;
  std::string expected_name;
  bool expected_properties_is_null;
  bool expected_is_map;
};

class DiscoveryTypeVertexSuccessTest
    : public ::testing::TestWithParam<DetermineTypesSuccess> {};

TEST_P(DiscoveryTypeVertexSuccessTest, DetermineTypesAndSynthesisSuccess) {
  auto json = nlohmann::json::parse(GetParam().json, nullptr, false);
  ASSERT_TRUE(json.is_object());

  auto result =
      DiscoveryTypeVertex::DetermineTypeAndSynthesis(json, "testField");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->name, Eq(GetParam().expected_name));
  EXPECT_THAT((result->properties == nullptr),
              Eq(GetParam().expected_properties_is_null));
  EXPECT_THAT(result->is_map, Eq(GetParam().expected_is_map));
}

INSTANTIATE_TEST_SUITE_P(
    DetermineTypesSuccess, DiscoveryTypeVertexSuccessTest,
    testing::Values(
        DetermineTypesSuccess{R"""({"$ref":"Foo"})""", "Foo", true, false},
        DetermineTypesSuccess{R"""({"type":"string"})""", "string", true,
                              false},
        DetermineTypesSuccess{R"""({"type":"boolean"})""", "bool", true, false},
        DetermineTypesSuccess{R"""({"type":"integer"})""", "int32", true,
                              false},
        DetermineTypesSuccess{R"""({"type":"integer","format":"uint8"})""",
                              "uint8", true, false},
        DetermineTypesSuccess{R"""({"type":"number"})""", "float", true, false},
        DetermineTypesSuccess{R"""({"type":"number","format":"double"})""",
                              "double", true, false},
        DetermineTypesSuccess{R"""({"type":"integer"})""", "int32", true,
                              false},
        DetermineTypesSuccess{R"""({"type":"array","items":{"$ref":"Foo"}})""",
                              "Foo", true, false},
        DetermineTypesSuccess{
            R"""({"type":"array","items":{"type":"string"}})""", "string", true,
            false},
        DetermineTypesSuccess{
            R"""({"type":"array","items":{"type":"object", "properties":{}}})""",
            "TestFieldItem", false, false},
        DetermineTypesSuccess{R"""({"type":"object","properties":{}})""",
                              "TestField", false, false},
        DetermineTypesSuccess{
            R"""({"type":"object","additionalProperties":{"$ref":"Foo"}})""",
            "Foo", true, true},
        DetermineTypesSuccess{
            R"""({"type":"object","additionalProperties":{"type":"string"}})""",
            "string", true, true},
        DetermineTypesSuccess{
            R"""({"type":"object","additionalProperties":{"type":"object", "properties":{}}})""",
            "TestFieldItem", false, true}));

struct DetermineTypesError {
  std::string json;
  std::string error_message;
};

class DiscoveryTypeVertexErrorTest
    : public ::testing::TestWithParam<DetermineTypesError> {};

TEST_P(DiscoveryTypeVertexErrorTest, DetermineTypesAndSynthesisError) {
  auto json = nlohmann::json::parse(GetParam().json, nullptr, false);
  ASSERT_TRUE(json.is_object());
  auto result =
      DiscoveryTypeVertex::DetermineTypeAndSynthesis(json, "testField");
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr(GetParam().error_message)));
}

INSTANTIATE_TEST_SUITE_P(
    DetermineTypesError, DiscoveryTypeVertexErrorTest,
    testing::Values(
        DetermineTypesError{R"""({})""",
                            "field: testField has neither $ref nor type."},
        DetermineTypesError{R"""({
  "type": "garbage"})""",
                            "field: testField has unknown type: garbage."},
        DetermineTypesError{R"""({
  "type": "object"})""",
                            "field: testField is type object with neither "
                            "properties nor additionalProperties."},
        DetermineTypesError{
            R"""({
  "type": "object",
  "additionalProperties": {
    "type": "garbage"
  }})""",
            "field: testField unknown type: garbage for map field."},
        DetermineTypesError{
            R"""({
  "type": "object",
  "additionalProperties": {}})""",
            "field: testField is a map with neither $ref nor type."},
        DetermineTypesError{
            R"""({
  "type": "array"})""",
            "field: testField array has no items."},
        DetermineTypesError{
            R"""({
  "type": "array",
  "items": {
    "type": "garbage"
  }
})""",
            "field: testField unknown type: garbage for array field."},
        DetermineTypesError{
            R"""({
  "type": "array",
  "items": {}})""",
            "field: testField is array with items having neither $ref nor "
            "type."}));

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
