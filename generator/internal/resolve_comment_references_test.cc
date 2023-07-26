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

#include "generator/internal/resolve_comment_references.h"
#include "generator/testing/descriptor_pool_fixture.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pair;

class ResolveCommentsReferenceTest
    : public generator_testing::DescriptorPoolFixture {
 public:
  google::protobuf::MethodDescriptor const* FindMethod(
      std::string const& name) {
    return pool().FindMethodByName(name);
  }
};

TEST_F(ResolveCommentsReferenceTest, Multiple) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message Resource {}

// Test service.
service Service {
    rpc Other(Request) returns (Response) {}
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
//
// It contains some simple text, and a reference to
// [Resource][test.v1.Resource], a message not used by the method. It also
// contains a reference to [Other][test.v1.Service.Other], another RPC in the
// same service.
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(actual, UnorderedElementsAre(
                          Pair("test.v1.Resource",
                               Field(&ProtoDefinitionLocation::filename,
                                     "test/v1/service.proto")),
                          Pair("test.v1.Service.Other",
                               Field(&ProtoDefinitionLocation::filename,
                                     "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, EnumType) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

enum State {
  STATE_0 = 0;
  STATE_1 = 1;
}

// Test service.
service Service {
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a enum [State][test.v1.State]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(actual,
              UnorderedElementsAre(Pair(
                  "test.v1.State", Field(&ProtoDefinitionLocation::filename,
                                         "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, EnumValue) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

enum State {
  STATE_0 = 0;
  STATE_1 = 1;
}

// Test service.
service Service {
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a enum value [STATE_0][test.v1.STATE_0]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(actual,
              UnorderedElementsAre(Pair(
                  "test.v1.STATE_0", Field(&ProtoDefinitionLocation::filename,
                                           "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, EnumValueInMessage) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message Container {
  enum State {
    STATE_0 = 0;
    STATE_1 = 1;
  }

  // The current state
  State state  = 1;
}

// Test service.
service Service {
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a enum value [STATE_0][test.v1.Container.State.STATE_0]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(
      actual,
      UnorderedElementsAre(Pair(
          "test.v1.Container.State.STATE_0",
          Field(&ProtoDefinitionLocation::filename, "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, Field) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message Resource {
  string the_field = 1;
}

// Test service.
service Service {
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a field [the_field][test.v1.Resource.the_field]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(
      actual,
      UnorderedElementsAre(Pair(
          "test.v1.Resource.the_field",
          Field(&ProtoDefinitionLocation::filename, "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, Message) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message Resource {}

// Test service.
service Service {
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a message [Resource][test.v1.Resource]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(actual,
              UnorderedElementsAre(Pair(
                  "test.v1.Resource", Field(&ProtoDefinitionLocation::filename,
                                            "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, Method) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service OtherService {
  rpc SomeMethod(Request) returns (Response) {}
}

service Service {
  rpc Unary(Request) returns (Request) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a method [SomeMethod][test.v1.OtherService.SomeMethod]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(
      actual,
      UnorderedElementsAre(Pair(
          "test.v1.OtherService.SomeMethod",
          Field(&ProtoDefinitionLocation::filename, "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, Oneof) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message Resource {
  oneof which {
    string o1 = 1;
    string o2 = 2;
  }
}

// Test service.
service Service {
    rpc Other(Request) returns (Response) {}
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a oneof [which][test.v1.Resource.which]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(
      actual,
      UnorderedElementsAre(Pair(
          "test.v1.Resource.which",
          Field(&ProtoDefinitionLocation::filename, "test/v1/service.proto"))));
}

TEST_F(ResolveCommentsReferenceTest, Service) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service OtherService {
  rpc Unary(Request) returns (Response) {}
}

// Test service.
service Service {
    rpc Unary(Request) returns (Response) {}
}
)""";

  auto constexpr kComment = R"""(// Test comment.
// Reference a service [Service][test.v1.OtherService]
  )""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));

  auto const actual = ResolveCommentReferences(kComment, pool());
  EXPECT_THAT(
      actual,
      UnorderedElementsAre(Pair(
          "test.v1.OtherService",
          Field(&ProtoDefinitionLocation::filename, "test/v1/service.proto"))));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
