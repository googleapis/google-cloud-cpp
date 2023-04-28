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
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Key;
using ::testing::SizeIs;
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
  auto types = ExtractTypesFromSchema({}, parsed_json);
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
  auto types = ExtractTypesFromSchema({}, parsed_json);
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
  auto types = ExtractTypesFromSchema({}, parsed_json);
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
  auto types = ExtractTypesFromSchema({}, parsed_json);
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
  auto types = ExtractTypesFromSchema({}, parsed_json);
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
  auto types = ExtractTypesFromSchema({}, parsed_json);
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
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "", response_type_json});
  auto response = DetermineAndVerifyResponseType(method_json, resource, types);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT((*response)->name(), Eq("Foo"));
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
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "", response_type_json});
  auto response = DetermineAndVerifyResponseType(method_json, resource, types);
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
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "", response_type_json});
  auto response = DetermineAndVerifyResponseType(method_json, resource, types);
  EXPECT_THAT(response, IsOkAndHolds(IsNull()));
}

TEST(SynthesizeRequestTypeTest, OperationResponseWithRefRequestField) {
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
  auto result =
      SynthesizeRequestType(method_json, resource, "Operation", "create");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->json(), Eq(expected_request_type_json));
}

TEST(SynthesizeRequestTypeTest,
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
    "description":"The FooResource for this request."
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
  auto result =
      SynthesizeRequestType(method_json, resource, "Operation", "create");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->json(), Eq(expected_request_type_json));
}

TEST(SynthesizeRequestTypeTest, NonOperationWithoutRequestField) {
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
  auto result = SynthesizeRequestType(method_json, resource, "Foo", "get");
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(result->json(), Eq(expected_request_type_json));
}

TEST(SynthesizeRequestTypeTest, MethodJsonMissingParameters) {
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
  auto result =
      SynthesizeRequestType(method_json, resource, "Operation", "create");
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInternal,
               HasSubstr("method_json does not contain parameters field")));
  EXPECT_THAT(result.status().error_info().metadata(),
              AllOf(Contains(Key("resource")), Contains(Key("method")),
                    Contains(Key("json"))));
}

TEST(SynthesizeRequestTypeTest, OperationResponseMissingRefInRequest) {
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
  auto result =
      SynthesizeRequestType(method_json, resource, "Operation", "create");
  EXPECT_THAT(
      result,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("resource foos has method Create with non $ref request")));
}

TEST(ProcessMethodRequestsAndResponsesTest, RequestWithOperationResponse) {
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
  types.emplace("Operation",
                DiscoveryTypeVertex("Operation", "", operation_type_json));
  auto result = ProcessMethodRequestsAndResponses(resources, types);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(
      types, UnorderedElementsAre(Key("Foos.CreateRequest"), Key("Operation")));
  EXPECT_THAT(resources.begin()->second.response_types(),
              UnorderedElementsAre(Key("Operation")));
  EXPECT_TRUE(resources.begin()->second.RequiresLROImport());
}

TEST(ProcessMethodRequestsAndResponsesTest, MethodWithEmptyRequest) {
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
  auto result = ProcessMethodRequestsAndResponses(resources, types);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(types, IsEmpty());
  EXPECT_TRUE(resources.begin()->second.RequiresEmptyImport());
}

TEST(ProcessMethodRequestsAndResponsesTest, MethodWithEmptyResponse) {
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
  auto result = ProcessMethodRequestsAndResponses(resources, types);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(types, Not(IsEmpty()));
  EXPECT_TRUE(resources.begin()->second.RequiresEmptyImport());
}

TEST(ProcessMethodRequestsAndResponsesTest, ResponseError) {
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
  types.emplace("Operation", DiscoveryTypeVertex("", "", operation_type_json));
  auto result = ProcessMethodRequestsAndResponses(resources, types);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("Missing $ref field in response")));
  EXPECT_THAT(result.error_info().metadata(),
              AllOf(Contains(Key("resource")), Contains(Key("method"))));
}

TEST(ProcessMethodRequestsAndResponsesTest, RequestError) {
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
  types.emplace("Operation", DiscoveryTypeVertex("", "", operation_type_json));
  auto result = ProcessMethodRequestsAndResponses(resources, types);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("with non $ref request")));
}

TEST(ProcessMethodRequestsAndResponsesTest, TypeInsertError) {
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
  types.emplace("Operation",
                DiscoveryTypeVertex("Operation", "", operation_type_json));
  types.emplace("Foos.CreateRequest",
                DiscoveryTypeVertex("", "", empty_type_json));
  auto result = ProcessMethodRequestsAndResponses(resources, types);
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
  DiscoveryDocumentProperties props{"", "", "product_name", "version"};
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
  DiscoveryDocumentProperties props{"", "", "product_name", "version"};
  auto result = CreateFilesFromResources(resources, props, "tmp");
  EXPECT_THAT(result, IsEmpty());
}

TEST(AssignResourcesAndTypesToFilesTest,
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
      DiscoveryTypeVertex("CreateRequest", "", synthesized_type_json));
  auto constexpr kOperationTypeJson = R"""({
})""";
  auto const operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  types.emplace("Operation",
                DiscoveryTypeVertex("Operation", "", operation_type_json));
  DiscoveryDocumentProperties props{"", "", "product_name", "version"};
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
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "");
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
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "");
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
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "");
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
  auto result = GenerateProtosFromDiscoveryDoc(document_json, "", "", "");
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Response name=baz not found in types")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
