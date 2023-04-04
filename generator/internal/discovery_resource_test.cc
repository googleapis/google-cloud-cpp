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

#include "generator/internal/discovery_resource.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

TEST(DiscoveryResourceTest, FormatUrlPath) {
  EXPECT_THAT(DiscoveryResource::FormatUrlPath("base/path/test"),
              Eq("base/path/test"));
  EXPECT_THAT(DiscoveryResource::FormatUrlPath(
                  "projects/{project}/zones/{zone}/myTests/{foo}/method1"),
              Eq("projects/{project}/zones/{zone}/myTests/{foo}/method1"));
  EXPECT_THAT(DiscoveryResource::FormatUrlPath(
                  "projects/{project}/zones/{zone}/myTests/{fooId}/method1"),
              Eq("projects/{project}/zones/{zone}/myTests/{foo_id}/method1"));
  EXPECT_THAT(
      DiscoveryResource::FormatUrlPath(
          "projects/{project}/zones/{zoneName}/myTests/{fooId}:method1"),
      Eq("projects/{project}/zones/{zone_name}/myTests/{foo_id}:method1"));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsGetRegion) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/regions/{region}/myTests/{foo}",
  "httpMethod": "GET",
  "parameterOrder": [
    "project",
    "region",
    "foo"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      get: "base/path/projects/{project}/regions/{region}/myTests/{foo}"
    };
    option (google.api.method_signature) = "project,region,foo";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsPatchZone) {
  auto constexpr kTypeJson = R"""({
  "request_resource_field_name": "my_request_resource"
})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/zones/{zone}/myTests/{fooId}/method1",
  "httpMethod": "PATCH",
  "response": {
    "$ref": "Operation"
  },
  "parameterOrder": [
    "project",
    "zone",
    "fooId"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      patch: "base/path/projects/{project}/zones/{zone}/myTests/{foo_id}/method1"
      body: "my_request_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_request_resource";
    option (google.cloud.operation_service) = "ZoneOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsPutRegion) {
  auto constexpr kTypeJson = R"""({
  "request_resource_field_name": "my_request_resource"
})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/regions/{region}/myTests/{fooId}/method1",
  "httpMethod": "PUT",
  "response": {
    "$ref": "Operation"
  },
  "parameterOrder": [
    "project",
    "region",
    "fooId"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      put: "base/path/projects/{project}/regions/{region}/myTests/{foo_id}/method1"
      body: "my_request_resource"
    };
    option (google.api.method_signature) = "project,region,foo_id,my_request_resource";
    option (google.cloud.operation_service) = "RegionOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsPostGlobal) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/global/myTests/{foo}:cancel",
  "httpMethod": "POST",
  "parameterOrder": [
    "project",
    "foo"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      post: "base/path/projects/{project}/global/myTests/{foo}:cancel"
      body: "*"
    };
    option (google.api.method_signature) = "project,foo";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsPostGlobalOperationResponse) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/global/myTests/{foo}:cancel",
  "httpMethod": "POST",
  "response": {
    "$ref": "Operation"
  },
  "parameterOrder": [
    "project",
    "foo"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      post: "base/path/projects/{project}/global/myTests/{foo}:cancel"
      body: "*"
    };
    option (google.api.method_signature) = "project,foo";
    option (google.cloud.operation_service) = "GlobalOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsGetNoParams) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "path": "resources/global/list",
  "httpMethod": "GET"
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      get: "base/path/resources/global/list"
    };)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsGetNoParamsOperation) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
    "path": "doFoo",
    "httpMethod": "POST",
    "response": {
      "$ref": "Operation"
    }
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      post: "base/path/doFoo"
      body: "*"
    };
    option (google.cloud.operation_service) = "GlobalOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsMissingPath) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "httpMethod": "GET"
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  EXPECT_THAT(
      options,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Method does not define httpMethod and/or path.")));
}

TEST(DiscoveryResourceTest, FormatRpcOptionsMissingHttpMethod) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "path": "resources/global/list"
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", "base/path", method_json);
  DiscoveryTypeVertex t("myType", type_json);
  auto options = r.FormatRpcOptions(method_json, t);
  EXPECT_THAT(
      options,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Method does not define httpMethod and/or path.")));
}

TEST(DiscoveryResourceTest, FormatOAuthScopesPresent) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "method1": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform"
      ]
    },
    "method2": {
      "scopes": [
        "https://www.googleapis.com/auth/cloud-platform",
        "https://www.googleapis.com/auth/compute"
      ]
    }
  }
})""";
  auto constexpr kExpectedProto =
      R"""(    "https://www.googleapis.com/auth/cloud-platform,"
    "https://www.googleapis.com/auth/compute";
)""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", "", json);
  auto scopes = r.FormatOAuthScopes();
  ASSERT_STATUS_OK(scopes);
  EXPECT_THAT(*scopes, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, FormatOAuthScopesZeroScopes) {
  auto constexpr kResourceJson = R"""({
"methods": {
  "method0": {
    "scopes": [
    ]
  }
}
})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", "", json);
  auto scopes = r.FormatOAuthScopes();
  EXPECT_THAT(
      scopes,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("No OAuth scopes found for service: myTests.")));
}

TEST(DiscoveryResourceTest, FormatOAuthScopesNotPresent) {
  auto constexpr kResourceJson = R"""({
"methods": {
  "methodNone": {
  }
}
})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", "", json);
  auto scopes = r.FormatOAuthScopes();
  EXPECT_THAT(
      scopes,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("No OAuth scopes found for service: myTests.")));
}

TEST(DiscoveryResourceTest, FormatPackageName) {
  auto constexpr kResourceJson = R"""({})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", "", json);
  EXPECT_THAT(r.FormatPackageName("product", "v2"),
              Eq("google.cloud.cpp.product.my_tests.v2"));
}

TEST(DiscoveryResourceTest, FormatFilePath) {
  auto constexpr kResourceJson = R"""({})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", "", json);
  EXPECT_THAT(r.FormatFilePath("product", "v2", "/tmp"),
              Eq("/tmp/google/cloud/product/my_tests/v2/my_tests.proto"));
}

TEST(DiscoveryResourceTest, FormatMethodName) {
  auto constexpr kResourceJson = R"""({})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", "", json);
  EXPECT_THAT(r.FormatMethodName("aggregatedList"),
              Eq("AggregatedListMyTests"));
  EXPECT_THAT(r.FormatMethodName("delete"), Eq("DeleteMyTests"));
  EXPECT_THAT(r.FormatMethodName("get"), Eq("GetMyTests"));
  EXPECT_THAT(r.FormatMethodName("insert"), Eq("InsertMyTests"));
  EXPECT_THAT(r.FormatMethodName("list"), Eq("ListMyTests"));
  EXPECT_THAT(r.FormatMethodName("patch"), Eq("PatchMyTests"));
  EXPECT_THAT(r.FormatMethodName("update"), Eq("UpdateMyTests"));
  EXPECT_THAT(r.FormatMethodName("testPermissions"), Eq("TestPermissions"));
}

TEST(DiscoveryResourceTest, JsonToProtobufService) {
  auto constexpr kGetRequestTypeJson = R"""({})""";
  auto constexpr kDoFooRequestTypeJson = R"""({
  "request_resource_field_name": "my_foo_resource"
})""";
  auto constexpr kResourceJson = R"""({
    "methods": {
      "get": {
        "description": "Description for the get method.",
        "scopes": [
          "https://www.googleapis.com/auth/cloud-platform"
        ],
        "path": "projects/{project}/regions/{region}/myResources/{foo}",
        "httpMethod": "GET",
        "parameterOrder": [
          "project",
          "region",
          "foo"
        ]
      },
      "doFoo": {
        "scopes": [
          "https://www.googleapis.com/auth/cloud-platform"
        ],
        "path": "projects/{project}/zones/{zone}/myResources/{fooId}/doFoo",
        "httpMethod": "POST",
        "response": {
          "$ref": "Operation"
        },
        "parameterOrder": [
          "project",
          "zone",
          "fooId"
        ]
      }
    }
})""";
  auto constexpr kExpectedProto =
      R"""(// Service for the myResources resource.
// https://cloud.google.com/$product_name$/docs/reference/rest/$version$/myResources
service MyResources {
  option (google.api.default_host) = "https://my.endpoint.com";
  option (google.api.oauth_scopes) =
    "https://www.googleapis.com/auth/cloud-platform";

  rpc DoFoo(DoFooRequest) returns (google.cloud.cpp.$product_name$.$version$.Operation) {
    option (google.api.http) = {
      post: "base/path/projects/{project}/zones/{zone}/myResources/{foo_id}/doFoo"
      body: "my_foo_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_foo_resource";
    option (google.cloud.operation_service) = "ZoneOperations";
  }

  // Description for the get method.
  rpc GetMyResources(GetMyResourcesRequest) returns (google.protobuf.Empty) {
    option (google.api.http) = {
      get: "base/path/projects/{project}/regions/{region}/myResources/{foo}"
    };
    option (google.api.method_signature) = "project,region,foo";
  }
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  DiscoveryResource r("myResources", "https://my.endpoint.com", "base/path",
                      resource_json);
  DiscoveryTypeVertex t("GetMyResourcesRequest", get_request_type_json);
  DiscoveryTypeVertex t2("DoFooRequest", do_foo_request_type_json);
  r.AddRequestType("GetMyResourcesRequest", &t);
  r.AddRequestType("DoFooRequest", &t2);
  auto emitted_proto = r.JsonToProtobufService();
  ASSERT_STATUS_OK(emitted_proto);
  EXPECT_THAT(*emitted_proto, Eq(kExpectedProto));
}

TEST(DiscoveryResourceTest, JsonToProtobufServiceMissingOAuthScopes) {
  auto constexpr kGetRequestTypeJson = R"""({})""";
  auto constexpr kResourceJson = R"""({
    "methods": {
      "get": {
        "path": "projects/{project}/regions/{region}/myResources/{foo}",
        "httpMethod": "GET",
        "parameterOrder": [
          "project",
          "region",
          "foo"
        ]
      }
    }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryResource r("myResources", "https://my.endpoint.com", "base/path",
                      resource_json);
  DiscoveryTypeVertex t("GetMyResourcesRequest", get_request_type_json);
  r.AddRequestType("GetMyResourcesRequest", &t);
  auto emitted_proto = r.JsonToProtobufService();
  EXPECT_THAT(
      emitted_proto,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("No OAuth scopes found for service: myResources.")));
}

TEST(DiscoveryResourceTest, JsonToProtobufServiceMissingRequestType) {
  auto constexpr kResourceJson = R"""({
    "methods": {
      "doFoo": {
        "scopes": [
          "https://www.googleapis.com/auth/cloud-platform"
        ],
        "path": "projects/{project}/zones/{zone}/myResources/{fooId}/doFoo",
        "httpMethod": "POST",
        "response": {
          "$ref": "Operation"
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
  DiscoveryResource r("myResources", "https://my.endpoint.com", "base/path",
                      resource_json);
  auto emitted_proto = r.JsonToProtobufService();
  EXPECT_THAT(
      emitted_proto,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Cannot find request_type_key=DoFooRequest in type_map")));
}

TEST(DiscoveryResourceTest, JsonToProtobufServiceErrorFormattingRpcOptions) {
  auto constexpr kGetRequestTypeJson = R"""({})""";
  auto constexpr kResourceJson = R"""({
    "methods": {
      "get": {
        "scopes": [
          "https://www.googleapis.com/auth/cloud-platform"
        ],
        "path": "projects/{project}/regions/{region}/myResources/{foo}",
        "parameterOrder": [
          "project",
          "region",
          "foo"
        ]
      }
    }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryResource r("myResources", "https://my.endpoint.com", "base/path",
                      resource_json);
  DiscoveryTypeVertex t("GetMyResourcesRequest", get_request_type_json);
  r.AddRequestType("GetMyResourcesRequest", &t);
  auto emitted_proto = r.JsonToProtobufService();
  EXPECT_THAT(
      emitted_proto,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Method does not define httpMethod and/or path.")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
