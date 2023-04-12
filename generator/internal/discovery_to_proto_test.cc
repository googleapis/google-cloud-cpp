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

#include "generator/internal/discovery_to_proto.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::UnorderedElementsAre;

TEST(ExtractTypesFromSchemaTest, Success) {
  auto constexpr kDiscoveryDocWithCorrectSchema = R"""(
{
  "schemas": {
    "Foo": {
      "id": "Foo",
      "type": "object"
    },
    "Bar": {
      "id": "Bar",
      "type": "object"
    }
  }
}
)""";

  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithCorrectSchema, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema(parsed_json);
  ASSERT_THAT(types, IsOk());
  EXPECT_THAT(*types, UnorderedElementsAre(Key("Foo"), Key("Bar")));
}

TEST(ExtractTypesFromSchemaTest, MissingSchema) {
  auto constexpr kDiscoveryDocMissingSchema = R"""(
{
}
)""";

  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocMissingSchema, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("does not contain schemas element")));
}

TEST(ExtractTypesFromSchemaTest, SchemaIdMissing) {
  auto constexpr kDiscoveryDocSchemaIdMissing = R"""(
{
  "schemas": {
    "Foo": {
      "id": "Foo",
      "type": "object"
    },
    "NoId": {
      "type": "object"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocSchemaIdMissing, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("schema without id")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr("current schema has no id. last schema with id=Foo")));
}

TEST(ExtractTypesFromSchemaTest, SchemaIdEmpty) {
  auto constexpr kDiscoveryDocSchemaIdEmpty = R"""(
{
  "schemas": {
    "Empty": {
      "id": null,
      "type": "object"
    },
    "NoId": {
      "id": null,
      "type": "object"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocSchemaIdEmpty, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("schema without id")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr(
                  "current schema has no id. last schema with id=(none)")));
}

TEST(ExtractTypesFromSchemaTest, SchemaMissingType) {
  auto constexpr kDiscoveryDocWithMissingType = R"""(
{
  "schemas": {
    "MissingType": {
      "id": "MissingType"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithMissingType, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("contains non object schema")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr("MissingType not type:object; is instead untyped")));
}

TEST(ExtractTypesFromSchemaTest, SchemaNonObject) {
  auto constexpr kDiscoveryDocWithNonObjectSchema = R"""(
{
  "schemas": {
    "NonObject": {
      "id": "NonObject",
      "type": "array"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithNonObjectSchema, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema(parsed_json);
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("contains non object schema")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr("NonObject not type:object; is instead array")));
}

TEST(ExtractResourcesTest, EmptyResources) {
  auto resources = ExtractResources({}, {}, {});
  EXPECT_THAT(resources, IsEmpty());
}

TEST(ExtractResourcesTest, NonEmptyResources) {
  auto constexpr kResourceJson = R"""({
  "resources": {
    "resource1": {
    },
    "resource2": {
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto resources = ExtractResources(resource_json, "", "");
  EXPECT_THAT(resources,
              UnorderedElementsAre(Key("resource1"), Key("resource2")));
}

TEST(DetermineAndVerifyResponseTypeNameTest, ResponseWithRef) {
  auto constexpr kResponseTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "response": {
    "$ref": "Foo"
  }
})""";
  auto response_type_json =
      nlohmann::json::parse(kResponseTypeJson, nullptr, false);
  ASSERT_TRUE(response_type_json.is_object());
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  DiscoveryResource resource;
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", response_type_json});
  auto response =
      DetermineAndVerifyResponseTypeName(method_json, resource, types);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, Eq("Foo"));
}

TEST(DetermineAndVerifyResponseTypeNameTest, EmptyResponse) {
  auto constexpr kResponseTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({})""";
  auto response_type_json =
      nlohmann::json::parse(kResponseTypeJson, nullptr, false);
  ASSERT_TRUE(response_type_json.is_object());
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  DiscoveryResource resource;
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", response_type_json});
  auto response =
      DetermineAndVerifyResponseTypeName(method_json, resource, types);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsEmpty());
}

TEST(DetermineAndVerifyResponseTypeNameTest, ResponseMissingRef) {
  auto constexpr kResponseTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "response": {
  }
})""";
  auto response_type_json =
      nlohmann::json::parse(kResponseTypeJson, nullptr, false);
  ASSERT_TRUE(response_type_json.is_object());
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  DiscoveryResource resource;
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", response_type_json});
  auto response =
      DetermineAndVerifyResponseTypeName(method_json, resource, types);
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("Missing $ref field in response")));
}

TEST(DetermineAndVerifyResponseTypeNameTest, ResponseFieldMissing) {
  auto constexpr kResponseTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({})""";
  auto response_type_json =
      nlohmann::json::parse(kResponseTypeJson, nullptr, false);
  ASSERT_TRUE(response_type_json.is_object());
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  DiscoveryResource resource;
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", response_type_json});
  auto response =
      DetermineAndVerifyResponseTypeName(method_json, resource, types);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsEmpty());
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
