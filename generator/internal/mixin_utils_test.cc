// Copyright 2024 Google LLC
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

#include "generator/internal/mixin_utils.h"
#include "generator/testing/descriptor_pool_fixture.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/api/annotations.pb.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::ServiceDescriptor;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NotNull;

MATCHER_P(HasMethod, expected, "has method full name") {
  return arg.get().full_name() == expected;
}

auto constexpr kServiceConfigYaml = R"""(
apis:
  - name: test.v1.Service
  - name: google.cloud.location.Locations
  - name: google.iam.v1.IAMPolicy
  - name: google.longrunning.Operations
http:
  rules:
  - selector: google.cloud.location.Locations.GetLocation
    get: 'OverwriteGetLocationPath'
  - selector: google.cloud.location.Locations.ListLocations
    get: 'OverwriteListLocationPath'
  - selector: google.iam.v1.IAMPolicy.SetIamPolicy
    post: 'OverwriteSetIamPolicyPath'
    body: '*'
    additional_bindings:
    - post: 'OverwriteSetIamPolicyPath0'
      body: '*'
    - get: 'OverwriteSetIamPolicyPath1'
)""";

auto constexpr kServiceConfigRedundantRulesYaml = R"""(
apis:
  - name: test.v1.Service
  - name: google.cloud.location.Locations
  - name: google.iam.v1.IAMPolicy
http:
  rules:
  - selector: google.cloud.location.Locations.GetLocation
    get: 'OverwriteGetLocationPath'
  - selector: google.cloud.location.Locations.ListLocations
    get: 'OverwriteListLocationPath'
  - selector: google.iam.v1.IAMPolicy.SetIamPolicy
    post: 'OverwriteSetIamPolicyPath'
    body: '*'
  - selector: google.cloud.Redundant.RedundantGet
    get: 'OverwriteListLocationPath'
)""";

auto constexpr kAnnotationsProto = R"""(
    syntax = "proto3";
    package google.api;
    import "google/api/http.proto";
    import "google/protobuf/descriptor.proto";
    extend google.protobuf.MethodOptions {
      // See `HttpRule`.
      HttpRule http = 72295728;
    };
)""";

auto constexpr kHttpProto = R"""(
    syntax = "proto3";
    package google.api;
    option cc_enable_arenas = true;
    message Http {
      repeated HttpRule rules = 1;
      bool fully_decode_reserved_expansion = 2;
    }
    message HttpRule {
      string selector = 1;
      oneof pattern {
        string get = 2;
        string put = 3;
        string post = 4;
        string delete = 5;
        string patch = 6;
        CustomHttpPattern custom = 8;
      }
      string body = 7;
      string response_body = 12;
      repeated HttpRule additional_bindings = 11;
    }
    message CustomHttpPattern {
      string kind = 1;
      string path = 2;
    };
)""";

auto constexpr kMixinLocationProto = R"""(
syntax = "proto3";
package google.cloud.location;
import "google/api/annotations.proto";
import "google/api/http.proto";
import "test/v1/common.proto";

service Locations {
  rpc GetLocation(test.v1.Request) returns (test.v1.Response) {
    option (google.api.http) = {
      get: "ToBeOverwrittenPath"
    };
  }
  rpc ListLocations(test.v1.Request) returns (test.v1.Response) {
    option (google.api.http) = {
      get: "ToBeOverwrittenPath"
    };
  }
}
)""";

auto constexpr kMixinIAMPolicyProto = R"""(
syntax = "proto3";
package google.iam.v1;
import "google/api/annotations.proto";
import "google/api/http.proto";
import "test/v1/common.proto";

service IAMPolicy {
  rpc SetIamPolicy(test.v1.Request) returns (test.v1.Response) {
    option (google.api.http) = {
      get: "ToBeOverwrittenPath"
    };
  }
}
)""";

auto constexpr kClientProto1 = R"""(
syntax = "proto3";
package test.v1;
import "test/v1/common.proto";

service Service0 {
  rpc method0(Request) returns (Response) {}
}
)""";

auto constexpr kClientProto2 = R"""(
syntax = "proto3";
package test.v1;
import "test/v1/common.proto";

service Service1 {
  rpc method0(Request) returns (Response) {}
  rpc GetLocation(Request) returns (Response) {}
  rpc ListLocations(Request) returns (Response) {}
}
)""";

class MixinUtilsTest : public generator_testing::DescriptorPoolFixture {
 public:
  void SetUp() override {
    ASSERT_TRUE(AddProtoFile("google/api/http.proto", kHttpProto));
    ASSERT_TRUE(
        AddProtoFile("google/api/annotations.proto", kAnnotationsProto));
    ASSERT_TRUE(AddProtoFile("test/v1/service1.proto", kClientProto1));
    ASSERT_TRUE(AddProtoFile("test/v1/service2.proto", kClientProto2));
    ASSERT_TRUE(AddProtoFile("google/cloud/location/locations.proto",
                             kMixinLocationProto));
    ASSERT_TRUE(
        AddProtoFile("google/iam/v1/iam_policy.proto", kMixinIAMPolicyProto));
  }

 protected:
  YAML::Node service_config_ = YAML::Load(kServiceConfigYaml);
  YAML::Node service_config_redundant_ =
      YAML::Load(kServiceConfigRedundantRulesYaml);
};

TEST_F(MixinUtilsTest, FilesParseSuccessfully) {
  ASSERT_THAT(FindFile("google/protobuf/descriptor.proto"), NotNull());
  ASSERT_THAT(FindFile("google/api/http.proto"), NotNull());
  ASSERT_THAT(FindFile("google/api/annotations.proto"), NotNull());
  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_THAT(FindFile("test/v1/service1.proto"), NotNull());
  ASSERT_THAT(FindFile("test/v1/service2.proto"), NotNull());
  ASSERT_THAT(FindFile("google/cloud/location/locations.proto"), NotNull());
  ASSERT_THAT(FindFile("google/iam/v1/iam_policy.proto"), NotNull());
}

TEST_F(MixinUtilsTest, ExtractMixinProtoPathsFromYaml) {
  auto const mixin_proto_paths = GetMixinProtoPaths(service_config_);
  EXPECT_THAT(mixin_proto_paths,
              AllOf(Contains("google/cloud/location/locations.proto"),
                    Contains("google/iam/v1/iam_policy.proto"),
                    Contains("google/longrunning/operations.proto")));
}

TEST_F(MixinUtilsTest, GetMixinMethods) {
  FileDescriptor const* file = FindFile("test/v1/service1.proto");
  ASSERT_THAT(file, NotNull());

  ServiceDescriptor const* service = file->service(0);
  ASSERT_THAT(service, NotNull());

  auto const& mixin_methods = GetMixinMethods(service_config_, *service);

  auto constexpr kText0 = R"pb(
    selector: "google.cloud.location.Locations.GetLocation"
    get: "OverwriteGetLocationPath"
    body: ""
  )pb";

  auto constexpr kText1 = R"pb(
    selector: "google.cloud.location.Locations.ListLocations"
    get: "OverwriteListLocationPath"
    body: ""
  )pb";

  auto constexpr kText2 = R"pb(
    selector: "google.iam.v1.IAMPolicy.SetIamPolicy"
    post: "OverwriteSetIamPolicyPath"
    body: "*"
    additional_bindings:
    [ { post: "OverwriteSetIamPolicyPath0", body: "*" }
      , { get: "OverwriteSetIamPolicyPath1", body: "" }]
  )pb";

  google::api::HttpRule http_rule0;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kText0, &http_rule0));

  google::api::HttpRule http_rule1;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kText1, &http_rule1));

  google::api::HttpRule http_rule2;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kText2, &http_rule2));

  auto mixin_matcher = [](auto full_name, auto stub_name, auto stub_fqn,
                          auto http_rule) {
    return AllOf(Field(&MixinMethod::method, HasMethod(full_name)),
                 Field(&MixinMethod::grpc_stub_name, Eq(stub_name)),
                 Field(&MixinMethod::grpc_stub_fqn, Eq(stub_fqn)),
                 Field(&MixinMethod::http_override, IsProtoEqual(http_rule)));
  };

  EXPECT_THAT(
      mixin_methods,
      ElementsAre(
          mixin_matcher("google.cloud.location.Locations.GetLocation",
                        "locations_stub", "google::cloud::location::Locations",
                        http_rule0),
          mixin_matcher("google.cloud.location.Locations.ListLocations",
                        "locations_stub", "google::cloud::location::Locations",
                        http_rule1),
          mixin_matcher("google.iam.v1.IAMPolicy.SetIamPolicy",
                        "iampolicy_stub", "google::iam::v1::IAMPolicy",
                        http_rule2)));
}

TEST_F(MixinUtilsTest, GetMixinMethodsWithDuplicatedMixinNames) {
  FileDescriptor const* file = FindFile("test/v1/service2.proto");
  ASSERT_THAT(file, NotNull());

  ServiceDescriptor const* service = file->service(0);
  ASSERT_THAT(service, NotNull());

  auto const& mixin_methods = GetMixinMethods(service_config_, *service);
  EXPECT_EQ(mixin_methods.size(), 1);
  EXPECT_EQ(mixin_methods[0].method.get().full_name(),
            "google.iam.v1.IAMPolicy.SetIamPolicy");
}

TEST_F(MixinUtilsTest, GetMixinMethodsWithRedundantRules) {
  FileDescriptor const* file = FindFile("test/v1/service1.proto");
  ASSERT_THAT(file, NotNull());

  ServiceDescriptor const* service = file->service(0);
  ASSERT_THAT(service, NotNull());

  auto const& mixin_methods =
      GetMixinMethods(service_config_redundant_, *service);
  EXPECT_EQ(mixin_methods.size(), 3);
  for (auto const& mixin_method : mixin_methods) {
    EXPECT_NE(mixin_method.method.get().full_name(),
              "google.cloud.Redundant.RedundantGet");
  }
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
