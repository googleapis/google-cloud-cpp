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

#include "generator/internal/resolve_method_return.h"
#include "generator/testing/descriptor_pool_fixture.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pair;

class ResolveMethodReturnTest
    : public generator_testing::DescriptorPoolFixture {
 public:
  google::protobuf::MethodDescriptor const* FindMethod(
      std::string const& name) {
    return pool().FindMethodByName(name);
  }
};

TEST_F(ResolveMethodReturnTest, Simple) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Response", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, StreamingRead) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (stream Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Response", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, StreamingWrite) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(stream Request) returns (Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Response", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, StreamingReadWrite) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(stream Request) returns (stream Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Response", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, Empty) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (google.protobuf.Empty) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_FALSE(actual.has_value());
}

TEST_F(ResolveMethodReturnTest, LongRunningFullyQualifiedNames) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/longrunning/operations.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (google.longrunning.Operation) {
        option (google.longrunning.operation_info) = {
          response_type: "test.v1.Response"
          metadata_type: "test.v1.Metadata"
        };
    }
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Response", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, LongRunningLocalNames) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/longrunning/operations.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (google.longrunning.Operation) {
        option (google.longrunning.operation_info) = {
          response_type: "Response"
          metadata_type: "Metadata"
        };
    }
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Response", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, LongRunningWithMetadataFullyQualified) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/longrunning/operations.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (google.longrunning.Operation) {
        option (google.longrunning.operation_info) = {
          response_type: "google.protobuf.Empty"
          metadata_type: "test.v1.Metadata"
        };
    }
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Metadata", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, LongRunningWithMetadataLocalName) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/longrunning/operations.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    rpc Method(Request) returns (google.longrunning.Operation) {
        option (google.longrunning.operation_info) = {
          response_type: "google.protobuf.Empty"
          metadata_type: "Metadata"
        };
    }
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Metadata", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/common.proto")));
}

TEST_F(ResolveMethodReturnTest, Paginated) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message Resource {}
message PageRequest {
    string page_token = 1;
    int32 page_size = 2;
}
message PageResponse {
    repeated Resource items = 1;
    string next_page_token = 2;
}

service Service {
    rpc Paginated(PageRequest) returns (PageResponse) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Paginated");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_TRUE(actual.has_value());
  EXPECT_THAT(*actual,
              Pair("test.v1.Resource", Field(&ProtoDefinitionLocation::filename,
                                             "test/v1/service.proto")));
}

TEST_F(ResolveMethodReturnTest, PaginatedString) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

message PageRequest {
    string page_token = 1;
    int32 page_size = 2;
}
message PageResponse {
    repeated string items = 1;
    string next_page_token = 2;
}

service Service {
    rpc Paginated(PageRequest) returns (PageResponse) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = FindMethod("test.v1.Service.Paginated");
  ASSERT_THAT(method, NotNull());

  auto const actual = ResolveMethodReturn(*method);
  ASSERT_FALSE(actual.has_value());
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
