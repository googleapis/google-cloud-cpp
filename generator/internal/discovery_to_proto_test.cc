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
                              HasSubstr("unrecognized schema type")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("MissingType type is not in "
                                 "`recognized_types`; is instead untyped")));
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
                              HasSubstr("unrecognized schema type")));
  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr(
          "NonObject type is not in `recognized_types`; is instead array")));
}

TEST_F(ExtractTypesFromSchemaTest, SchemaAnyType) {
  auto constexpr kDiscoveryDocWithAnyTypeSchema = R"""(
{
  "schemas": {
    "Foo": {
      "id": "Foo",
      "type": "any"
    }
  }
}
)""";

  testing_util::ScopedLog log;
  auto parsed_json =
      nlohmann::json::parse(kDiscoveryDocWithAnyTypeSchema, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto types = ExtractTypesFromSchema({}, parsed_json, &pool());
  ASSERT_THAT(types, IsOk());
  EXPECT_THAT(*types, UnorderedElementsAre(Key("Foo")));
}

TEST(ExtractResourcesTest, EmptyResources) {
  auto resources = ExtractResources({}, {});
  EXPECT_THAT(resources,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("No resources found in Discovery Document.")));
}

TEST(ExtractResourcesTest, NonEmptyResources) {
  auto constexpr kResourceJson = R"""({
  "resources": {
    "resource1": {
      "methods": {
        "method0": {
        }
      }
    },
    "resource2": {
      "methods": {
        "method0": {
        }
      }
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto resources = ExtractResources({}, resource_json);
  ASSERT_STATUS_OK(resources);
  EXPECT_THAT(*resources,
              UnorderedElementsAre(Key("resource1"), Key("resource2")));
}

TEST(ExtractResourcesTest, SameApiVersionsSpecified) {
  auto constexpr kResourceJson = R"""({
  "resources": {
    "resource1": {
      "methods": {
        "emptyResponseMethod1": {
          "apiVersion": "test-api-version"
        },
        "emptyResponseMethod2": {
          "apiVersion": "test-api-version"
        }
      }
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto resources = ExtractResources({}, resource_json);
  EXPECT_THAT(resources, IsOk());
}

TEST(ExtractResourcesTest, DifferentApiVersionsSpecified) {
  auto constexpr kResourceJson = R"""({
  "resources": {
    "resource1": {
      "methods": {
        "emptyResponseMethod1": {
          "apiVersion": "test-api-version"
        },
        "emptyResponseMethod2": {
          "apiVersion": "other-test-api-version"
        }
      }
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto resources = ExtractResources({}, resource_json);
  EXPECT_THAT(
      resources,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "resource contains methods with different apiVersion values")));
}

TEST(ExtractResourcesTest, SomeApiVersionsSpecified) {
  auto constexpr kResourceJson = R"""({
  "resources": {
    "resource1": {
      "methods": {
        "emptyResponseMethod1": {
          "apiVersion": "test-api-version"
        },
        "emptyResponseMethod2": {
        }
      }
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto resources = ExtractResources({}, resource_json);
  EXPECT_THAT(
      resources,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "resource contains methods with different apiVersion values")));

  auto constexpr kMethodOrderReversedResourceJson = R"""({
  "resources": {
    "resource1": {
      "methods": {
        "emptyResponseMethod2": {
        },
        "emptyResponseMethod1": {
          "apiVersion": "test-api-version"
        }
      }
    }
  }
})""";
  resource_json =
      nlohmann::json::parse(kMethodOrderReversedResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  resources = ExtractResources({}, resource_json);
  EXPECT_THAT(
      resources,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "resource contains methods with different apiVersion values")));
}

TEST(ExtractResourcesTest, OnlyLastMethodApiVersionsSpecified) {
  auto constexpr kResourceJson = R"""({
  "resources": {
    "resource1": {
      "methods": {
        "emptyResponseMethod1": {
        },
        "emptyResponseMethod2": {
        },
        "emptyResponseMethod3": {
          "apiVersion": "test-api-version"
        }
      }
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto resources = ExtractResources({}, resource_json);
  EXPECT_THAT(
      resources,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "resource contains methods with different apiVersion values")));
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
  auto constexpr kResourceJson = R"""({
  "methods": {
    "get": {
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
  }
}
})""";
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
"description":"Request message for GetFoo.",
"id":"GetFooRequest",
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
                                    "", {}, "2023"};
  auto result = CreateFilesFromResources(resources, props, "tmp", {});
  ASSERT_THAT(result, SizeIs(1));
  EXPECT_THAT(result.front().resource_name(), Eq("foos"));
  EXPECT_THAT(result.front().file_path(),
              Eq("tmp/google/cloud/product_name/foos/version/foos.proto"));
  EXPECT_THAT(result.front().package_name(),
              Eq("google.cloud.cpp.product_name.foos.version"));
  EXPECT_THAT(result.front().import_paths(),
              UnorderedElementsAre("google/api/annotations.proto",
                                   "google/api/client.proto",
                                   "google/api/field_behavior.proto"));
}

TEST(CreateFilesFromResourcesTest, EmptyResources) {
  std::map<std::string, DiscoveryResource> resources;
  DiscoveryDocumentProperties props{"", "", "product_name", "version", "",
                                    "", {}, "2023"};
  auto result = CreateFilesFromResources(resources, props, "tmp", {});
  EXPECT_THAT(result, IsEmpty());
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
  auto result = GenerateProtosFromDiscoveryDoc(
      document_json, "", "", "", "", "",
      /*enable_parallel_write_for_discovery_protos=*/false);
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
  auto result = GenerateProtosFromDiscoveryDoc(
      document_json, "", "", "", "", "",
      /*enable_parallel_write_for_discovery_protos=*/false);
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
  auto result = GenerateProtosFromDiscoveryDoc(
      document_json, "", "", "", "", "",
      /*enable_parallel_write_for_discovery_protos=*/false);
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
  auto result = GenerateProtosFromDiscoveryDoc(
      document_json, "", "", "", "", "",
      /*enable_parallel_write_for_discovery_protos=*/false);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Response name=baz not found in types")));
}

TEST(FindAllTypesToImportTest, NonRefNonAnyField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "type": "string"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, IsEmpty());
}

TEST(FindAllTypesToImportTest, SimpleRefField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "$ref": "Foo"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo"));
}

TEST(FindAllTypesToImportTest, SimpleAnyField) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "field_name_1": {
      "type": "any"
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("google.protobuf.Any"));
}

TEST(FindAllTypesToImportTest, MultipleSimpleRefFields) {
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
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllTypesToImportTest, ArrayRefFields) {
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
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllTypesToImportTest, ArrayRefAnyFields) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "array_field_name_1": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties" : {
          "type": "any"
        }
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
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("google.protobuf.Any", "Bar"));
}

TEST(FindAllTypesToImportTest, MapRefFields) {
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
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllTypesToImportTest, MapAnyFields) {
  auto constexpr kTypeJson = R"""({
  "properties": {
    "map_field_name_1": {
      "type": "object",
      "additionalProperties": {
        "type": "any"
      }
    }
  }
})""";

  auto const parsed_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("google.protobuf.Any"));
}

TEST(FindAllTypesToImportTest, SingleNestedRefField) {
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
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo"));
}

TEST(FindAllTypesToImportTest, MultipleNestedRefFields) {
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
  auto result = FindAllTypesToImport(parsed_json);
  EXPECT_THAT(result, UnorderedElementsAre("Foo", "Bar"));
}

TEST(FindAllTypesToImportTest, SingleNestedNestedRefField) {
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
  auto result = FindAllTypesToImport(parsed_json);
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

TEST(FindAllTypesToImportTest, ComplexJsonWithRefTypes) {
  auto const parsed_json =
      nlohmann::json::parse(kOperationJson, nullptr, false);
  ASSERT_TRUE(parsed_json.is_object());
  auto result = FindAllTypesToImport(parsed_json);
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

TEST_F(ApplyResourceLabelsToTypesTest, HandleCirclularDependency) {
  // Create the nested schema dependency. The following jsons establish a
  // recursive relationship in the TableFieldSchema.
  auto constexpr kTableSchemaJson = R"""({
      "description": "Schema of a table",
      "id": "TableSchema",
      "properties": {
        "fields": {
          "description": "Describes the fields in a table.",
          "items": {
            "$ref": "TableFieldSchema"
          },
          "type": "array"
        }
      },
      "type": "object"
})""";
  auto const table_schema_json =
      nlohmann::json::parse(kTableSchemaJson, nullptr, false);
  ASSERT_TRUE(table_schema_json.is_object());

  auto constexpr kTableFieldSchemaJson = R"""({
        "id": "TableFieldSchema",
        "type": "object",
        "properties": {
            "fields": {
            "items": {
              "$ref": "TableFieldSchema"
            },
            "type": "array"
          }
        }
  })""";
  auto const table_field_schema_json =
      nlohmann::json::parse(kTableFieldSchemaJson, nullptr, false);
  ASSERT_TRUE(table_field_schema_json.is_object());

  // Create a map of the types to establish the type dependencies.
  std::map<std::string, DiscoveryTypeVertex> types;
  auto add_to_types = [&](DiscoveryTypeVertex t,
                          DiscoveryTypeVertex*& return_val) {
    auto iter = types.emplace(t.name(), std::move(t));
    ASSERT_TRUE(iter.second);
    return_val = &(iter.first->second);
  };

  DiscoveryTypeVertex* table_schema;
  add_to_types(DiscoveryTypeVertex{"TableSchema", "package_name",
                                   table_schema_json, &pool()},
               table_schema);
  DiscoveryTypeVertex* table_field_schema;
  add_to_types(DiscoveryTypeVertex{"TableFieldSchema", "package_name",
                                   table_field_schema_json, &pool()},
               table_field_schema);

  // Create the corresponding resource that refers to the problematic schema.
  auto constexpr kQueryResponseJson = R"""({
        "id": "QueryResponse",
        "properties": {
          "schema": {
            "$ref": "TableSchema"
          }
        },
        "type": "object"
  })""";
  auto const query_response_json =
      nlohmann::json::parse(kQueryResponseJson, nullptr, false);
  ASSERT_TRUE(query_response_json.is_object());
  // Add the QueryResponse schema which references the recursive schema.s
  auto response_iter = types.emplace(
      "QueryResponse", DiscoveryTypeVertex{"QueryResponse", "package_name",
                                           query_response_json, &pool()});
  ASSERT_TRUE(response_iter.second);
  DiscoveryTypeVertex* response = &response_iter.first->second;
  // Establish the type dependencies based on the given jsons. This will add
  // metadata to each node based on the "$ref" tag in the json.
  auto result = EstablishTypeDependencies(types);
  ASSERT_STATUS_OK(result);

  // Create a DiscoveryResource with the QueryResponse type.
  std::map<std::string, DiscoveryResource> resources;
  auto resource_iter = resources.emplace(
      "resource_name", DiscoveryResource("resource_name", "package_name", {}));
  ASSERT_TRUE(resource_iter.second);
  auto& resource = resource_iter.first->second;
  resource.AddResponseType("QueryResponse", response);

  ApplyResourceLabelsToTypes(resources);

  EXPECT_THAT(response->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(table_schema->needed_by_resource(), ElementsAre("resource_name"));
  EXPECT_THAT(table_field_schema->needed_by_resource(),
              ElementsAre("resource_name"));
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
                                    "", {}, "2023"};
  auto result = AssignResourcesAndTypesToFiles(
      resources, types, props, "output_path", "export_output_path");
  ASSERT_STATUS_OK(result);
  ASSERT_THAT(result->first.size(), Eq(2));
  EXPECT_THAT(
      result->first[0].file_path(),
      Eq("output_path/google/cloud/product_name/foos/version/foos.proto"));
  EXPECT_THAT(result->first[0].types(), IsEmpty());
  EXPECT_THAT(result->first[1].file_path(),
              Eq("output_path/google/cloud/product_name/"
                 "version/internal/common_000.proto"));
  ASSERT_THAT(result->first[1].types().size(), Eq(1));
  EXPECT_THAT(result->first[1].types().front()->name(), Eq("Operation"));

  ASSERT_THAT(result->second, SizeIs(1));
  EXPECT_THAT(result->second[0].relative_file_path(),
              Eq("google/cloud/product_name/foos/version/foos_proto_export.h"));
  EXPECT_THAT(result->second[0].proto_includes(), IsEmpty());
}

TEST_F(AssignResourcesAndTypesToFilesTest, ResourceAndCommonFilesWithImports) {
  //  The JSON Discovery doc defines two resource:
  //    disks
  //    foos
  //  As well as the schema:
  //    CustomerEncryptionKey
  //    Disk
  //    DiskAsyncReplication
  //    DiskAsyncReplicationList
  //    ErrorInfo
  //    GuestOsFeature
  //    Operation
  //    Snapshot
  //    LocalizedMessage
  //    TestPermissionsRequest
  //    TestPermissionsResponse
  //    OtherCommonSchema
  //  The rpcs of the services use these schema in both request and response
  //  roles.
  auto constexpr kDiscoveryJson = R"""(
{
  "resources": {
    "disks": {
      "methods": {
        "testIamPermissions": {
          "httpMethod": "POST",
          "id": "compute.disks.testIamPermissions",
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute",
            "https://www.googleapis.com/auth/compute.readonly"
          ],
          "request": {
            "$ref": "TestPermissionsRequest"
          },
          "parameterOrder": [
            "project",
            "zone",
            "resource"
          ],
          "response": {
            "$ref": "TestPermissionsResponse"
          },
          "path": "projects/{project}/zones/{zone}/disks/{resource}/testIamPermissions",
          "parameters": {
            "resource": {
              "type": "string",
              "location": "path",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?|[1-9][0-9]{0,19}",
              "required": true
            },
            "zone": {
              "type": "string",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
              "location": "path",
              "required": true
            },
            "project": {
              "location": "path",
              "type": "string",
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))",
              "required": true
            }
          }
        },
        "delete": {
          "parameters": {
            "disk": {
              "location": "path",
              "type": "string",
              "required": true
            },
            "zone": {
              "type": "string",
              "location": "path",
              "required": true,
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?"
            },
            "project": {
              "location": "path",
              "type": "string",
              "required": true,
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))"
            },
            "requestId": {
              "type": "string",
              "location": "query"
            }
          },
          "path": "projects/{project}/zones/{zone}/disks/{disk}",
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute"
          ],
          "id": "compute.disks.delete",
          "parameterOrder": [
            "project",
            "zone",
            "disk"
          ],
          "response": {
            "$ref": "Operation"
          },
          "httpMethod": "DELETE"
        },
        "insert": {
          "parameterOrder": [
            "project",
            "zone"
          ],
          "response": {
            "$ref": "Operation"
          },
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute"
          ],
          "httpMethod": "POST",
          "parameters": {
            "zone": {
              "required": true,
              "type": "string",
              "location": "path",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?"
            },
            "sourceImage": {
              "location": "query",
              "type": "string"
            },
            "project": {
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))",
              "type": "string",
              "location": "path",
              "required": true
            },
            "requestId": {
              "location": "query",
              "type": "string"
            }
          },
          "id": "compute.disks.insert",
          "path": "projects/{project}/zones/{zone}/disks",
          "request": {
            "$ref": "Disk"
          }
        },
        "createSnapshot": {
          "path": "projects/{project}/zones/{zone}/disks/{disk}/createSnapshot",
          "request": {
            "$ref": "Snapshot"
          },
          "parameters": {
            "guestFlush": {
              "type": "boolean",
              "location": "query"
            },
            "zone": {
              "type": "string",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
              "location": "path",
              "required": true
            },
            "project": {
              "required": true,
              "type": "string",
              "location": "path",
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))"
            },
            "requestId": {
              "type": "string",
              "location": "query"
            },
            "disk": {
              "type": "string",
              "location": "path",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?|[1-9][0-9]{0,19}",
              "required": true
            }
          },
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute"
          ],
          "id": "compute.disks.createSnapshot",
          "httpMethod": "POST",
          "parameterOrder": [
            "project",
            "zone",
            "disk"
          ],
          "response": {
            "$ref": "Operation"
          }
        },
        "get": {
          "httpMethod": "GET",
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute",
            "https://www.googleapis.com/auth/compute.readonly"
          ],
          "id": "compute.disks.get",
          "parameters": {
            "project": {
              "type": "string",
              "location": "path",
              "required": true,
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))"
            },
            "disk": {
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?|[1-9][0-9]{0,19}",
              "location": "path",
              "required": true,
              "type": "string"
            },
            "zone": {
              "type": "string",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
              "required": true,
              "location": "path"
            }
          },
          "response": {
            "$ref": "Disk"
          },
          "path": "projects/{project}/zones/{zone}/disks/{disk}",
          "parameterOrder": [
            "project",
            "zone",
            "disk"
          ]
        }
      }
    },
    "foos": {
      "methods": {
        "testIamPermissions": {
          "httpMethod": "POST",
          "id": "compute.foos.testIamPermissions",
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute",
            "https://www.googleapis.com/auth/compute.readonly"
          ],
          "request": {
            "$ref": "TestPermissionsRequest"
          },
          "parameterOrder": [
            "project",
            "zone",
            "resource"
          ],
          "response": {
            "$ref": "TestPermissionsResponse"
          },
          "path": "projects/{project}/zones/{zone}/foos/{resource}/testIamPermissions",
          "parameters": {
            "resource": {
              "type": "string",
              "location": "path",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?|[1-9][0-9]{0,19}",
              "required": true
            },
            "zone": {
              "type": "string",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
              "location": "path",
              "required": true
            },
            "project": {
              "location": "path",
              "type": "string",
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))",
              "required": true
            }
          }
        },
        "emptyResponseMethod": {
          "httpMethod": "POST",
          "id": "compute.foos.emptyResponseMethod",
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute",
            "https://www.googleapis.com/auth/compute.readonly"
          ],
          "request": {
            "$ref": "LocalizedMessage"
          },
          "parameterOrder": [
            "project",
            "zone",
            "resource"
          ],
          "path": "projects/{project}/zones/{zone}/foos/{resource}/emptyResponseMethod",
          "parameters": {
            "resource": {
              "type": "string",
              "location": "path",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?|[1-9][0-9]{0,19}",
              "required": true
            },
            "zone": {
              "type": "string",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
              "location": "path",
              "required": true
            },
            "project": {
              "location": "path",
              "type": "string",
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))",
              "required": true
            }
          }
        },
        "otherCommonTypeMethod": {
          "httpMethod": "POST",
          "id": "compute.foos.otherCommonTypeMethod",
          "scopes": [
            "https://www.googleapis.com/auth/cloud-platform",
            "https://www.googleapis.com/auth/compute",
            "https://www.googleapis.com/auth/compute.readonly"
          ],
          "request": {
            "$ref": "OtherCommonSchema"
          },
          "parameterOrder": [
            "project",
            "zone",
            "resource"
          ],
          "path": "projects/{project}/zones/{zone}/foos/{resource}/otherCommonTypeMethod",
          "parameters": {
            "resource": {
              "type": "string",
              "location": "path",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?|[1-9][0-9]{0,19}",
              "required": true
            },
            "zone": {
              "type": "string",
              "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
              "location": "path",
              "required": true
            },
            "project": {
              "location": "path",
              "type": "string",
              "pattern": "(?:(?:[-a-z0-9]{1,63}\\.)*(?:[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?):)?(?:[0-9]{1,19}|(?:[a-z0-9](?:[-a-z0-9]{0,61}[a-z0-9])?))",
              "required": true
            }
          }
        }
      }
    }
  },
  "schemas": {
    "CustomerEncryptionKey": {
      "id": "CustomerEncryptionKey",
      "type": "object",
      "properties": {
        "sha256": {
          "type": "string"
        },
        "rsaEncryptedKey": {
          "type": "string"
        },
        "rawKey": {
          "type": "string"
        },
        "kmsKeyName": {
          "type": "string"
        },
        "kmsKeyServiceAccount": {
          "type": "string"
        }
      }
    },
    "Disk": {
      "id": "Disk",
      "type": "object",
      "properties": {
        "diskEncryptionKey": {
          "$ref": "CustomerEncryptionKey"
        },
        "asyncSecondaryDisks": {
          "type": "object",
          "additionalProperties": {
            "$ref": "DiskAsyncReplicationList"
          }
        },
        "sourceImageEncryptionKey": {
          "$ref": "CustomerEncryptionKey"
        },
        "status": {
          "enumDescriptions": [
            "Disk is provisioning",
            "Disk is deleting.",
            "Disk creation failed.",
            "Disk is ready for use.",
            "Source data is being copied into the disk."
          ],
          "enum": [
            "CREATING",
            "DELETING",
            "FAILED",
            "READY",
            "RESTORING"
          ],
          "type": "string"
        },
        "description": {
          "type": "string"
        },
        "id": {
          "type": "string"
        },
        "labels": {
          "additionalProperties": {
            "type": "string"
          },
          "type": "object"
        },
        "zone": {
          "type": "string"
        },
        "sourceDiskId": {
          "type": "string"
        },
        "name": {
          "pattern": "[a-z](?:[-a-z0-9]{0,61}[a-z0-9])?",
          "type": "string",
          "annotations": {
            "required": [
              "compute.disks.insert"
            ]
          }
        },
        "guestOsFeatures": {
          "items": {
            "$ref": "GuestOsFeature"
          },
          "type": "array"
        },
        "sourceSnapshotEncryptionKey": {
          "$ref": "CustomerEncryptionKey"
        },
        "type": {
          "type": "string"
        }
      }
    },
    "DiskAsyncReplication": {
      "id": "DiskAsyncReplication",
      "type": "object",
      "properties": {
        "disk": {
          "type": "string"
        }
      }
    },
    "DiskAsyncReplicationList": {
      "type": "object",
      "properties": {
        "asyncReplicationDisk": {
          "$ref": "DiskAsyncReplication"
        }
      },
      "id": "DiskAsyncReplicationList"
    },
    "ErrorInfo": {
      "id": "ErrorInfo",
      "properties": {
        "domain": {
          "type": "string"
        },
        "reason": {
          "type": "string"
        },
        "metadatas": {
          "additionalProperties": {
            "type": "string"
          },
          "type": "object"
        }
      },
      "type": "object"
    },
    "GuestOsFeature": {
      "type": "object",
      "properties": {
        "type": {
          "enum": [
            "FEATURE_TYPE_UNSPECIFIED",
            "GVNIC",
            "MULTI_IP_SUBNET",
            "SECURE_BOOT",
            "SEV_CAPABLE",
            "SEV_LIVE_MIGRATABLE",
            "SEV_SNP_CAPABLE",
            "UEFI_COMPATIBLE",
            "VIRTIO_SCSI_MULTIQUEUE",
            "WINDOWS"
          ],
          "enumDescriptions": [
            "","","","","","","","","",""
          ],
          "type": "string"
        }
      },
      "id": "GuestOsFeature"
    },
    "LocalizedMessage": {
      "id": "LocalizedMessage",
      "properties": {
        "locale": {
          "type": "string"
        },
        "message": {
          "type": "string"
        }
      },
      "type": "object"
    },
    "Operation": {
      "type": "object",
      "properties": {
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
                        "errorInfo": {
                          "$ref": "ErrorInfo"
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
        }
      },
      "id": "Operation"
    },
    "OtherCommonSchema": {
      "id": "OtherCommonSchema",
      "type": "object",
      "properties": {
        "field_name": {
          "type": "string"
        }
      }
    },
    "Snapshot": {
      "properties": {
        "id": {
          "type": "string"
        },
        "snapshotEncryptionKey": {
          "$ref": "CustomerEncryptionKey"
        }
      },
      "id": "Snapshot",
      "type": "object"
    },
    "TestPermissionsRequest": {
      "type": "object",
      "properties": {
        "permissions": {
          "items": {
            "type": "string"
          },
          "type": "array"
        }
      },
      "id": "TestPermissionsRequest"
    },
    "TestPermissionsResponse": {
      "properties": {
        "permissions": {
          "items": {
            "type": "string"
          },
          "type": "array"
        }
      },
      "type": "object",
      "id": "TestPermissionsResponse"
    },
    "ResponseWithAny": {
      "properties": {
        "permissions": {
          "items": {
            "type": "object",
            "additionalProperties": {
              "type": "any"
            }
          },
          "type": "array"
        }
      },
      "type": "object",
      "id": "TestPermissionsResponse"
    }
  }
}
)""";
  auto const discovery_json =
      nlohmann::json::parse(kDiscoveryJson, nullptr, false);
  ASSERT_TRUE(discovery_json.is_object());
  DiscoveryDocumentProperties document_properties{
      "", "", "product_name", "version", "", "", {}, "2023"};
  auto types =
      ExtractTypesFromSchema(document_properties, discovery_json, &pool());
  ASSERT_STATUS_OK(types);
  auto resources = ExtractResources(document_properties, discovery_json);
  ASSERT_STATUS_OK(resources);
  auto method_status =
      ProcessMethodRequestsAndResponses(*resources, *types, &pool());
  ASSERT_STATUS_OK(method_status);
  EstablishTypeDependencies(*types);
  ApplyResourceLabelsToTypes(*resources);
  auto files =
      AssignResourcesAndTypesToFiles(*resources, *types, document_properties,
                                     "output_path", "export_output_path");
  ASSERT_STATUS_OK(files);

  //  The resulting set of proto files contains one file per resource as well as
  //  a minimal number of common files containing shared types that are
  //  imported. Package names are leveraged to allow us to discriminate when
  //  type names are synthesized from rpc/method names.
  //
  //  Below is a pseudo-proto representation of each file:
  //  file: google/cloud/product_name/disks/version/disks.proto
  //    package: google.cloud.cpp.product_name.disks.version
  //    import: google/api/annotations.proto
  //    import: google/api/client.proto
  //    import: google/api/field_behavior.proto
  //    import: google/cloud/extended_operations.proto
  //    import: google/cloud/product_name/version/internal/common_000.proto
  //    import: google/cloud/product_name/version/internal/common_001.proto
  //    type: CreateSnapshotRequest
  //    type: DeleteDiskRequest
  //    type: GetDiskRequest
  //    type: InsertDiskRequest
  //    type: TestIamPermissionsRequest
  //  file: google/cloud/product_name/foos/version/foos.proto
  //    package: google.cloud.cpp.product_name.foos.version
  //    import: google/api/annotations.proto
  //    import: google/api/client.proto
  //    import: google/api/field_behavior.proto
  //    import: google/cloud/product_name/version/internal/common_001.proto
  //    import: google/cloud/product_name/version/internal/common_002.proto
  //    import: google/protobuf/empty.proto
  //    type: EmptyResponseMethodRequest
  //    type: OtherCommonTypeMethodRequest
  //    type: TestIamPermissionsRequest
  //  file: google/cloud/product_name/version/internal/common_000.proto
  //    package: google.cloud.cpp.product_name.version
  //    import: google/cloud/product_name/version/internal/common_001.proto
  //    type: CustomerEncryptionKey
  //    type: Disk
  //    type: DiskAsyncReplication
  //    type: DiskAsyncReplicationList
  //    type: ErrorInfo
  //    type: GuestOsFeature
  //    type: Operation
  //    type: Snapshot
  //  file: google/cloud/product_name/version/internal/common_001.proto
  //    package: google.cloud.cpp.product_name.version
  //    type: LocalizedMessage
  //    type: TestPermissionsRequest
  //    type: TestPermissionsResponse
  //  file: google/cloud/product_name/version/internal/common_002.proto
  //    package: google.cloud.cpp.product_name.version
  //    type: OtherCommonSchema

  auto relative_proto_path = [](std::string const& path) {
    return Property(&DiscoveryFile::relative_proto_path, Eq(path));
  };

  EXPECT_THAT(
      files->first,
      UnorderedElementsAre(
          relative_proto_path(
              "google/cloud/product_name/foos/version/foos.proto"),
          relative_proto_path(
              "google/cloud/product_name/disks/version/disks.proto"),
          relative_proto_path("google/cloud/product_name/version/internal/"
                              "common_000.proto"),
          relative_proto_path("google/cloud/product_name/version/internal/"
                              "common_001.proto"),
          relative_proto_path("google/cloud/product_name/version/internal/"
                              "common_002.proto")));

  auto relative_file_path = [](std::string const& path) {
    return Property(&DiscoveryProtoExportFile::relative_file_path, Eq(path));
  };

  EXPECT_THAT(
      files->second,
      UnorderedElementsAre(
          relative_file_path(
              "google/cloud/product_name/foos/version/foos_proto_export.h"),
          relative_file_path(
              "google/cloud/product_name/disks/version/disks_proto_export.h")));

  auto type_named = [](std::string const& name) {
    return Pointee(Property(&DiscoveryTypeVertex::name, Eq(name)));
  };

  // There are no guarantees which generated common_file_xxx.proto the shared
  // schema types exist in, so we have to determine them programmatically.
  auto common_other_schema_file = std::find_if(
      files->first.begin(), files->first.end(), [&](DiscoveryFile const& f) {
        return std::find_if(f.types().begin(), f.types().end(),
                            [&](DiscoveryTypeVertex* t) {
                              return (t->name() == "OtherCommonSchema");
                            }) != f.types().end();
      });
  EXPECT_THAT(common_other_schema_file->package_name(),
              Eq("google.cloud.cpp.product_name.version"));
  EXPECT_THAT(common_other_schema_file->types(),
              UnorderedElementsAre(type_named("OtherCommonSchema")));
  EXPECT_THAT(common_other_schema_file->import_paths(), IsEmpty());

  auto common_test_permissions_file = std::find_if(
      files->first.begin(), files->first.end(), [&](DiscoveryFile const& f) {
        return std::find_if(f.types().begin(), f.types().end(),
                            [&](DiscoveryTypeVertex* t) {
                              return (t->name() == "TestPermissionsRequest");
                            }) != f.types().end();
      });
  EXPECT_THAT(common_test_permissions_file->package_name(),
              Eq("google.cloud.cpp.product_name.version"));
  EXPECT_THAT(common_test_permissions_file->types(),
              UnorderedElementsAre(type_named("LocalizedMessage"),
                                   type_named("TestPermissionsRequest"),
                                   type_named("TestPermissionsResponse")));
  EXPECT_THAT(common_test_permissions_file->import_paths(),
              UnorderedElementsAre("google/protobuf/any.proto"));

  auto common_disk_types_file = std::find_if(
      files->first.begin(), files->first.end(), [&](DiscoveryFile const& f) {
        return std::find_if(f.types().begin(), f.types().end(),
                            [&](DiscoveryTypeVertex* t) {
                              return (t->name() == "Disk");
                            }) != f.types().end();
      });
  EXPECT_THAT(common_disk_types_file->package_name(),
              Eq("google.cloud.cpp.product_name.version"));
  EXPECT_THAT(common_disk_types_file->types(),
              UnorderedElementsAre(
                  type_named("CustomerEncryptionKey"), type_named("Disk"),
                  type_named("DiskAsyncReplication"),
                  type_named("DiskAsyncReplicationList"),
                  type_named("ErrorInfo"), type_named("GuestOsFeature"),
                  type_named("Operation"), type_named("Snapshot")));
  EXPECT_THAT(common_disk_types_file->import_paths(),
              UnorderedElementsAre(
                  common_test_permissions_file->relative_proto_path()));

  // Proto files containing a resource/service have definitive file paths.
  auto disks_file =
      std::find_if(files->first.begin(), files->first.end(), [](auto const& f) {
        return f.relative_proto_path() ==
               "google/cloud/product_name/disks/version/disks.proto";
      });
  EXPECT_THAT(disks_file->package_name(),
              Eq("google.cloud.cpp.product_name.disks.version"));
  EXPECT_THAT(disks_file->types(),
              UnorderedElementsAre(type_named("CreateSnapshotRequest"),
                                   type_named("DeleteDiskRequest"),
                                   type_named("GetDiskRequest"),
                                   type_named("InsertDiskRequest"),
                                   type_named("TestIamPermissionsRequest")));
  EXPECT_THAT(disks_file->import_paths(),
              UnorderedElementsAre(
                  "google/api/annotations.proto", "google/api/client.proto",
                  "google/api/field_behavior.proto",
                  "google/cloud/extended_operations.proto",
                  common_disk_types_file->relative_proto_path(),
                  common_test_permissions_file->relative_proto_path()));

  auto disks_proto_export_file = std::find_if(
      files->second.begin(), files->second.end(), [](auto const& f) {
        return f.relative_file_path() ==
               "google/cloud/product_name/disks/version/disks_proto_export.h";
      });
  EXPECT_THAT(
      disks_proto_export_file->proto_includes(),
      ElementsAre(
          "google/cloud/product_name/version/internal/common_000.proto",
          "google/cloud/product_name/version/internal/common_001.proto"));

  auto foos_file =
      std::find_if(files->first.begin(), files->first.end(), [](auto const& f) {
        return f.relative_proto_path() ==
               "google/cloud/product_name/foos/version/foos.proto";
      });
  EXPECT_THAT(foos_file->package_name(),
              Eq("google.cloud.cpp.product_name.foos.version"));
  EXPECT_THAT(foos_file->types(),
              UnorderedElementsAre(type_named("EmptyResponseMethodRequest"),
                                   type_named("OtherCommonTypeMethodRequest"),
                                   type_named("TestIamPermissionsRequest")));
  EXPECT_THAT(
      foos_file->import_paths(),
      UnorderedElementsAre(
          "google/api/annotations.proto", "google/api/client.proto",
          "google/api/field_behavior.proto", "google/protobuf/empty.proto",
          common_other_schema_file->relative_proto_path(),
          common_test_permissions_file->relative_proto_path()));

  auto foos_proto_export_file = std::find_if(
      files->second.begin(), files->second.end(), [](auto const& f) {
        return f.relative_file_path() ==
               "google/cloud/product_name/foos/version/foos_proto_export.h";
      });
  EXPECT_THAT(
      foos_proto_export_file->proto_includes(),
      ElementsAre(
          "google/cloud/product_name/version/internal/common_001.proto",
          "google/cloud/product_name/version/internal/common_002.proto"));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
