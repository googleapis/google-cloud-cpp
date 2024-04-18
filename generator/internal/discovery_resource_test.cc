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
#include "generator/testing/descriptor_pool_fixture.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

class DiscoveryResourceTest : public generator_testing::DescriptorPoolFixture {
};

TEST_F(DiscoveryResourceTest, HasEmptyRequest) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  EXPECT_FALSE(r.RequiresEmptyImport());
  r.AddEmptyRequestType();
  EXPECT_TRUE(r.RequiresEmptyImport());
}

TEST_F(DiscoveryResourceTest, HasEmptyResponse) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  EXPECT_FALSE(r.RequiresEmptyImport());
  r.AddEmptyResponseType();
  EXPECT_TRUE(r.RequiresEmptyImport());
}

TEST_F(DiscoveryResourceTest, RequiresLROImport) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  r.AddResponseType("Operation", nullptr);
  EXPECT_TRUE(r.RequiresLROImport());
}

TEST_F(DiscoveryResourceTest, GetServiceApiVersionEmpty) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "emptyResponseMethod1": {
    },
    "emptyResponseMethod2": {
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  EXPECT_THAT(r.GetServiceApiVersion(), IsOkAndHolds(IsEmpty()));
}

TEST_F(DiscoveryResourceTest, GetServiceApiVersionSameVersion) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "emptyResponseMethod1": {
      "apiVersion": "test-api-version"
    },
    "emptyResponseMethod2": {
      "apiVersion": "test-api-version"
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  EXPECT_THAT(r.GetServiceApiVersion(), IsOkAndHolds("test-api-version"));
}

TEST_F(DiscoveryResourceTest, GetServiceApiVersionDifferentVersion) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "emptyResponseMethod1": {
      "apiVersion": "test-api-version"
    },
    "emptyResponseMethod2": {
      "apiVersion": "other-test-api-version"
    }
  }
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  auto service_api_version = r.SetServiceApiVersion();
  EXPECT_THAT(
      service_api_version,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "resource contains methods with different apiVersion values")));
}

TEST_F(DiscoveryResourceTest, GetServiceApiVersionNoMethods) {
  auto constexpr kResourceJson = R"""({})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myTests", "", resource_json);
  auto service_api_version = r.SetServiceApiVersion();
  EXPECT_THAT(service_api_version,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("resource contains no methods")));
}

TEST_F(DiscoveryResourceTest, FormatUrlPath) {
  EXPECT_THAT(DiscoveryResource::FormatUrlPath("base/path/test"),
              Eq("base/path/test"));
  EXPECT_THAT(DiscoveryResource::FormatUrlPath(
                  "projects/{project}/zones/{zone}/myTests/{foo}/method1"),
              Eq("projects/{project}/zones/{zone}/myTests/"
                 "{foo}/method1"));
  EXPECT_THAT(DiscoveryResource::FormatUrlPath(
                  "projects/{project}/zones/{zone}/myTests/{fooId}/method1"),
              Eq("projects/{project}/zones/{zone}/myTests/"
                 "{foo_id}/method1"));
  EXPECT_THAT(
      DiscoveryResource::FormatUrlPath(
          "projects/{project}/zones/{zoneName}/myTests/{fooId}:method1"),
      Eq("projects/{project}/zones/{zone_name}/myTests/"
         "{foo_id}:method1"));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsGetRegion) {
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
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPatchZoneNoUpdateMaskParam) {
  auto constexpr kTypeJson = R"""({
  "request_resource_field_name": "my_request_resource"
})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/zones/{zone}/myTests/{fooId}/method1",
  "httpMethod": "PATCH",
  "response": {
    "$ref": "Operation"
  },
  "parameters":  {
    "project": {},
    "zone": {},
    "fooId": {}
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
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPatchZoneUpdateMaskParam) {
  auto constexpr kTypeJson = R"""({
  "request_resource_field_name": "my_request_resource"
})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/zones/{zone}/myTests/{fooId}/method1",
  "httpMethod": "PATCH",
  "response": {
    "$ref": "Operation"
  },
  "parameters":  {
    "project": {},
    "zone": {},
    "fooId": {},
    "updateMask": {}
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
    option (google.api.method_signature) = "project,zone,foo_id,update_mask,my_request_resource";
    option (google.cloud.operation_service) = "ZoneOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest,
       FormatRpcOptionsPatchZoneUpdateMaskParamNotPatchMethod) {
  auto constexpr kTypeJson = R"""({
  "request_resource_field_name": "my_request_resource"
})""";
  auto constexpr kMethodJson = R"""({
  "path": "projects/{project}/zones/{zone}/myTests/{fooId}/method1",
  "httpMethod": "POST",
  "response": {
    "$ref": "Operation"
  },
  "parameters":  {
    "project": {},
    "zone": {},
    "fooId": {},
    "updateMask": {}
  },
  "parameterOrder": [
    "project",
    "zone",
    "fooId"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      post: "base/path/projects/{project}/zones/{zone}/myTests/{foo_id}/method1"
      body: "my_request_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_request_resource";
    option (google.cloud.operation_service) = "ZoneOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPutRegion) {
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
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPostGlobal) {
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
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPostGlobalOperationResponse) {
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
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsGetNoParams) {
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
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsGetNoParamsOperation) {
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
    option (google.cloud.operation_service) = "GlobalOrganizationOperations";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsMissingPath) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "httpMethod": "GET"
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  EXPECT_THAT(
      options,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Method does not define httpMethod and/or path.")));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsMissingHttpMethod) {
  auto constexpr kTypeJson = R"""({})""";
  auto constexpr kMethodJson = R"""({
  "path": "resources/global/list"
})""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("myTests", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options = r.FormatRpcOptions(method_json, "base/path", {}, &t);
  EXPECT_THAT(
      options,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Method does not define httpMethod and/or path.")));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPutRegionOperationService) {
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
    option (google.api.method_signature) = "project,region,foo_id,my_request_resource";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("regionOperations", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options =
      r.FormatRpcOptions(method_json, "base/path", {"RegionOperations"}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatRpcOptionsPostGlobalOperationService) {
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
    option (google.api.method_signature) = "project,foo";)""";
  auto method_json = nlohmann::json::parse(kMethodJson, nullptr, false);
  ASSERT_TRUE(method_json.is_object());
  auto type_json = nlohmann::json::parse(kTypeJson, nullptr, false);
  ASSERT_TRUE(type_json.is_object());
  DiscoveryResource r("GlobalOperations", "", method_json);
  DiscoveryTypeVertex t("myType", "", type_json, &pool());
  auto options =
      r.FormatRpcOptions(method_json, "base/path", {"GlobalOperations"}, &t);
  ASSERT_STATUS_OK(options);
  EXPECT_THAT(*options, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatOAuthScopesPresent) {
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
  DiscoveryResource r("myTests", "", json);
  auto scopes = r.FormatOAuthScopes();
  ASSERT_STATUS_OK(scopes);
  EXPECT_THAT(*scopes, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, FormatOAuthScopesZeroScopes) {
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
  DiscoveryResource r("myTests", "", json);
  auto scopes = r.FormatOAuthScopes();
  EXPECT_THAT(
      scopes,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("No OAuth scopes found for service: myTests.")));
}

TEST_F(DiscoveryResourceTest, FormatOAuthScopesNotPresent) {
  auto constexpr kResourceJson = R"""({
"methods": {
  "methodNone": {
  }
}
})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", json);
  auto scopes = r.FormatOAuthScopes();
  EXPECT_THAT(
      scopes,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("No OAuth scopes found for service: myTests.")));
}

TEST_F(DiscoveryResourceTest, FormatFilePath) {
  auto constexpr kResourceJson = R"""({})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", json);
  EXPECT_THAT(r.FormatFilePath("product", "v2", "/tmp"),
              Eq("/tmp/google/cloud/product/my_tests/v2/my_tests.proto"));
}

TEST_F(DiscoveryResourceTest, FormatFilePathEmptyOutputPath) {
  auto constexpr kResourceJson = R"""({})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("myTests", "", json);
  EXPECT_THAT(r.FormatFilePath("product", "v2", {}),
              Eq("google/cloud/product/my_tests/v2/my_tests.proto"));
}

TEST_F(DiscoveryResourceTest, FormatMethodName) {
  auto constexpr kResourceJson = R"""({
  "methods": {
    "get": {
      "response": {
        "$ref": "Address"
      }
    }
  }
})""";
  auto json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(json.is_object());
  DiscoveryResource r("addresses", "", json);
  EXPECT_THAT(r.FormatMethodName("aggregatedList"),
              Eq("AggregatedListAddresses"));
  EXPECT_THAT(r.FormatMethodName("delete"), Eq("DeleteAddress"));
  EXPECT_THAT(r.FormatMethodName("get"), Eq("GetAddress"));
  EXPECT_THAT(r.FormatMethodName("insert"), Eq("InsertAddress"));
  EXPECT_THAT(r.FormatMethodName("list"), Eq("ListAddresses"));
  EXPECT_THAT(r.FormatMethodName("patch"), Eq("PatchAddress"));
  EXPECT_THAT(r.FormatMethodName("update"), Eq("UpdateAddress"));
  EXPECT_THAT(r.FormatMethodName("testPermissions"), Eq("TestPermissions"));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufService) {
  auto constexpr kOperationTypeJson = R"""({})""";
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
        "apiVersion": "test-api-version",
        "path": "projects/{project}/regions/{region}/myResources/{foo}",
        "httpMethod": "GET",
        "parameters": {
          "project": {
            "type": "string"
          }
        },
        "response": {
          "$ref": "MyResource"
        },
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
        "apiVersion": "test-api-version",
        "path": "projects/{project}/zones/{zone}/myResources/{fooId}/doFoo",
        "httpMethod": "POST",
        "parameters": {
          "project": {
            "type": "string"
          }
        },
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
  option (google.api.api_version) = "test-api-version";
  option (google.api.oauth_scopes) =
    "https://www.googleapis.com/auth/cloud-platform";

  // https://cloud.google.com/$product_name$/docs/reference/rest/$version$/myResources/doFoo
  rpc DoFoo(DoFooRequest) returns (other.package.Operation) {
    option (google.api.http) = {
      post: "base/path/projects/{project}/zones/{zone}/myResources/{foo_id}/doFoo"
      body: "my_foo_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_foo_resource";
    option (google.cloud.operation_service) = "ZoneOperations";
  }

  // Description for the get method.
  // https://cloud.google.com/$product_name$/docs/reference/rest/$version$/myResources/get
  rpc GetMyResource(GetMyResourceRequest) returns (MyResource) {
    option (google.api.http) = {
      get: "base/path/projects/{project}/regions/{region}/myResources/{foo}"
    };
    option (google.api.method_signature) = "project,region,foo";
  }
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryTypeVertex t("GetMyResourceRequest", "this.package",
                        get_request_type_json, &pool());
  DiscoveryTypeVertex t2("DoFooRequest", "this.package",
                         do_foo_request_type_json, &pool());
  DiscoveryTypeVertex response("MyResource", "this.package", {}, &pool());
  r.AddRequestType("GetMyResourceRequest", &t);
  r.AddRequestType("DoFooRequest", &t2);
  r.AddResponseType("MyResource", &response);
  DiscoveryTypeVertex t3("Operation", "other.package", operation_type_json,
                         &pool());
  r.AddResponseType("Operation", &t3);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  auto emitted_proto = r.JsonToProtobufService(document_properties);
  EXPECT_THAT(emitted_proto, IsOkAndHolds(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufServiceNoApiVersion) {
  auto constexpr kOperationTypeJson = R"""({})""";
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
        "parameters": {
          "project": {
            "type": "string"
          }
        },
        "response": {
          "$ref": "MyResource"
        },
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
        "parameters": {
          "project": {
            "type": "string"
          }
        },
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

  // https://cloud.google.com/$product_name$/docs/reference/rest/$version$/myResources/doFoo
  rpc DoFoo(DoFooRequest) returns (other.package.Operation) {
    option (google.api.http) = {
      post: "base/path/projects/{project}/zones/{zone}/myResources/{foo_id}/doFoo"
      body: "my_foo_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_foo_resource";
    option (google.cloud.operation_service) = "ZoneOperations";
  }

  // Description for the get method.
  // https://cloud.google.com/$product_name$/docs/reference/rest/$version$/myResources/get
  rpc GetMyResource(GetMyResourceRequest) returns (MyResource) {
    option (google.api.http) = {
      get: "base/path/projects/{project}/regions/{region}/myResources/{foo}"
    };
    option (google.api.method_signature) = "project,region,foo";
  }
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryTypeVertex t("GetMyResourceRequest", "this.package",
                        get_request_type_json, &pool());
  DiscoveryTypeVertex t2("DoFooRequest", "this.package",
                         do_foo_request_type_json, &pool());
  DiscoveryTypeVertex response("MyResource", "this.package", {}, &pool());
  r.AddRequestType("GetMyResourceRequest", &t);
  r.AddRequestType("DoFooRequest", &t2);
  r.AddResponseType("MyResource", &response);
  DiscoveryTypeVertex t3("Operation", "other.package", operation_type_json,
                         &pool());
  r.AddResponseType("Operation", &t3);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  auto emitted_proto = r.JsonToProtobufService(document_properties);
  ASSERT_STATUS_OK(emitted_proto);
  EXPECT_THAT(*emitted_proto, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufServiceMissingOAuthScopes) {
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
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryTypeVertex t("GetMyResourcesRequest", "this.package",
                        get_request_type_json, &pool());
  r.AddRequestType("GetMyResourcesRequest", &t);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  auto emitted_proto = r.JsonToProtobufService(document_properties);
  EXPECT_THAT(
      emitted_proto,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("No OAuth scopes found for service: myResources.")));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufServiceMissingRequestType) {
  auto constexpr kResourceJson = R"""({
    "methods": {
      "doFoo": {
        "scopes": [
          "https://www.googleapis.com/auth/cloud-platform"
        ],
        "path": "projects/{project}/zones/{zone}/myResources/{fooId}/doFoo",
        "httpMethod": "POST",
        "parameters": {
          "project": {
            "type": "string"
          }
        },
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
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  auto emitted_proto = r.JsonToProtobufService(document_properties);
  EXPECT_THAT(
      emitted_proto,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Cannot find request_type_name=DoFooRequest in type_map")));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufServiceEmptyRequestType) {
  auto constexpr kResourceJson = R"""({
    "methods": {
      "noop": {
        "scopes": [
          "https://www.googleapis.com/auth/cloud-platform"
        ],
        "path": "noop",
        "httpMethod": "POST"
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

  // https://cloud.google.com/$product_name$/docs/reference/rest/$version$/myResources/noop
  rpc Noop(google.protobuf.Empty) returns (google.protobuf.Empty) {
    option (google.api.http) = {
      post: "base/path/noop"
    };
  }
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  auto emitted_proto = r.JsonToProtobufService(document_properties);
  ASSERT_STATUS_OK(emitted_proto);
  EXPECT_THAT(*emitted_proto, Eq(kExpectedProto));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufServiceErrorFormattingRpcOptions) {
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
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryTypeVertex t("GetMyResourcesRequest", "this.package",
                        get_request_type_json, &pool());
  r.AddRequestType("GetMyResourcesRequest", &t);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.SetServiceApiVersion(), IsOk());
  auto emitted_proto = r.JsonToProtobufService(document_properties);
  EXPECT_THAT(
      emitted_proto,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Method does not define httpMethod and/or path.")));
}

TEST_F(DiscoveryResourceTest, JsonToProtobufServiceCalledWithoutApiVersionSet) {
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
  DiscoveryResource r("myResources", "this.package", resource_json);
  DiscoveryTypeVertex t("GetMyResourcesRequest", "this.package",
                        get_request_type_json, &pool());
  r.AddRequestType("GetMyResourcesRequest", &t);
  DiscoveryDocumentProperties document_properties{
      "base/path", "https://my.endpoint.com", "", "", "", "", {}, "2023"};
  EXPECT_THAT(r.JsonToProtobufService(document_properties),
              StatusIs(StatusCode::kInternal,
                       HasSubstr("SetServiceApiVersion must be called before "
                                 "JsonToProtobufService is called")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
