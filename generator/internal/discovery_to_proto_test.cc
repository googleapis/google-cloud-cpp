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
#include "generator/testing/descriptor_pool_fixture.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Key;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class ExtractTypesFromSchemaTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(ExtractTypesFromSchemaTest, Success) {
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
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  ASSERT_THAT(types, IsOk());
  EXPECT_THAT(*types, UnorderedElementsAre(Key("Foo"), Key("Bar")));
}

TEST_F(ExtractTypesFromSchemaTest, MissingSchema) {
  auto constexpr kDiscoveryDocMissingSchema = R"""(
{
}
)""";

  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocMissingSchema, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("does not contain schemas element")));
}

TEST_F(ExtractTypesFromSchemaTest, SchemaIdMissing) {
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
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("schema without id")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr("current schema has no id. last schema with id=Foo")));
}

TEST_F(ExtractTypesFromSchemaTest, SchemaIdEmpty) {
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
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("schema without id")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr(
                  "current schema has no id. last schema with id=(none)")));
}

TEST_F(ExtractTypesFromSchemaTest, SchemaMissingType) {
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
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("contains non object schema")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr("MissingType not type:object; is instead untyped")));
}

TEST_F(ExtractTypesFromSchemaTest, SchemaNonObject) {
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
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  EXPECT_THAT(types, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("contains non object schema")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr("NonObject not type:object; is instead array")));
}

TEST(ExtractResourcesTest, EmptyResources) {
  auto resources = ExtractResources({}, {});
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
  auto resources = ExtractResources({}, resource_json);
  EXPECT_THAT(resources,
              UnorderedElementsAre(Key("resource1"), Key("resource2")));
}

class DetermineAndVerifyResponseTypeNameTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(DetermineAndVerifyResponseTypeNameTest, ResponseWithRef) {
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
  types.emplace("Foo",
                DiscoveryTypeVertex{"Foo", "", response_type_json, &pool()});
  auto response = DetermineAndVerifyResponseType(method_json, resource, types);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT((*response)->name(), Eq("Foo"));
}

TEST_F(DetermineAndVerifyResponseTypeNameTest, ResponseMissingRef) {
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
  types.emplace("Foo",
                DiscoveryTypeVertex{"Foo", "", response_type_json, &pool()});
  auto response = DetermineAndVerifyResponseType(method_json, resource, types);
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr("Missing $ref field in response")));
}

TEST_F(DetermineAndVerifyResponseTypeNameTest, ResponseFieldMissing) {
  auto constexpr kResponseTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({})""";
  auto response_type_json =
      nlohmann::json::parse(kResponseTypeJson, nullptr, false);
  ASSERT_TRUE(response_type_json.is_object());
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  DiscoveryResource resource;
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo",
                DiscoveryTypeVertex{"Foo", "", response_type_json, &pool()});
  auto response = DetermineAndVerifyResponseType(method_json, resource, types);
  EXPECT_THAT(response, IsOkAndHolds(IsNull()));
}

class SynthesizeRequestTypeTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(SynthesizeRequestTypeTest, OperationResponseWithRefRequestField) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kMethodJson = R"""({
  "scopes": [
    "https://www.googleapis.com/auth/cloud-platform"
  ],
  "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
  "httpMethod": "POST",
  "parameters": {
    "project": {
      "type": "string"
    },
    "zone": {
      "type": "string"
    },
    "fooId": {
      "type": "string"
    }
  },
  "response": {
    "$ref": "Operation"
  },
  "request": {
    "$ref": "Foo"
  },
  "parameterOrder": [
    "project",
    "zone",
    "fooId"
  ]
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto constexpr kExpectedRequestTypeJson = R"""({
"description":"Request message for Create.",
"id":"CreateRequest",
"method":"create",
"properties":{
  "fooId":{
    "type":"string"
  },
  "foo_resource":{
    "$ref":"Foo",
    "description":"The Foo for this request.",
    "is_resource":true
  },
  "project":{
    "operation_request_field":true,
    "type":"string"
  },
  "zone":{
    "operation_request_field":true,
    "type":"string"
  }
},
"request_resource_field_name":"foo_resource",
"resource":"foos",
"synthesized_request":true,
"type":"object"
})""";
  auto const expected_request_type_json =
      nlohmann::json::parse(kExpectedRequestTypeJson, nullptr, false);
  ASSERT_TRUE(expected_request_type_json.is_object());
  DiscoveryResource resource("foos", "", resource_json);
  auto result = SynthesizeRequestType(method_json, resource, "Operation",
                                      "create", &pool());
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->json(), Eq(expected_request_type_json));
}

TEST_F(SynthesizeRequestTypeTest,
       OperationResponseWithRefRequestFieldEndingInResource) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kMethodJson = R"""({
  "scopes": [
    "https://www.googleapis.com/auth/cloud-platform"
  ],
  "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
  "httpMethod": "POST",
  "parameters": {
    "project": {
      "type": "string"
    },
    "zone": {
      "type": "string"
    },
    "fooId": {
      "type": "string"
    }
  },
  "response": {
    "$ref": "Operation"
  },
  "request": {
    "$ref": "FooResource"
  },
  "parameterOrder": [
    "project",
    "zone",
    "fooId"
  ]
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto constexpr kExpectedRequestTypeJson = R"""({
"description":"Request message for Create.",
"id":"CreateRequest",
"method":"create",
"properties":{
  "fooId":{
    "type":"string"
  },
  "foo_resource":{
    "$ref":"FooResource",
    "description":"The FooResource for this request.",
    "is_resource": true
  },
  "project":{
    "operation_request_field":true,
    "type":"string"
  },
  "zone":{
    "operation_request_field":true,
    "type":"string"
  }
},
"request_resource_field_name":"foo_resource",
"resource":"foos",
"synthesized_request":true,
"type":"object"
})""";
  auto const expected_request_type_json =
      nlohmann::json::parse(kExpectedRequestTypeJson, nullptr, false);
  ASSERT_TRUE(expected_request_type_json.is_object());
  DiscoveryResource resource("foos", "", resource_json);
  auto result = SynthesizeRequestType(method_json, resource, "Operation",
                                      "create", &pool());
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->json(), Eq(expected_request_type_json));
}

TEST_F(SynthesizeRequestTypeTest, NonOperationWithoutRequestField) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kMethodJson = R"""({
  "scopes": [
    "https://www.googleapis.com/auth/cloud-platform"
  ],
  "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
  "httpMethod": "GET",
  "parameters": {
    "project": {
      "type": "string"
    },
    "zone": {
      "type": "string"
    },
    "fooId": {
      "type": "string"
    }
  },
  "response": {
    "$ref": "Foo"
  },
  "parameterOrder": [
    "project",
    "zone",
    "fooId"
  ]
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto constexpr kExpectedRequestTypeJson = R"""({
"description":"Request message for GetFoos.",
"id":"GetFoosRequest",
"method":"get",
"properties":{
  "fooId":{
    "type":"string"
  },
  "project":{
    "type":"string"
  },
  "zone":{
    "type":"string"
  }
},
"resource":"foos",
"synthesized_request":true,
"type":"object"
})""";
  auto const expected_request_type_json =
      nlohmann::json::parse(kExpectedRequestTypeJson, nullptr, false);
  ASSERT_TRUE(expected_request_type_json.is_object());
  DiscoveryResource resource("foos", "", resource_json);
  auto result =
      SynthesizeRequestType(method_json, resource, "Foo", "get", &pool());
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->json(), Eq(expected_request_type_json));
}

TEST_F(SynthesizeRequestTypeTest, MethodJsonMissingParameters) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kMethodJson = R"""({
  "scopes": [
    "https://www.googleapis.com/auth/cloud-platform"
  ],
  "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
  "httpMethod": "POST"
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  DiscoveryResource resource("foos", "", resource_json);
  auto result = SynthesizeRequestType(method_json, resource, "Operation",
                                      "create", &pool());
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInternal,
               HasSubstr("method_json does not contain parameters field")));
  EXPECT_THAT(result.status().error_info().metadata(),
              AllOf(Contains(Key("resource")), Contains(Key("method")),
                    Contains(Key("json"))));
}

TEST_F(SynthesizeRequestTypeTest, OperationResponseMissingRefInRequest) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kMethodJson = R"""({
  "scopes": [
    "https://www.googleapis.com/auth/cloud-platform"
  ],
  "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
  "httpMethod": "POST",
  "parameters": {
    "project": {
      "type": "string"
    },
    "zone": {
      "type": "string"
    },
    "fooId": {
      "type": "string"
    }
  },
  "response": {
    "$ref": "Operation"
  },
  "request": {
  },
  "parameterOrder": [
    "project",
    "zone",
    "fooId"
  ]
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto constexpr kExpectedRequestTypeJson = R"""({
"description":"Request message for Create.",
"id":"CreateRequest",
"method":"create",
"properties":{
  "fooId":{
    "type":"string"
  },
  "foo_resource":{
    "$ref":"Foo",
    "description":"The Foo for this request."
  },
  "project":{
    "operation_request_field":true,
    "type":"string"
  },
  "zone":{
    "operation_request_field":true,
    "type":"string"
  }
},
"request_resource_field_name":"foo_resource",
"resource":"foos",
"synthesized_request":true,
"type":"object"
})""";
  auto const expected_request_type_json =
      nlohmann::json::parse(kExpectedRequestTypeJson, nullptr, false);
  ASSERT_TRUE(expected_request_type_json.is_object());
  DiscoveryResource resource("foos", "", resource_json);
  auto result = SynthesizeRequestType(method_json, resource, "Operation",
                                      "create", &pool());
  EXPECT_THAT(
      result,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("resource foos has method Create with non $ref request")));
}

class ProcessMethodRequestsAndResponsesTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(ProcessMethodRequestsAndResponsesTest, RequestWithOperationResponse) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "create": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        },
        "zone": {
          "type": "string"
        },
        "fooId": {
          "type": "string"
        }
      },
      "response": {
        "$ref": "Operation"
      },
      "request": {
        "$ref": "Foo"
      },
      "parameterOrder": [
        "project",
        "zone",
        "fooId"
      ]
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kOperationTypeJson = R"""({
})""";
  auto const operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Operation", DiscoveryTypeVertex("Operation", "",
                                                 operation_type_json, &pool()));
  auto result = ProcessMethodRequestsAndResponses(resources, types, &pool());
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(
      types, UnorderedElementsAre(Key("Foos.CreateRequest"), Key("Operation")));
  EXPECT_THAT(resources.begin()->second.response_types(),
              UnorderedElementsAre(Key("Operation")));
  EXPECT_TRUE(resources.begin()->second.RequiresLROImport());
}

TEST_F(ProcessMethodRequestsAndResponsesTest, MethodWithEmptyRequest) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "noop": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/myResources",
      "httpMethod": "POST"
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  auto result = ProcessMethodRequestsAndResponses(resources, types, &pool());
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(types, IsEmpty());
  EXPECT_TRUE(resources.begin()->second.RequiresEmptyImport());
}

TEST_F(ProcessMethodRequestsAndResponsesTest, MethodWithEmptyResponse) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "cancel": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/myResources",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        }
      }
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  auto result = ProcessMethodRequestsAndResponses(resources, types, &pool());
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(types, Not(IsEmpty()));
  EXPECT_TRUE(resources.begin()->second.RequiresEmptyImport());
}

TEST_F(ProcessMethodRequestsAndResponsesTest, ResponseError) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "create": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        },
        "zone": {
          "type": "string"
        },
        "fooId": {
          "type": "string"
        }
      },
      "response": {
      },
      "request": {
        "$ref": "Foo"
      },
      "parameterOrder": [
        "project",
        "zone",
        "fooId"
      ]
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kOperationTypeJson = R"""({
})""";
  auto const operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Operation",
                DiscoveryTypeVertex("", "", operation_type_json, &pool()));
  auto result = ProcessMethodRequestsAndResponses(resources, types, &pool());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("Missing $ref field in response")));
  EXPECT_THAT(result.error_info().metadata(),
              AllOf(Contains(Key("resource")), Contains(Key("method"))));
}

TEST_F(ProcessMethodRequestsAndResponsesTest, RequestError) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "create": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        },
        "zone": {
          "type": "string"
        },
        "fooId": {
          "type": "string"
        }
      },
      "response": {
        "$ref": "Operation"
      },
      "request": {
      },
      "parameterOrder": [
        "project",
        "zone",
        "fooId"
      ]
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kOperationTypeJson = R"""({
})""";
  auto const operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Operation",
                DiscoveryTypeVertex("", "", operation_type_json, &pool()));
  auto result = ProcessMethodRequestsAndResponses(resources, types, &pool());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("with non $ref request")));
}

TEST_F(ProcessMethodRequestsAndResponsesTest, TypeInsertError) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "create": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        },
        "zone": {
          "type": "string"
        },
        "fooId": {
          "type": "string"
        }
      },
      "response": {
        "$ref": "Operation"
      },
      "request": {
        "$ref": "Foo"
      },
      "parameterOrder": [
        "project",
        "zone",
        "fooId"
      ]
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto constexpr kOperationTypeJson = R"""({
})""";
  auto const operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  auto constexpr kEmptyTypeJson = R"""({
})""";
  auto const empty_type_json =
      nlohmann::json::parse(kEmptyTypeJson, nullptr, false);
  ASSERT_TRUE(empty_type_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Operation", DiscoveryTypeVertex("Operation", "",
                                                 operation_type_json, &pool()));
  types.emplace("Foos.CreateRequest",
                DiscoveryTypeVertex("", "", empty_type_json, &pool()));
  auto result = ProcessMethodRequestsAndResponses(resources, types, &pool());
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInternal,
                       HasSubstr("Unable to insert type Foos.CreateRequest")));
}

TEST(CreateFilesFromResourcesTest, NonEmptyResources) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "create": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        },
        "zone": {
          "type": "string"
        },
        "fooId": {
          "type": "string"
        }
      },
      "response": {
        "$ref": "Operation"
      },
      "request": {
        "$ref": "Foo"
      },
      "parameterOrder": [
        "project",
        "zone",
        "fooId"
      ]
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace(
      "foos",
      DiscoveryResource("foos", "google.cloud.cpp.product_name.foos.version",
                        resource_json));
  DiscoveryDocumentProperties props{"", "", "product_name", "version", "",
                                    "", {}};
  auto result = CreateFilesFromResources(resources, props, "tmp");
  ASSERT_THAT(result, SizeIs(1));
  EXPECT_THAT(result.front().resource_name(), Eq("foos"));
  EXPECT_THAT(result.front().file_path(),
              Eq("tmp/google/cloud/product_name/foos/version/foos.proto"));
  EXPECT_THAT(result.front().package_name(),
              Eq("google.cloud.cpp.product_name.foos.version"));
}

TEST(CreateFilesFromResourcesTest, EmptyResources) {
  std::map<std::string, DiscoveryResource> resources;
  DiscoveryDocumentProperties props{"", "", "product_name", "version", "",
                                    "", {}};
  auto result = CreateFilesFromResources(resources, props, "tmp");
  EXPECT_THAT(result, IsEmpty());
}

class AssignResourcesAndTypesToFilesTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(AssignResourcesAndTypesToFilesTest,
       TypesContainsBothSynthesizedAndNonsynthesizedTypes) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "create": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ],
      "path": "projects/{project}/zones/{zone}/myResources/{fooId}",
      "httpMethod": "POST",
      "parameters": {
        "project": {
          "type": "string"
        },
        "zone": {
          "type": "string"
        },
        "fooId": {
          "type": "string"
        }
      },
      "response": {
        "$ref": "Operation"
      },
      "request": {
        "$ref": "Foo"
      },
      "parameterOrder": [
        "project",
        "zone",
        "fooId"
      ]
    }
  }
})""";
  auto const resource_json =
      nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  std::map<std::string, DiscoveryResource> resources;
  resources.emplace("foos", DiscoveryResource("foos", "", resource_json));
  std::map<std::string, DiscoveryTypeVertex> types;
  auto constexpr kSynthesizedTypeJson = R"""({
"synthesized_request": true
})""";
  auto const synthesized_type_json =
      nlohmann::json::parse(kSynthesizedTypeJson, nullptr, false);
  ASSERT_TRUE(synthesized_type_json.is_object());
  types.emplace(
      "Foos.CreateRequest",
      DiscoveryTypeVertex("CreateRequest", "", synthesized_type_json, &pool()));
  auto constexpr kOperationTypeJson = R"""({
})""";
  auto const operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  types.emplace("Operation", DiscoveryTypeVertex("Operation", "",
                                                 operation_type_json, &pool()));
  DiscoveryDocumentProperties props{"", "", "product_name", "version", "",
                                    "", {}};
  auto result =
      AssignResourcesAndTypesToFiles(resources, types, props, "output_path");
  ASSERT_THAT(result.size(), Eq(2));
  EXPECT_THAT(
      result[0].file_path(),
      Eq("output_path/google/cloud/product_name/foos/version/foos.proto"));
  EXPECT_THAT(result[0].types(), IsEmpty());
  EXPECT_THAT(result[1].file_path(), Eq("output_path/google/cloud/product_name/"
                                        "version/internal/common.proto"));
  ASSERT_THAT(result[1].types().size(), Eq(1));
  EXPECT_THAT(result[1].types().front()->name(), Eq("Operation"));
}

TEST(DefaultHostFromRootUrlTest, RootUrlFormattedAsExpected) {
  auto constexpr kDocumentJson = R"""({
"rootUrl": "https://default.hostname.com/"
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = DefaultHostFromRootUrl(document_json);
  EXPECT_THAT(result, IsOkAndHolds("default.hostname.com"));
}

TEST(DefaultHostFromRootUrlTest, RootUrlFormattedWithNoTrailingSlash) {
  auto constexpr kDocumentJson = R"""({
"rootUrl": "https://default.hostname.com"
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = DefaultHostFromRootUrl(document_json);
  EXPECT_THAT(result, IsOkAndHolds("default.hostname.com"));
}

TEST(DefaultHostFromRootUrlTest, RootUrlFormattedWithNoPrefix) {
  auto constexpr kDocumentJson = R"""({
"rootUrl": "default.hostname.com"
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = DefaultHostFromRootUrl(document_json);
  EXPECT_THAT(
      result,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "rootUrl field in unexpected format: default.hostname.com")));
}

TEST(GenerateProtosFromDiscoveryDocTest, MissingDocumentProperty) {
  auto constexpr kDocumentJson = R"""({
"rootUrl": "https://default.hostname.com/"
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "", "");
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Missing one or more document properties")));
}

TEST(GenerateProtosFromDiscoveryDocTest, ExtractTypesFromSchemaFailure) {
  auto constexpr kDocumentJson = R"""({
"basePath": "base/path",
"name": "my_product",
"rootUrl": "https://default.hostname.com/",
"version": "v8"
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "", "");
  EXPECT_THAT(
      result,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Discovery Document does not contain schemas element")));
}

TEST(GenerateProtosFromDiscoveryDocTest, EmptyResourcesFailure) {
  auto constexpr kDocumentJson = R"""({
  "basePath": "base/path",
  "name": "my_product",
  "rootUrl": "https://default.hostname.com/",
  "version": "v8",
  "schemas": {
    "foo": {
      "id": "foo",
      "type": "object"
    }
  }
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "", "");
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("No resources found in Discovery Document")));
}

TEST(GenerateProtosFromDiscoveryDocTest, ProcessRequestResponseFailure) {
  auto constexpr kDocumentJson = R"""({
  "basePath": "base/path",
  "name": "my_product",
  "rootUrl": "https://default.hostname.com/",
  "version": "v8",
  "resources": {
    "bar": {
      "methods": {
        "get": {
          "response": {
            "$ref": "baz"
          }
        }
      }
    }
  },
  "schemas": {
    "foo": {
      "id": "foo",
      "type": "object",
      "properties": {}
    }
  }
})""";
  auto const document_json =
      nlohmann::json::parse(kDocumentJson, nullptr, false);
  ASSERT_TRUE(document_json.is_object());
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "", "");
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Response name=baz not found in types")));
}

TEST(FindAllRefValuesTest, NonRefField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "type": "string"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, IsEmpty());
}

TEST(FindAllRefValuesTest, SimpleRefField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "$ref": "Foo"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo"));
}

TEST(FindAllRefValuesTest, MultipleSimpleRefFields) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "$ref": "Foo"
    },
    "field_name_2": {
      "$ref": "Bar"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllRefValuesTest, ArrayRefFields) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "array_field_name_1": {
      "type": "array",
      "items": {
        "$ref": "Foo"
      }
    },
    "array_field_name_2": {
      "type": "array",
      "items": {
        "$ref": "Bar"
      }
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllRefValuesTest, MapRefFields) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "map_field_name_1": {
      "type": "object",
      "additionalProperties": {
        "$ref": "Foo"
      }
    },
    "map_field_name_2": {
      "type": "object",
      "additionalProperties": {
        "$ref": "Bar"
      }
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllRefValuesTest, SingleNestedRefField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "type": "object",
      "properties": {
        "nested_field_1": {
          "$ref": "Foo"
        }
      }
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo"));
}

TEST(FindAllRefValuesTest, MultipleNestedRefFields) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "type": "object",
      "properties": {
        "nested_field_1": {
          "$ref": "Foo"
        },
        "nested_field_2": {
          "$ref": "Bar"
        }
      }
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllRefValuesTest, SingleNestedNestedRefField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "type": "object",
      "properties": {
        "nested_field_1": {
          "$ref": "Foo"
        },
        "nested_field_2": {
          "type": "object",
          "properties": {
            "nested_nested_field_1": {
              "$ref": "Bar"
            }
          }
        }
      }
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

auto constexpr kOperationJson = R"""({
      "type": "object",
      "properties": {
        "operationGroupId": {
          "type": "string"
        },
        "error": {
          "type": "object",
          "properties": {
            "errors": {
              "items": {
                "type": "object",
                "properties": {
                  "message": {
                    "type": "string"
                  },
                  "code": {
                    "type": "string"
                  },
                  "location": {
                    "type": "string"
                  },
                  "errorDetails": {
                    "items": {
                      "type": "object",
                      "properties": {
                        "localizedMessage": {
                          "$ref": "LocalizedMessage"
                        },
                        "quotaInfo": {
                          "$ref": "QuotaExceededInfo"
                        },
                        "errorInfo": {
                          "$ref": "ErrorInfo"
                        },
                        "help": {
                          "$ref": "Help"
                        },
                        "labels": {
                          "type": "object",
                          "additionalProperties": {
                            "$ref": "Label2"
                          }
                        }
                      }
                    },
                    "type": "array"
                  }
                }
              },
              "type": "array"
            }
          }
        },
        "clientOperationId": {
          "type": "string"
        },
        "httpErrorStatusCode": {
          "type": "integer",
          "format": "int32"
        },
        "status": {
          "type": "string",
          "enum": [
            "DONE",
            "PENDING",
            "RUNNING"
          ],
          "enumDescriptions": [
            "",
            "",
            ""
          ]
        },
        "progress": {
          "format": "int32",
          "type": "integer"
        },
        "creationTimestamp": {
          "$ref": "Timestamp"
        },
        "insertTime": {
          "$ref": "Timestamp"
        },
        "endTime": {
          "$ref": "Timestamp"
        },
        "zone": {
          "type": "string"
        },
        "labels": {
          "type": "object",
          "additionalProperties": {
            "$ref": "Label"
          }
        }
      },
      "id": "Operation"
})""";

TEST(FindAllRefValuesTest, ComplexJsonWithRefTypes) {
  auto const parsed_json =
      nlohmann::json::parse(kOperationJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllRefValues(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre(
                          "LocalizedMessage", "QuotaExceededInfo", "ErrorInfo",
                          "Help", "Timestamp", "Label", "Label2"));
}

class EstablishTypeDependenciesTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(EstablishTypeDependenciesTest, DependedTypeNotFound) {
  std::map<std::string, DiscoveryTypeVertex> types;
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "$ref": "Foo"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto iter = types.emplace(
      "Bar", DiscoveryTypeVertex{"Bar", "package_name", parsed_json, &pool()});
  ASSERT_TRUE(iter.second);
  auto result = EstablishTypeDependencies(types);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("Unknown depended upon type: Foo")));
  EXPECT_THAT(
      result.error_info().metadata(),
      Contains(AllOf(
          (Pair(std::string("dependent type"), std::string("Bar")),
           Pair(std::string("depended upon type"), std::string("Foo"))))));
}

TEST_F(EstablishTypeDependenciesTest, AllTypesLinked) {
  std::map<std::string, DiscoveryTypeVertex> types;

  auto add_to_types = [&](DiscoveryTypeVertex t,
                          DiscoveryTypeVertex*& return_val) {
    auto iter = types.emplace(t.name(), std::move(t));
    ASSERT_TRUE(iter.second);
    return_val = &(iter.first->second);
  };

  DiscoveryTypeVertex* localized_message;
  add_to_types(
      DiscoveryTypeVertex{"LocalizedMessage", "package_name", {}, &pool()},
      localized_message);
  DiscoveryTypeVertex* quota_exceeded_info;
  add_to_types(
      DiscoveryTypeVertex{"QuotaExceededInfo", "package_name", {}, &pool()},
      quota_exceeded_info);
  DiscoveryTypeVertex* error_info;
  add_to_types(DiscoveryTypeVertex{"ErrorInfo", "package_name", {}, &pool()},
               error_info);
  DiscoveryTypeVertex* help;
  add_to_types(DiscoveryTypeVertex{"Help", "package_name", {}, &pool()}, help);
  DiscoveryTypeVertex* timestamp;
  add_to_types(DiscoveryTypeVertex{"Timestamp", "package_name", {}, &pool()},
               timestamp);
  DiscoveryTypeVertex* label;
  add_to_types(DiscoveryTypeVertex{"Label", "package_name", {}, &pool()},
               label);
  DiscoveryTypeVertex* label2;
  add_to_types(DiscoveryTypeVertex{"Label2", "package_name", {}, &pool()},
               label2);

  auto const operation_json =
      nlohmann::json::parse(kOperationJson, nullptr, false);
  ASSERT_TRUE(operation_json.is_object());
  auto operation_iter = types.emplace(
      "Operation", DiscoveryTypeVertex{"Operation", "package_name",
                                       operation_json, &pool()});
  ASSERT_TRUE(operation_iter.second);
  DiscoveryTypeVertex* operation = &operation_iter.first->second;
  auto result = EstablishTypeDependencies(types);
  ASSERT_STATUS_OK(result);

  auto named = [](std::string const& name) {
    return Pointee(Property(&DiscoveryTypeVertex::name, Eq(name)));
  };

  EXPECT_THAT(operation->needs_type(),
              UnorderedElementsAre(
                  named("LocalizedMessage"), named("QuotaExceededInfo"),
                  named("ErrorInfo"), named("Help"), named("Timestamp"),
                  named("Label"), named("Label2")));
  EXPECT_THAT(operation->needed_by_type(), IsEmpty());

  EXPECT_THAT(localized_message->needs_type(), IsEmpty());
  EXPECT_THAT(localized_message->needed_by_type(),
              UnorderedElementsAre(named("Operation")));
  EXPECT_THAT(quota_exceeded_info->needs_type(), IsEmpty());
  EXPECT_THAT(quota_exceeded_info->needed_by_type(),
              UnorderedElementsAre(named("Operation")));
  EXPECT_THAT(error_info->needs_type(), IsEmpty());
  EXPECT_THAT(error_info->needed_by_type(),
              UnorderedElementsAre(named("Operation")));
  EXPECT_THAT(help->needs_type(), IsEmpty());
  EXPECT_THAT(help->needed_by_type(), UnorderedElementsAre(named("Operation")));
  EXPECT_THAT(timestamp->needs_type(), IsEmpty());
  EXPECT_THAT(timestamp->needed_by_type(),
              UnorderedElementsAre(named("Operation")));
  EXPECT_THAT(label->needs_type(), IsEmpty());
  EXPECT_THAT(label->needed_by_type(),
              UnorderedElementsAre(named("Operation")));
  EXPECT_THAT(label2->needs_type(), IsEmpty());
  EXPECT_THAT(label2->needed_by_type(),
              UnorderedElementsAre(named("Operation")));
}

class ApplyResourceLabelsToTypesTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(ApplyResourceLabelsToTypesTest,
       LabelsAllRequestAndResponseDependedTypes) {
  std::map<std::string, DiscoveryTypeVertex> types;
  auto add_to_types = [&](DiscoveryTypeVertex t,
                          DiscoveryTypeVertex*& return_val) {
    auto iter = types.emplace(t.name(), std::move(t));
    ASSERT_TRUE(iter.second);
    return_val = &(iter.first->second);
  };

  DiscoveryTypeVertex* localized_message;
  add_to_types(
      DiscoveryTypeVertex{"LocalizedMessage", "package_name", {}, &pool()},
      localized_message);
  DiscoveryTypeVertex* quota_exceeded_info;
  add_to_types(
      DiscoveryTypeVertex{"QuotaExceededInfo", "package_name", {}, &pool()},
      quota_exceeded_info);
  DiscoveryTypeVertex* error_info;
  add_to_types(DiscoveryTypeVertex{"ErrorInfo", "package_name", {}, &pool()},
               error_info);
  DiscoveryTypeVertex* help;
  add_to_types(DiscoveryTypeVertex{"Help", "package_name", {}, &pool()}, help);
  DiscoveryTypeVertex* timestamp;
  add_to_types(DiscoveryTypeVertex{"Timestamp", "package_name", {}, &pool()},
               timestamp);
  DiscoveryTypeVertex* label;
  add_to_types(DiscoveryTypeVertex{"Label", "package_name", {}, &pool()},
               label);
  DiscoveryTypeVertex* label2;
  add_to_types(DiscoveryTypeVertex{"Label2", "package_name", {}, &pool()},
               label2);
  auto const operation_json =
      nlohmann::json::parse(kOperationJson, nullptr, false);
  ASSERT_TRUE(operation_json.is_object());
  auto operation_iter = types.emplace(
      "Operation", DiscoveryTypeVertex{"Operation", "package_name",
                                       operation_json, &pool()});
  ASSERT_TRUE(operation_iter.second);
  DiscoveryTypeVertex* operation = &operation_iter.first->second;
  auto result = EstablishTypeDependencies(types);
  ASSERT_STATUS_OK(result);

  std::map<std::string, DiscoveryResource> resources;
  auto resource_iter = resources.emplace(
      "resource_name", DiscoveryResource("resource_name", "package_name", {}));
  ASSERT_TRUE(resource_iter.second);
  auto& resource = resource_iter.first->second;
  resource.AddResponseType("Operation", operation);

  auto other_resource_iter = resources.emplace(
      "other_resource_name",
      DiscoveryResource("other_resource_name", "package_name", {}));
  ASSERT_TRUE(other_resource_iter.second);
  auto& other_resource = other_resource_iter.first->second;
  other_resource.AddRequestType("LocalizedMessage", localized_message);

  ApplyResourceLabelsToTypes(resources);
  EXPECT_THAT(operation->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(quota_exceeded_info->needed_by_resource(),
              ElementsAre("resource_name"));
  EXPECT_THAT(error_info->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(help->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(timestamp->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(label->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(label2->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(localized_message->needed_by_resource(),
              ElementsAre("other_resource_name", "resource_name"));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
