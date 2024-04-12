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
#include "generator/testing/descriptor_pool_fixture.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;

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

TEST(DiscoveryTypeVertexTest, FormatMessageDescriptionWithDollarSigns) {
  auto constexpr kMessageWithDollarSignInDescription = R"""(
        {
          "description": "Example: `sample_table$20190123`",
          "type": "string"
        }
  )""";
  auto json = nlohmann::json::parse(kMessageWithDollarSignInDescription,
                                    nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(DiscoveryTypeVertex::FormatMessageDescription(json, 0),
              Eq("// Example: `sample_table$$20190123`\n"));
}

TEST(DiscoveryTypeVertexTest, FormatFieldOptionsEmpty) {
  auto constexpr kOptionalEmptyFieldJson = R"""({})""";
  auto json = nlohmann::json::parse(kOptionalEmptyFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(
      DiscoveryTypeVertex::FormatFieldOptions("test_field", "testField", json),
      Eq(" [json_name=\"testField\"]"));
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
  EXPECT_THAT(
      DiscoveryTypeVertex::FormatFieldOptions("test_field", "testField", json),
      Eq(" [(google.api.field_behavior) = REQUIRED,json_name=\"testField\"]"));
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
  EXPECT_THAT(
      DiscoveryTypeVertex::FormatFieldOptions("test_field", "testField", json),
      Eq(" [(google.cloud.operation_request_field) = "
         "\"test_field\",json_name=\"testField\"]"));
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
      DiscoveryTypeVertex::FormatFieldOptions("test_field", "testField", json),
      Eq(" [(google.api.field_behavior) = "
         "REQUIRED,(google.cloud.operation_request_field) = "
         "\"test_field\",json_name=\"testField\"]"));
}

TEST(DiscoveryTypeVertexTest, FormatFieldOptionsRequiredIsResource) {
  auto constexpr kRequiredIsResourceFieldJson = R"""(
{
  "type": "string",
  "required": true,
  "is_resource": true
}
)""";
  auto json =
      nlohmann::json::parse(kRequiredIsResourceFieldJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  EXPECT_THAT(
      DiscoveryTypeVertex::FormatFieldOptions("test_field", "testField", json),
      Eq(" [(google.api.field_behavior) = "
         "REQUIRED,json_name=\"__json_request_body\"]"));
}

struct DetermineTypesSuccess {
  std::string name;
  std::string json;
  std::string expected_name;
  bool expected_properties_is_null;
  bool expected_is_map;
  bool expected_compare_package_name;
  bool expected_is_message;
};

class DiscoveryTypeVertexDetermineTypesSuccessTest
    : public ::testing::TestWithParam<DetermineTypesSuccess> {};

TEST_P(DiscoveryTypeVertexDetermineTypesSuccessTest,
       DetermineTypesAndSynthesisSuccess) {
  auto json = nlohmann::json::parse(GetParam().json, nullptr, false);
  ASSERT_TRUE(json.is_object());

  auto result =
      DiscoveryTypeVertex::DetermineTypeAndSynthesis(json, "testField");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->name, Eq(GetParam().expected_name));
  EXPECT_THAT((result->properties == nullptr),
              Eq(GetParam().expected_properties_is_null));
  EXPECT_THAT(result->is_map, Eq(GetParam().expected_is_map));
  EXPECT_THAT(result->compare_package_name,
              Eq(GetParam().expected_compare_package_name));
  EXPECT_THAT(result->is_message, Eq(GetParam().expected_is_message));
}

INSTANTIATE_TEST_SUITE_P(
    DetermineTypesSuccess, DiscoveryTypeVertexDetermineTypesSuccessTest,
    testing::Values(
        DetermineTypesSuccess{"message", R"""({"$ref":"Foo"})""", "Foo", true,
                              false, true, true},
        DetermineTypesSuccess{"string", R"""({"type":"string"})""", "string",
                              true, false, false, false},
        DetermineTypesSuccess{"any", R"""({"type":"any"})""",
                              "google.protobuf.Any", true, false, false, false},
        DetermineTypesSuccess{"boolean", R"""({"type":"boolean"})""", "bool",
                              true, false, false, false},
        DetermineTypesSuccess{"integer_no_format", R"""({"type":"integer"})""",
                              "int32", true, false, false, false},
        DetermineTypesSuccess{"integer_uint8",
                              R"""({"type":"integer","format":"uint8"})""",
                              "uint8", true, false, false, false},
        DetermineTypesSuccess{"number_no_format", R"""({"type":"number"})""",
                              "float", true, false, false, false},
        DetermineTypesSuccess{"number_double",
                              R"""({"type":"number","format":"double"})""",
                              "double", true, false, false, false},
        DetermineTypesSuccess{"array_message",
                              R"""({"type":"array","items":{"$ref":"Foo"}})""",
                              "Foo", true, false, true, true},
        DetermineTypesSuccess{
            "array_string", R"""({"type":"array","items":{"type":"string"}})""",
            "string", true, false, false, false},
        DetermineTypesSuccess{
            "array_int64",
            R"""({"type":"array","items":{"type":"integer","format":"int64"}})""",
            "int64", true, false, false, false},
        DetermineTypesSuccess{
            "array_any",
            R"""({"type":"array","items":{"type":"object","additionalProperties":{"type":"any"}}})""",
            "google.protobuf.Any", true, false, false, false},
        DetermineTypesSuccess{
            "array_nested_message",
            R"""({"type":"array","items":{"type":"object", "properties":{}}})""",
            "TestFieldItem", false, false, false, true},
        DetermineTypesSuccess{"nested_message",
                              R"""({"type":"object","properties":{}})""",
                              "TestField", false, false, false, true},
        DetermineTypesSuccess{
            "map_message",
            R"""({"type":"object","additionalProperties":{"$ref":"Foo"}})""",
            "Foo", true, true, true, false},
        DetermineTypesSuccess{
            "map_string",
            R"""({"type":"object","additionalProperties":{"type":"string"}})""",
            "string", true, true, false, false},
        DetermineTypesSuccess{
            "map_any",
            R"""({"type":"object","additionalProperties":{"type":"any"}})""",
            "google.protobuf.Any", true, true, false, false},
        DetermineTypesSuccess{
            "map_nested_message",
            R"""({"type":"object","additionalProperties":{"type":"object", "properties":{}}})""",
            "TestFieldItem", false, true, false, true}),
    [](testing::TestParamInfo<
        DiscoveryTypeVertexDetermineTypesSuccessTest::ParamType> const& info) {
      return info.param.name;
    });

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

class DiscoveryTypeVertexDescriptorTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(DiscoveryTypeVertexDescriptorTest, JsonToProtobufScalarTypes) {
  auto constexpr kSchemaJson = R"""(
{
  "id": "TestSchema",
  "properties": {
    "refField": {
      "$ref": "Foo",
      "description": "Description for refField."
    },
    "booleanField": {
      "type": "boolean",
      "description": "Description for booleanField."
    },
    "integerNoFormatField": {
      "type": "integer",
      "description": "Description for integerNoFormatField."
    },
    "integerFormatInt64Field": {
      "type": "integer",
      "format": "int64",
      "description": "Description for integerFormatInt64Field."
    },
    "numberNoFormatField": {
      "type": "number",
      "description": "Description for numberNoFormatField."
    },
    "numberFormatDoubleField": {
      "type": "number",
      "format": "double",
      "description": "Description for numberFormatDoubleField."
    },
    "stringField": {
      "type": "string",
      "description": "Description for stringField."
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(message TestSchema {
  // Description for booleanField.
  optional bool boolean_field = 1 [json_name="booleanField"];

  // Description for integerFormatInt64Field.
  optional int64 integer_format_int64_field = 2 [json_name="integerFormatInt64Field"];

  // Description for integerNoFormatField.
  optional int32 integer_no_format_field = 3 [json_name="integerNoFormatField"];

  // Description for numberFormatDoubleField.
  optional double number_format_double_field = 4 [json_name="numberFormatDoubleField"];

  // Description for numberNoFormatField.
  optional float number_no_format_field = 5 [json_name="numberNoFormatField"];

  // Description for refField.
  optional Foo ref_field = 6 [json_name="refField"];

  // Description for stringField.
  optional string string_field = 7 [json_name="stringField"];
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest, JsonToProtobufMapType) {
  auto constexpr kSchemaJson = R"""(
{
  "id": "TestSchema",
  "properties": {
    "mapRefField": {
      "type": "object",
      "description": "Description for mapRefField.",
      "additionalProperties": {
        "$ref": "Foo"
      }
    },
    "mapStringField": {
      "type": "object",
      "description": "Description for mapStringField.",
      "additionalProperties": {
        "type": "string"
      }
    },
    "mapSynthesizedField": {
      "type": "object",
      "description": "Description for mapSynthesizedField.",
      "additionalProperties": {
        "type": "object",
        "properties": {
          "field1": {
            "type": "string"
          },
          "field2": {
            "type": "number"
          }
        }
      }
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(message TestSchema {
  // Description for mapRefField.
  map<string, Foo> map_ref_field = 1 [json_name="mapRefField"];

  // Description for mapStringField.
  map<string, string> map_string_field = 2 [json_name="mapStringField"];

  message MapSynthesizedFieldItem {
    optional string field1 = 1 [json_name="field1"];

    optional float field2 = 2 [json_name="field2"];
  }

  // Description for mapSynthesizedField.
  map<string, MapSynthesizedFieldItem> map_synthesized_field = 3 [json_name="mapSynthesizedField"];
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest, JsonToProtobufBigQueryMapType) {
  auto constexpr kSchemaJson = R"""({
      "additionalProperties": {
        "$ref": "JsonValue"
      },
      "description": "Represents a single JSON object.",
      "id": "JsonObject",
      "type": "object"
})""";

  auto constexpr kExpectedProto = R"""(// Represents a single JSON object.
message JsonObject {
  // Represents a single JSON object.
  map<string, JsonValue> json_object = 1 [json_name="JsonObject"];
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("JsonObject", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("JsonValue",
                DiscoveryTypeVertex{"JsonValue", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest, JsonToProtobufArrayTypes) {
  auto constexpr kSchemaJson = R"""(
{
  "id": "TestSchema",
  "properties": {
    "booleanArray": {
      "type": "array",
      "description": "Description of booleanArray.",
      "items": {
        "type": "boolean"
      }
    },
    "integerFormatInt64Array": {
      "type": "array",
      "description": "Description of integerFormatInt64Array.",
      "items": {
        "type": "integer",
        "format": "int64"
      }
    },
    "integerNoFormatArray": {
      "type": "array",
      "description": "Description of integerNoFormatArray.",
      "items": {
        "type": "integer"
      }
    },
    "numberFormatDoubleArray": {
      "type": "array",
      "description": "Description of numberFormatDoubleArray.",
      "items": {
        "type": "number",
        "format": "double"
      }
    },
    "numberNoFormatArray": {
      "type": "array",
      "description": "Description of numberNoFormatArray.",
      "items": {
        "type": "number"
      }
    },
    "refArray": {
      "type": "array",
      "description": "Description of refArray.",
      "items": {
        "$ref": "Foo"
      }
    },
    "stringArray": {
      "type": "array",
      "description": "Description of stringArray.",
      "items": {
        "type": "string"
      }
    },
    "synthesizedArray": {
      "type": "array",
      "description": "Description of synthesizedArray.",
      "items": {
        "type": "object",
        "properties": {
          "field1": {
            "type": "string",
            "description": "Description of field1."
          },
          "field2": {
            "type": "array",
            "description": "Description of field2.",
            "items": {
              "type": "integer",
              "format": "uint8"
            }
          }
        }
      }
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(message TestSchema {
  // Description of booleanArray.
  repeated bool boolean_array = 1 [json_name="booleanArray"];

  // Description of integerFormatInt64Array.
  repeated int64 integer_format_int64_array = 2 [json_name="integerFormatInt64Array"];

  // Description of integerNoFormatArray.
  repeated int32 integer_no_format_array = 3 [json_name="integerNoFormatArray"];

  // Description of numberFormatDoubleArray.
  repeated double number_format_double_array = 4 [json_name="numberFormatDoubleArray"];

  // Description of numberNoFormatArray.
  repeated float number_no_format_array = 5 [json_name="numberNoFormatArray"];

  // Description of refArray.
  repeated Foo ref_array = 6 [json_name="refArray"];

  // Description of stringArray.
  repeated string string_array = 7 [json_name="stringArray"];

  message SynthesizedArrayItem {
    // Description of field1.
    optional string field1 = 1 [json_name="field1"];

    // Description of field2.
    repeated uint8 field2 = 2 [json_name="field2"];
  }

  // Description of synthesizedArray.
  repeated SynthesizedArrayItem synthesized_array = 8 [json_name="synthesizedArray"];
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest, JsonToProtobufMessageNoDescription) {
  auto constexpr kSchemaJson = R"""(
{
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "stringField": {
      "type": "string",
      "description": "Description for stringField."
    },
    "enumField": {
      "enum": [
        "ENUM_VALUE1",
        "ENUM_VALUE2",
        "ENUM_VALUE3",
        "ENUM_VALUE4"
      ],
      "enumDescriptions": [
        "Description for ENUM_VALUE1.",
        "Description for ENUM_VALUE2.",
        "Description for ENUM_VALUE3.",
        "Description for ENUM_VALUE4."
      ],
      "type": "string"
    },
    "int32Field": {
      "type": "integer",
      "description": "Description for int32Field.",
      "format": "int32"
    },
    "mapStringField": {
      "additionalProperties": {
        "type": "string"
      },
      "type": "object",
      "description": "Description for mapStringField."
    },
    "mapRefField": {
      "additionalProperties": {
        "$ref": "Foo"
      },
      "type": "object",
      "description": "Description for mapRefField."
    },
    "nestedTypeField": {
      "description": "Description for nestedTypeField.",
      "type": "object",
      "properties": {
        "nestedField1": {
          "type": "string",
          "description": "Description for nestedField1."
        },
        "nestedField2": {
          "$ref": "Bar",
          "description": "Description for nestedField2."
        }
      }
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(message TestSchema {
  // ENUM_VALUE1: Description for ENUM_VALUE1.
  // ENUM_VALUE2: Description for ENUM_VALUE2.
  // ENUM_VALUE3: Description for ENUM_VALUE3.
  // ENUM_VALUE4: Description for ENUM_VALUE4.
  optional string enum_field = 1 [json_name="enumField"];

  // Description for int32Field.
  optional int32 int32_field = 2 [json_name="int32Field"];

  // Description for mapRefField.
  map<string, Foo> map_ref_field = 3 [json_name="mapRefField"];

  // Description for mapStringField.
  map<string, string> map_string_field = 4 [json_name="mapStringField"];

  message NestedTypeField {
    // Description for nestedField1.
    optional string nested_field1 = 1 [json_name="nestedField1"];

    // Description for nestedField2.
    optional other.package.Bar nested_field2 = 2 [json_name="nestedField2"];
  }

  // Description for nestedTypeField.
  optional NestedTypeField nested_type_field = 5 [json_name="nestedTypeField"];

  // Description for stringField.
  optional string string_field = 6 [json_name="stringField"];
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  types.emplace("Bar",
                DiscoveryTypeVertex{"Bar", "other.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithDescription) {
  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "stringField": {
      "type": "string",
      "description": "Description for stringField."
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(// Description of the message.
message TestSchema {
  // Description for stringField.
  optional string string_field = 1 [json_name="stringField"];
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest, DetermineReservedAndMaxFieldNumbers) {
  auto constexpr kProtoFile = R"""(
syntax = "proto3";
package generator.test;

message FieldsOnly {
  string field1 = 1 [json_name="field1"];
  string field2 = 2 [json_name="field2"];
}

message ReservedOnly {
  reserved 25, 4 to 6;
}

message FieldHighest {
  reserved 25, 4 to 6;
  string field26 = 26 [json_name="field26"];
}

message ReservedHighest {
  reserved 25, 4 to 6;
  string field7 = 7 [json_name="field7"];
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kProtoFile));
  auto const* file_descriptor = pool().FindFileByName("test.proto");
  ASSERT_THAT(file_descriptor, NotNull());

  auto const* message_descriptor =
      file_descriptor->FindMessageTypeByName("FieldsOnly");
  ASSERT_THAT(message_descriptor, NotNull());
  auto message_properties =
      DiscoveryTypeVertex::DetermineReservedAndMaxFieldNumbers(
          *message_descriptor);
  EXPECT_THAT(message_properties.next_available_field_number, Eq(3));
  EXPECT_THAT(message_properties.reserved_numbers, IsEmpty());

  message_descriptor = file_descriptor->FindMessageTypeByName("ReservedOnly");
  ASSERT_THAT(message_descriptor, NotNull());
  message_properties = DiscoveryTypeVertex::DetermineReservedAndMaxFieldNumbers(
      *message_descriptor);
  EXPECT_THAT(message_properties.next_available_field_number, Eq(26));
  EXPECT_THAT(message_properties.reserved_numbers,
              testing::ElementsAre(4, 5, 6, 25));

  message_descriptor = file_descriptor->FindMessageTypeByName("FieldHighest");
  ASSERT_THAT(message_descriptor, NotNull());
  message_properties = DiscoveryTypeVertex::DetermineReservedAndMaxFieldNumbers(
      *message_descriptor);
  EXPECT_THAT(message_properties.next_available_field_number, Eq(27));
  EXPECT_THAT(message_properties.reserved_numbers,
              testing::ElementsAre(4, 5, 6, 25));

  message_descriptor =
      file_descriptor->FindMessageTypeByName("ReservedHighest");
  ASSERT_THAT(message_descriptor, NotNull());
  message_properties = DiscoveryTypeVertex::DetermineReservedAndMaxFieldNumbers(
      *message_descriptor);
  EXPECT_THAT(message_properties.next_available_field_number, Eq(26));
  EXPECT_THAT(message_properties.reserved_numbers,
              testing::ElementsAre(4, 5, 6, 25));
}

TEST_F(DiscoveryTypeVertexDescriptorTest, GetFieldNumber) {
  auto constexpr kImportedProtoFile = R"""(
syntax = "proto3";
package generator.imported;

message Bar {}
)""";

  auto constexpr kProtoFile = R"""(
syntax = "proto3";
package generator.test;

import "imported.proto";

message Foo {}

message FieldsOnly {
  optional string field1 = 1;
  Foo field2 = 2;
  map<string, Foo> field3 = 3;
  int32 field4 = 4;
  repeated generator.imported.Bar field5 = 5;
  map<string, generator.imported.Bar> field6 = 6;
}

)""";

  ASSERT_TRUE(AddProtoFile("imported.proto", kImportedProtoFile));
  auto const* imported_file_descriptor =
      pool().FindFileByName("imported.proto");
  ASSERT_THAT(imported_file_descriptor, NotNull());

  ASSERT_TRUE(AddProtoFile("test.proto", kProtoFile));
  auto const* file_descriptor = pool().FindFileByName("test.proto");
  ASSERT_THAT(file_descriptor, NotNull());
  auto const* message_descriptor =
      file_descriptor->FindMessageTypeByName("NoPreviousMessage");
  ASSERT_THAT(message_descriptor, IsNull());

  auto field_number = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "new_field", "string", 1);
  ASSERT_STATUS_OK(field_number);
  EXPECT_THAT(*field_number, Eq(1));

  int const candidate_field_number = 7;
  message_descriptor = file_descriptor->FindMessageTypeByName("FieldsOnly");

  ASSERT_THAT(message_descriptor, NotNull());
  auto new_field_number = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "new_field", "string", candidate_field_number);
  ASSERT_STATUS_OK(new_field_number);
  EXPECT_THAT(*new_field_number, Eq(candidate_field_number));

  auto existing_optional_field_number = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "field1", "optional string", candidate_field_number);
  ASSERT_STATUS_OK(existing_optional_field_number);
  EXPECT_THAT(*existing_optional_field_number, Eq(1));

  auto existing_message_field_number = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "field2", "generator.test.Foo",
      candidate_field_number);
  ASSERT_STATUS_OK(existing_message_field_number);
  EXPECT_THAT(*existing_message_field_number, Eq(2));

  auto existing_map_field_number = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "field3", "map<string, generator.test.Foo>",
      candidate_field_number);
  ASSERT_STATUS_OK(existing_map_field_number);
  EXPECT_THAT(*existing_map_field_number, Eq(3));

  auto existing_pod_field_number = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "field4", "int32", candidate_field_number);
  ASSERT_STATUS_OK(existing_pod_field_number);
  EXPECT_THAT(*existing_pod_field_number, Eq(4));

  auto existing_repeated_different_package_field_number =
      DiscoveryTypeVertex::GetFieldNumber(message_descriptor, "field5",
                                          "repeated generator.imported.Bar",
                                          candidate_field_number);
  ASSERT_STATUS_OK(existing_repeated_different_package_field_number);
  EXPECT_THAT(*existing_repeated_different_package_field_number, Eq(5));

  auto existing_map_different_package_field_number =
      DiscoveryTypeVertex::GetFieldNumber(message_descriptor, "field6",
                                          "map<string, generator.imported.Bar>",
                                          candidate_field_number);
  ASSERT_STATUS_OK(existing_map_different_package_field_number);
  EXPECT_THAT(*existing_map_different_package_field_number, Eq(6));

  auto field_type_changed = DiscoveryTypeVertex::GetFieldNumber(
      message_descriptor, "field6", "map<string, generator.test.Bar>",
      candidate_field_number);
  EXPECT_THAT(
      field_type_changed,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "Message: generator.test.FieldsOnly has field: field6 "
              "whose type has changed from: map<string, "
              "generator.imported.Bar> to: map<string, generator.test.Bar>")));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithReservedFieldNumberLessThanMax) {
  auto constexpr kPreviousProto = R"""(
syntax = "proto3";
package test.package;
// Description of the message.
message TestSchema {
  reserved 1;
  // Description for existingField.
  optional string existing_field = 2;
}
)""";

  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "newField": {
      "$ref": "Foo",
      "description": "Description for newField."
    },
    "existingField": {
      "type": "string",
      "description": "Description for existingField."
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(// Description of the message.
message TestSchema {
  reserved 1;
  // Description for existingField.
  optional string existing_field = 2 [json_name="existingField"];

  // Description for newField.
  optional Foo new_field = 3 [json_name="newField"];
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kPreviousProto));
  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithReservedFieldNumberGreaterThanMax) {
  auto constexpr kPreviousProto = R"""(
syntax = "proto3";
package test.package;
// Description of the message.
message TestSchema {
  reserved 3;
  // Description for existingField.
  optional string existing_field = 1 [json_name="existingField"];

  // Description for otherExistingField.
  map<string, string> other_existing_field = 2 [json_name="otherExistingField"];
}
)""";

  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "newMapField": {
      "additionalProperties": {
        "type": "string"
      },
      "type": "object",
      "description": "Description for newMapField."
    },
    "existingField": {
      "type": "string",
      "description": "Description for existingField."
    },
    "otherExistingField": {
      "additionalProperties": {
        "type": "string"
      },
      "type": "object",
      "description": "Description for otherExistingField."
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(// Description of the message.
message TestSchema {
  reserved 3;
  // Description for existingField.
  optional string existing_field = 1 [json_name="existingField"];

  // Description for newMapField.
  map<string, string> new_map_field = 4 [json_name="newMapField"];

  // Description for otherExistingField.
  map<string, string> other_existing_field = 2 [json_name="otherExistingField"];
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kPreviousProto));
  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithDeletedField) {
  auto constexpr kPreviousProto = R"""(
syntax = "proto3";
package test.package;
// Description of the message.
message TestSchema {
  reserved 3;
  // Description for existingField.
  optional string existing_field = 1;

  // Description for deletedField.
  optional string deleted_field = 2;
}
)""";

  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "existingField": {
      "type": "string",
      "description": "Description for existingField."
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(// Description of the message.
message TestSchema {
  reserved 2, 3;
  // Description for existingField.
  optional string existing_field = 1 [json_name="existingField"];
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kPreviousProto));
  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithMultipleDeletedFields) {
  auto constexpr kPreviousProto = R"""(
syntax = "proto3";
package test.package;
// Description of the message.
message TestSchema {
  reserved 3;
  // Description for existingField.
  optional string existing_field = 1;

  // Description for deletedField.
  optional string deleted_field = 2;

  // Description for alsoDeletedField.
  optional string also_deleted_field = 4;
}
)""";

  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "existingField": {
      "type": "string",
      "description": "Description for existingField."
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(// Description of the message.
message TestSchema {
  reserved 2, 3, 4;
  // Description for existingField.
  optional string existing_field = 1 [json_name="existingField"];
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kPreviousProto));
  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithDeletedFieldAndNewField) {
  auto constexpr kPreviousProto = R"""(
syntax = "proto3";
package test.package;
// Description of the message.
message TestSchema {
  reserved 3;
  // Description for existingField.
  optional string existing_field = 1;

  // Description for deletedField.
  repeated string deleted_field = 2;
}
)""";

  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "existingField": {
      "type": "string",
      "description": "Description for existingField."
    },
    "newField": {
      "type": "array",
      "description": "Description for newField.",
      "items": {
        "type": "string"
      }
    }
  }
}
)""";

  auto constexpr kExpectedProto = R"""(// Description of the message.
message TestSchema {
  reserved 2, 3;
  // Description for existingField.
  optional string existing_field = 1 [json_name="existingField"];

  // Description for newField.
  repeated string new_field = 4 [json_name="newField"];
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kPreviousProto));
  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST_F(DiscoveryTypeVertexDescriptorTest,
       JsonToProtobufMessageWithBreakingTypeChange) {
  auto constexpr kPreviousProto = R"""(
syntax = "proto3";
package test.package;
// Description of the message.
message TestSchema {
  reserved 1;
  // Description for existingField.
  optional string existing_field = 2;
}
)""";

  auto constexpr kSchemaJson = R"""(
{
  "description": "Description of the message.",
  "type": "object",
  "id": "TestSchema",
  "properties": {
    "newField": {
      "$ref": "Foo",
      "description": "Description for newField."
    },
    "existingField": {
      "type": "number",
      "format": "double",
      "description": "Description for existingField."
    }
  }
}
)""";

  ASSERT_TRUE(AddProtoFile("test.proto", kPreviousProto));
  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", "test.package", json, &pool());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "test.package", {}, &pool()});
  auto result = t.JsonToProtobufMessage(types, "test.package");
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Message: test.package.TestSchema has field: "
                                 "existing_field whose type has changed from: "
                                 "optional string to: optional double")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
