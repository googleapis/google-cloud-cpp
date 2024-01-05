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

#include "generator/internal/doxygen.h"
#include "generator/testing/descriptor_pool_fixture.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::testing::Eq;
using ::testing::NotNull;

class FormatDoxygenLinkTest : public generator_testing::DescriptorPoolFixture {
 public:
  google::protobuf::MethodDescriptor const* FindMethod(
      std::string const& name) {
    return pool().FindMethodByName(name);
  }
};

TEST_F(FormatDoxygenLinkTest, ImportedAndLocalMessageDefinitions) {
  auto constexpr kProtoFile = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message LocalResponse {}

// Test service.
service Service {
    rpc Imported(Request) returns (Response) {}
    rpc Local(Request) returns (LocalResponse) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kProtoFile));

  auto const* output_type_descriptor =
      FindMethod("test.v1.Service.Imported")->output_type();
  ASSERT_THAT(output_type_descriptor, NotNull());
  auto result = FormatDoxygenLink(*output_type_descriptor);
  EXPECT_THAT(
      result,
      Eq("@googleapis_link{test::v1::Response,test/v1/common.proto#L11}"));

  output_type_descriptor = FindMethod("test.v1.Service.Local")->output_type();
  ASSERT_THAT(output_type_descriptor, NotNull());
  result = FormatDoxygenLink(*output_type_descriptor);
  EXPECT_THAT(
      result,
      Eq("@googleapis_link{test::v1::LocalResponse,test/v1/service.proto#L6}"));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
