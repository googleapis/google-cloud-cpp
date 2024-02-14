// Copyright 2024 Google LLC
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

#include "generator/internal/request_id.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::FakeSourceTree;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::NotNull;

// We factor out the protobuf mumbo jumbo to keep the tests concise and
// self-contained. The protobuf objects must be in scope for the duration of a
// test. To achieve this, we pass in a `test` lambda which is invoked in this
// method.
void RunRequestIdTest(std::string const& service_proto,
                      std::function<void(FileDescriptor const*)> const& test) {
  auto constexpr kServiceBoilerPlate = R"""(
syntax = "proto3";
package google.cloud.test.v1;
import "google/api/field_info.proto";
)""";

  auto constexpr kFieldInfoProto = R"""(
syntax = "proto3";
package google.api;
import "google/protobuf/descriptor.proto";

extend google.protobuf.FieldOptions {
  google.api.FieldInfo field_info = 291403980;
}
message FieldInfo {
  enum Format {
    FORMAT_UNSPECIFIED = 0;
    UUID4 = 1;
    IPV4 = 2;
    IPV6 = 3;
    IPV4_OR_IPV6 = 4;
  }
  Format format = 1;
}
)""";

  FakeSourceTree source_tree(std::map<std::string, std::string>{
      {"google/api/field_info.proto", kFieldInfoProto},
      {"google/cloud/foo/service.proto", kServiceBoilerPlate + service_proto}});
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db(
      &source_tree);
  google::protobuf::SimpleDescriptorDatabase simple_db;
  FileDescriptorProto file_proto;
  // We need descriptor.proto to be accessible by the pool
  // since our test file imports it.
  FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto);
  simple_db.Add(file_proto);
  google::protobuf::MergedDescriptorDatabase merged_db(&simple_db,
                                                       &source_tree_db);
  generator_testing::ErrorCollector collector;
  DescriptorPool pool(&merged_db, &collector);

  // Run the test.
  test(pool.FindFileByName("google/cloud/foo/service.proto"));
}

TEST(RequestId, FieldIsRequestId) {
  auto constexpr kProto = R"""(
message Request {
  string treat_as_request_id = 1 [
    (google.api.field_info).format = UUID4
  ];
}
)""";

  RunRequestIdTest(kProto, [](FileDescriptor const* fd) {
    ASSERT_THAT(fd, NotNull());
    auto const& field = *fd->message_type(0)->field(0);
    EXPECT_TRUE(MeetsRequestIdRequirements(field));
  });
}

TEST(RequestId, FieldBadType) {
  auto constexpr kProto = R"""(
message Request {
  bytes treat_as_request_id = 1 [
    (google.api.field_info).format = UUID4
  ];
}
)""";

  RunRequestIdTest(kProto, [](FileDescriptor const* fd) {
    ASSERT_THAT(fd, NotNull());
    auto const& field = *fd->message_type(0)->field(0);
    EXPECT_FALSE(MeetsRequestIdRequirements(field));
  });
}

TEST(RequestId, FieldRepeated) {
  auto constexpr kProto = R"""(
message Request {
  repeated string treat_as_request_id = 1 [
    (google.api.field_info).format = UUID4
  ];
}
)""";

  RunRequestIdTest(kProto, [](FileDescriptor const* fd) {
    ASSERT_THAT(fd, NotNull());
    auto const& field = *fd->message_type(0)->field(0);
    EXPECT_FALSE(MeetsRequestIdRequirements(field));
  });
}

TEST(RequestId, FieldNoExtension) {
  auto constexpr kProto = R"""(
message Request {
  string request_id = 1;
}
)""";

  RunRequestIdTest(kProto, [](FileDescriptor const* fd) {
    ASSERT_THAT(fd, NotNull());
    auto const& field = *fd->message_type(0)->field(0);
    EXPECT_FALSE(MeetsRequestIdRequirements(field));
  });
}

TEST(RequestId, FieldBadFormat) {
  auto constexpr kProto = R"""(
message Request {
  string treat_as_request_id = 1 [
    (google.api.field_info).format = IPV4
  ];
}
)""";

  RunRequestIdTest(kProto, [](FileDescriptor const* fd) {
    ASSERT_THAT(fd, NotNull());
    auto const& field = *fd->message_type(0)->field(0);
    EXPECT_FALSE(MeetsRequestIdRequirements(field));
  });
}

TEST(RequestId, MethodRequestFieldName) {
  auto constexpr kProto = R"""(
message M0 {
  string f1 = 1;
  string f2 = 2;
  string request_id = 3 [ (google.api.field_info).format = UUID4 ];
  string another_request_id = 4 [ (google.api.field_info).format = UUID4 ];
}

message M1 {
  string not_in_yaml = 1[ (google.api.field_info).format = UUID4 ];
}

message M2 {
  string alternative = 1[ (google.api.field_info).format = UUID4 ];
}

message M3 {
  string bad_format = 1[ (google.api.field_info).format = IPV4 ];
}

service Service {
  rpc Method0(M0) returns (M0) {}
  rpc Method1(M1) returns (M0) {}
  rpc Method2(M2) returns (M0) {}
  rpc Method3(M3) returns (M0) {}
}

)""";

  auto constexpr kServiceConfigYaml = R"""(publishing:
  method_settings:
  - selector: google.cloud.test.v1.Service.Method0
    auto_populated_fields:
    - request_id
  - selector: google.cloud.test.v1.Service.Method2
    auto_populated_fields:
    - alternative
  - selector: google.cloud.test.v1.Service.Method3
    auto_populated_fields:
    - bad_format
)""";

  auto const service_config = YAML::Load(kServiceConfigYaml);

  RunRequestIdTest(kProto, [&service_config](FileDescriptor const* fd) {
    ASSERT_THAT(fd, NotNull());
    ASSERT_EQ(fd->service_count(), 1);
    auto const& sd = *fd->service(0);
    ASSERT_EQ(sd.method_count(), 4);
    EXPECT_EQ(RequestIdFieldName(service_config, *sd.method(0)), "request_id");
    EXPECT_EQ(RequestIdFieldName(service_config, *sd.method(1)), "");
    EXPECT_EQ(RequestIdFieldName(service_config, *sd.method(2)), "alternative");
    EXPECT_EQ(RequestIdFieldName(service_config, *sd.method(3)), "");
  });
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
