// Copyright 2023 Google LLC
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

#include "generator/internal/format_method_comments.h"
#include "generator/testing/descriptor_pool_fixture.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::StartsWith;

class FormatMethodCommentsTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(FormatMethodCommentsTest, Full) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
    rpc Method(Request) returns (Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual =
      FormatMethodComments(*method, "/// <variable parameter comments>\n");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(
      lines,
      ElementsAre(
          Eq(std::string{"  ///"}),  //
          Eq(std::string{"  /// Some brief description of the method."}),
          Eq(std::string{"  ///"}),  //
          HasSubstr("<variable parameter comments>"),
          StartsWith("  /// @param opts Optional."),
          StartsWith("  ///     backoff policies."),
          Eq(std::string{"  /// @return $method_return_doxygen_link$"}),
          Eq("  ///"),
          StartsWith("  /// [test.v1.Request]: "
                     "@googleapis_reference_link{test/v1/common.proto#L"),
          StartsWith("  /// [test.v1.Response]: "
                     "@googleapis_reference_link{test/v1/common.proto#L"),
          Eq(std::string{"  ///"}), Eq(std::string{})));
}

TEST_F(FormatMethodCommentsTest, EmptyReturn) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
    rpc Method(Request) returns (google.protobuf.Empty) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Not(Contains(HasSubstr("[google.protobuf.Empty")))));
}

TEST_F(FormatMethodCommentsTest, StreamingRead) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
    rpc Method(Request) returns (stream Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Contains(StartsWith(
                        "  /// [test.v1.Response]: "
                        "@googleapis_reference_link{test/v1/common.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, StreamingWrite) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
    rpc Method(stream Request) returns (Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Contains(StartsWith(
                        "  /// [test.v1.Response]: "
                        "@googleapis_reference_link{test/v1/common.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, StreamReadWrite) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
    rpc Method(stream Request) returns (stream Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Contains(StartsWith(
                        "  /// [test.v1.Response]: "
                        "@googleapis_reference_link{test/v1/common.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, PaginatedMessage) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

message Resource {}
message ListRequest {
    string page_token = 1;
    int32 page_size = 2;
}
message ListResponse {
    repeated Resource items = 1;
    string next_page_token = 2;
}

service Service {
    // Some brief description of the method.
    rpc Method(ListRequest) returns (ListResponse) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(
      lines, AllOf(Contains(StartsWith(
                       "  /// [test.v1.ListRequest]: "
                       "@googleapis_reference_link{test/v1/service.proto#L")),
                   Contains(StartsWith(
                       "  /// [test.v1.Resource]: "
                       "@googleapis_reference_link{test/v1/service.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, PaginatedStrings) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/protobuf/empty.proto";
import "test/v1/common.proto";
package test.v1;

message ListRequest {
    string page_token = 1;
    int32 page_size = 2;
}
message ListResponse {
    repeated string items = 1;
    string next_page_token = 2;
}

service Service {
    // Some brief description of the method.
    rpc Method(ListRequest) returns (ListResponse) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                  "  /// [test.v1.ListRequest]: "
                  "@googleapis_reference_link{test/v1/service.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, LongRunningWithResponse) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/longrunning/operations.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
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
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Contains(StartsWith(
                        "  /// [test.v1.Response]: "
                        "@googleapis_reference_link{test/v1/common.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, LongRunningWithMetadata) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/longrunning/operations.proto";
import "test/v1/common.proto";
package test.v1;

service Service {
    // Some brief description of the method.
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
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "");
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Contains(StartsWith(
                        "  /// [test.v1.Metadata]: "
                        "@googleapis_reference_link{test/v1/common.proto#L")),
                    Not(Contains(HasSubstr("[google.protobuf.Empty")))));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google