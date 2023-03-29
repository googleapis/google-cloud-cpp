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
  "path": "projects/{project}/zones/{zone}/myTests/{foo}/method1",
  "httpMethod": "PATCH",
  "response": {
    "$ref": "Operation"
  },
  "parameterOrder": [
    "project",
    "zone",
    "foo"
  ]
})""";
  auto constexpr kExpectedProto =
      R"""(    option (google.api.http) = {
      patch: "base/path/projects/{project}/zones/{zone}/myTests/{foo}/method1"
      body: "my_request_resource"
    };
    option (google.api.method_signature) = "project,zone,foo,my_request_resource";
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

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
