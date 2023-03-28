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

TEST(DiscoveryTypeVertexTest, JsonToProtobufScalarTypes) {
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
  optional bool boolean_field = 1;

  // Description for integerFormatInt64Field.
  optional int64 integer_format_int64_field = 2;

  // Description for integerNoFormatField.
  optional int32 integer_no_format_field = 3;

  // Description for numberFormatDoubleField.
  optional double number_format_double_field = 4;

  // Description for numberNoFormatField.
  optional float number_no_format_field = 5;

  // Description for refField.
  optional Foo ref_field = 6;

  // Description for stringField.
  optional string string_field = 7;
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", json);

  auto result = t.JsonToProtobufMessage();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST(DiscoveryTypeVertexTest, JsonToProtobufMapType) {
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
  map<string, Foo> map_ref_field = 1;

  // Description for mapStringField.
  map<string, string> map_string_field = 2;

  message MapSynthesizedFieldItem {
    optional string field1 = 1;

    optional float field2 = 2;
  }

  // Description for mapSynthesizedField.
  map<string, MapSynthesizedFieldItem> map_synthesized_field = 3;
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", json);

  auto result = t.JsonToProtobufMessage();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST(DiscoveryTypeVertexTest, JsonToProtobufArrayTypes) {
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
  repeated bool boolean_array = 1;

  // Description of integerFormatInt64Array.
  repeated int64 integer_format_int64_array = 2;

  // Description of integerNoFormatArray.
  repeated int32 integer_no_format_array = 3;

  // Description of numberFormatDoubleArray.
  repeated double number_format_double_array = 4;

  // Description of numberNoFormatArray.
  repeated float number_no_format_array = 5;

  // Description of refArray.
  repeated Foo ref_array = 6;

  // Description of stringArray.
  repeated string string_array = 7;

  message SynthesizedArrayItem {
    // Description of field1.
    optional string field1 = 1;

    // Description of field2.
    repeated uint8 field2 = 2;
  }

  // Description of synthesizedArray.
  repeated SynthesizedArrayItem synthesized_array = 8;
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", json);

  auto result = t.JsonToProtobufMessage();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST(DiscoveryTypeVertex, JsonToProtobufMessageNoDescription) {
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
  optional string enum_field = 1;

  // Description for int32Field.
  optional int32 int32_field = 2;

  // Description for mapRefField.
  map<string, Foo> map_ref_field = 3;

  // Description for mapStringField.
  map<string, string> map_string_field = 4;

  message NestedTypeField {
    // Description for nestedField1.
    optional string nested_field1 = 1;

    // Description for nestedField2.
    optional Bar nested_field2 = 2;
  }

  // Description for nestedTypeField.
  optional NestedTypeField nested_type_field = 5;

  // Description for stringField.
  optional string string_field = 6;
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", json);

  auto result = t.JsonToProtobufMessage();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

TEST(DiscoveryTypeVertex, JsonToProtobufMessageWithDescription) {
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
  optional string string_field = 1;
}
)""";

  auto json = nlohmann::json::parse(kSchemaJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryTypeVertex t("TestSchema", json);

  auto result = t.JsonToProtobufMessage();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(*result, Eq(kExpectedProto));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
