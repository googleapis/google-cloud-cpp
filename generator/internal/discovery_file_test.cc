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

#include "generator/internal/discovery_file.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

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
          "type": "string",
          "description": "Description for project."
        },
        "region": {
          "type": "string",
          "description": "Description for region."
        },
        "foo": {
          "type": "string",
          "description": "Description for foo."
        }
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
          "type": "string",
          "description": "Description for project."
        },
        "zone": {
          "type": "string",
          "description": "Description for zone."
        },
        "fooId": {
          "type": "string",
          "description": "Description for fooId."
        },
        "my_foo_resource": {
          "$ref": "Foo"
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

auto constexpr kDoFooRequestTypeJson = R"""({
  "type": "object",
  "id": "DoFooRequest",
  "properties": {
    "project": {
      "type": "string",
      "description": "Description for project."
    },
    "zone": {
      "type": "string",
      "description": "Description for zone."
    },
    "fooId": {
      "type": "string",
      "description": "Description for fooId."
    },
    "my_foo_resource": {
      "$ref": "Foo",
      "is_resource": true
    }
  },
  "request_resource_field_name": "my_foo_resource"
})""";

auto constexpr kGetRequestTypeJson = R"""({
  "type": "object",
  "id": "GetMyResourcesRequest",
  "properties": {
    "project": {
      "type": "string",
      "description": "Description for project."
    },
    "region": {
      "type": "string",
      "description": "Description for region."
    },
    "foo": {
      "type": "string",
      "description": "Description for foo."
    }
  }
})""";

auto constexpr kOperationTypeJson = R"""({})""";

TEST(DiscoveryFile, FormatFileWithImport) {
  auto constexpr kExpectedProto = R"""(// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

syntax = "proto3";

package my.package.name;

import "path/to/import.proto";

// Service for the myResources resource.
// https://cloud.google.com/my_product/docs/reference/rest/v1/myResources
service MyResources {
  option (google.api.default_host) = "https://default.host";
  option (google.api.oauth_scopes) =
    "https://www.googleapis.com/auth/cloud-platform";

  rpc DoFoo(DoFooRequest) returns (other.package.Operation) {
    option (google.api.http) = {
      post: "my/service/projects/{project=project}/zones/{zone=zone}/myResources/{foo_id=foo_id}/doFoo"
      body: "my_foo_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_foo_resource";
    option (google.cloud.operation_service) = "ZoneOperations";
  }

  // Description for the get method.
  rpc GetMyResources(GetMyResourcesRequest) returns (google.protobuf.Empty) {
    option (google.api.http) = {
      get: "my/service/projects/{project=project}/regions/{region=region}/myResources/{foo=foo}"
    };
    option (google.api.method_signature) = "project,region,foo";
  }
}

message DoFooRequest {
  // Description for fooId.
  optional string foo_id = 1;

  optional Foo my_foo_resource = 2 [json_name="__json_request_body"];

  // Description for project.
  optional string project = 3;

  // Description for zone.
  optional string zone = 4;
}

message GetMyResourcesRequest {
  // Description for foo.
  optional string foo = 1;

  // Description for project.
  optional string project = 2;

  // Description for region.
  optional string region = 3;
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryResource r("myResources", "my.package.name", resource_json);
  DiscoveryTypeVertex do_foo_request_type("DoFooRequest", "my.package.name",
                                          do_foo_request_type_json);
  DiscoveryTypeVertex get_request_type(
      "GetMyResourcesRequest", "my.package.name", get_request_type_json);
  r.AddRequestType("DoFooRequest", &do_foo_request_type);
  r.AddRequestType("GetMyResourcesRequest", &get_request_type);
  DiscoveryTypeVertex operation_type("Operation", "other.package",
                                     operation_type_json);
  r.AddResponseType("Operation", &operation_type);
  DiscoveryFile f(&r, "my_path", "my.package.name", r.GetRequestTypesList());
  f.AddImportPath("path/to/import.proto");
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "my.package.name", {}});
  std::stringstream os;
  DiscoveryDocumentProperties document_properties{
      "my/service", "https://default.host", "my_product", "v1"};
  auto result = f.FormatFile(document_properties, types, os);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(os.str(), Eq(kExpectedProto));
}

TEST(DiscoveryFile, FormatFileWithoutImports) {
  auto constexpr kExpectedProto = R"""(// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

syntax = "proto3";

package my.package.name;

// Service for the myResources resource.
// https://cloud.google.com/my_product/docs/reference/rest/v1/myResources
service MyResources {
  option (google.api.default_host) = "https://default.host";
  option (google.api.oauth_scopes) =
    "https://www.googleapis.com/auth/cloud-platform";

  rpc DoFoo(DoFooRequest) returns (other.package.Operation) {
    option (google.api.http) = {
      post: "my/service/projects/{project=project}/zones/{zone=zone}/myResources/{foo_id=foo_id}/doFoo"
      body: "my_foo_resource"
    };
    option (google.api.method_signature) = "project,zone,foo_id,my_foo_resource";
    option (google.cloud.operation_service) = "ZoneOperations";
  }

  // Description for the get method.
  rpc GetMyResources(GetMyResourcesRequest) returns (google.protobuf.Empty) {
    option (google.api.http) = {
      get: "my/service/projects/{project=project}/regions/{region=region}/myResources/{foo=foo}"
    };
    option (google.api.method_signature) = "project,region,foo";
  }
}

message DoFooRequest {
  // Description for fooId.
  optional string foo_id = 1;

  optional Foo my_foo_resource = 2 [json_name="__json_request_body"];

  // Description for project.
  optional string project = 3;

  // Description for zone.
  optional string zone = 4;
}

message GetMyResourcesRequest {
  // Description for foo.
  optional string foo = 1;

  // Description for project.
  optional string project = 2;

  // Description for region.
  optional string region = 3;
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryResource r("myResources", "my.package.name", resource_json);
  DiscoveryTypeVertex do_foo_request_type("DoFooRequest", "my.package.name",
                                          do_foo_request_type_json);
  DiscoveryTypeVertex get_request_type(
      "GetMyResourcesRequest", "my.package.name", get_request_type_json);
  r.AddRequestType("DoFooRequest", &do_foo_request_type);
  r.AddRequestType("GetMyResourcesRequest", &get_request_type);
  DiscoveryTypeVertex operation_type("Operation", "other.package",
                                     operation_type_json);
  r.AddResponseType("Operation", &operation_type);
  DiscoveryFile f(&r, "my_path", "my.package.name", r.GetRequestTypesList());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "my.package.name", {}});
  std::stringstream os;
  DiscoveryDocumentProperties document_properties{
      "my/service", "https://default.host", "my_product", "v1"};
  auto result = f.FormatFile(document_properties, types, os);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(os.str(), Eq(kExpectedProto));
}

TEST(DiscoveryFile, FormatFileNoResource) {
  auto constexpr kExpectedProto = R"""(// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

syntax = "proto3";

package my.package.name;

message DoFooRequest {
  // Description for fooId.
  optional string foo_id = 1;

  optional Foo my_foo_resource = 2 [json_name="__json_request_body"];

  // Description for project.
  optional string project = 3;

  // Description for zone.
  optional string zone = 4;
}

message GetMyResourcesRequest {
  // Description for foo.
  optional string foo = 1;

  // Description for project.
  optional string project = 2;

  // Description for region.
  optional string region = 3;
}
)""";
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryTypeVertex do_foo_request_type("DoFooRequest", "my.package.name",
                                          do_foo_request_type_json);
  DiscoveryTypeVertex get_request_type("GetMyResourcesRequest", "",
                                       get_request_type_json);
  DiscoveryFile f(nullptr, "my_path", "my.package.name",
                  {&do_foo_request_type, &get_request_type});
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "my.package.name", {}});
  std::stringstream os;
  DiscoveryDocumentProperties document_properties{"", "", "my_product", "v1"};
  auto result = f.FormatFile(document_properties, types, os);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(os.str(), Eq(kExpectedProto));
}

TEST(DiscoveryFile, FormatFileNoTypes) {
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

  auto constexpr kExpectedProto = R"""(// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

syntax = "proto3";

package my.package.name;

// Service for the myResources resource.
// https://cloud.google.com/my_product/docs/reference/rest/v1/myResources
service MyResources {
  option (google.api.default_host) = "https://default.host";
  option (google.api.oauth_scopes) =
    "https://www.googleapis.com/auth/cloud-platform";

  rpc Noop(google.protobuf.Empty) returns (google.protobuf.Empty) {
    option (google.api.http) = {
      post: "my/service/noop"
    };
  }
}
)""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  DiscoveryResource r("myResources", "", resource_json);
  DiscoveryFile f(&r, "my_path", "my.package.name", r.GetRequestTypesList());
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "my.package.name", {}});
  std::stringstream os;
  DiscoveryDocumentProperties document_properties{
      "my/service", "https://default.host", "my_product", "v1"};
  auto result = f.FormatFile(document_properties, types, os);
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(os.str(), Eq(kExpectedProto));
}

TEST(DiscoveryFile, FormatFileResourceScopeError) {
  auto constexpr kScopeMissingResourceJson = R"""({
    "methods": {
      "get": {
        "description": "Description for the get method.",
        "path": "projects/{project}/regions/{region}/myResources/{foo}",
        "httpMethod": "GET",
        "parameterOrder": [
          "project",
          "region",
          "foo"
        ]
      },
      "doFoo": {
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

  auto resource_json =
      nlohmann::json::parse(kScopeMissingResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryResource r("myResources", "", resource_json);
  DiscoveryTypeVertex do_foo_request_type("DoFooRequest", "",
                                          do_foo_request_type_json);
  DiscoveryTypeVertex get_request_type("GetMyResourcesRequest", "",
                                       get_request_type_json);
  r.AddRequestType("DoFooRequest", &do_foo_request_type);
  r.AddRequestType("GetMyResourcesRequest", &get_request_type);
  DiscoveryFile f(&r, "my_path", "my.package.name", r.GetRequestTypesList());
  f.AddImportPath("path/to/import.proto");
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "my.package.name", {}});
  std::stringstream os;
  DiscoveryDocumentProperties document_properties{"", "", "my_product", "v1"};
  auto result = f.FormatFile(document_properties, types, os);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("scope")));
}

TEST(DiscoveryFile, FormatFileTypeMissingError) {
  auto constexpr kDoFooRequestMissingTypeJson = R"""({
  "type": "object",
  "id": "DoFooRequest",
  "properties": {
    "project": {
      "type": "string",
      "description": "Description for project."
    },
    "zone": {
      "type": "string",
      "description": "Description for zone."
    },
    "fooId": {
      "type": "string",
      "description": "Description for fooId."
    },
    "my_foo_resource": {
    }
  },
  "request_resource_field_name": "my_foo_resource"
})""";
  auto resource_json = nlohmann::json::parse(kResourceJson, nullptr, false);
  ASSERT_TRUE(resource_json.is_object());
  auto operation_type_json =
      nlohmann::json::parse(kOperationTypeJson, nullptr, false);
  ASSERT_TRUE(operation_type_json.is_object());
  auto do_foo_request_type_json =
      nlohmann::json::parse(kDoFooRequestMissingTypeJson, nullptr, false);
  ASSERT_TRUE(do_foo_request_type_json.is_object());
  auto get_request_type_json =
      nlohmann::json::parse(kGetRequestTypeJson, nullptr, false);
  ASSERT_TRUE(get_request_type_json.is_object());
  DiscoveryResource r("myResources", "", resource_json);
  DiscoveryTypeVertex do_foo_request_type("DoFooRequest", "",
                                          do_foo_request_type_json);
  DiscoveryTypeVertex get_request_type("GetMyResourcesRequest", "",
                                       get_request_type_json);
  r.AddRequestType("DoFooRequest", &do_foo_request_type);
  r.AddRequestType("GetMyResourcesRequest", &get_request_type);
  DiscoveryTypeVertex operation_type("Operation", "other.package",
                                     operation_type_json);
  r.AddResponseType("Operation", &operation_type);

  DiscoveryFile f(&r, "my_path", "my.package.name", r.GetRequestTypesList());
  f.AddImportPath("path/to/import.proto");
  std::map<std::string, DiscoveryTypeVertex> types;
  types.emplace("Foo", DiscoveryTypeVertex{"Foo", "my.package.name", {}});
  std::stringstream os;
  DiscoveryDocumentProperties document_properties{"", "", "my_product", "v1"};
  auto result = f.FormatFile(document_properties, types, os);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("neither $ref nor type")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
