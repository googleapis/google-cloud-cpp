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
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include "absl/memory/memory.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::FakeSourceTree;
using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pair;

class ResolveCommentsReferenceTest : public ::testing::Test {
 public:
  ResolveCommentsReferenceTest();

  google::protobuf::FileDescriptor const* FindFile(
      std::string const& name) const {
    return pool_.FindFileByName(name);
  }
  bool AddProtoFile(std::string const& name, std::string contents) {
    source_tree_.Insert(name, std::move(contents));
    return pool_.FindFileByName(name) != nullptr;
  }

  google::protobuf::MethodDescriptor const* FindMethod(
      std::string const& name) {
    return pool_.FindMethodByName(name);
  }

  google::protobuf::DescriptorPool const& pool() const { return pool_; }

 private:
  std::unique_ptr<google::protobuf::DescriptorPool::ErrorCollector>
      descriptor_error_collector_;
  std::unique_ptr<google::protobuf::compiler::MultiFileErrorCollector>
      multifile_error_collector_;
  FakeSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;
  google::protobuf::DescriptorPool pool_;
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

auto constexpr kLongrunningOperationsContents = R"""(
// This is a (extremely) simplified version of LRO support.
syntax = "proto3";
package google.longrunning;
import "google/protobuf/descriptor.proto";

extend google.protobuf.MethodOptions {
    google.longrunning.OperationInfo operation_info = 1049;
}

message OperationInfo {
    string response_type = 1;
    string metadata_type = 2;
}

message Operation {
    string name = 1;
}
)""";

auto constexpr kCommonFileContents = R"""(
// We need to test that our generator handles references to different entities.
// This simulated .proto file provides their definition.

syntax = "proto3";
package test.v1;

// A request type for the methods
message Request {}
// A response type for the methods
message Response {}
// A metadata type for some LROs
message Metadata {}
)""";

ResolveCommentsReferenceTest::ResolveCommentsReferenceTest()
    : descriptor_error_collector_(
          absl::make_unique<generator_testing::ErrorCollector>()),
      multifile_error_collector_(
          absl::make_unique<generator_testing::MultiFileErrorCollector>()),
      source_tree_({
          {"google/longrunning/operation.proto",
           kLongrunningOperationsContents},
          {"test/v1/common.proto", kCommonFileContents},
      }),
      source_tree_db_(&source_tree_),
      merged_db_(&simple_db_, &source_tree_db_),
      pool_(&merged_db_, descriptor_error_collector_.get()) {
  source_tree_db_.RecordErrorsTo(multifile_error_collector_.get());
  // We need the basic "descriptor" protos to be available.
  google::protobuf::FileDescriptorProto proto;
  google::protobuf::FileDescriptorProto::descriptor()->file()->CopyTo(&proto);
  simple_db_.Add(proto);
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
