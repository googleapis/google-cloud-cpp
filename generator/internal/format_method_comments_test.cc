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
    // Some brief description of the method. With a reference to another
    // message [Metadata][test.v1.Metadata].
    rpc Method(Request) returns (Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(
      *method, "  /// <variable parameter comments>\n", false);
  // Modern versions of googletest produce diffs for large strings with
  // newlines.
  EXPECT_EQ(actual, R"""(  // clang-format off
  ///
  /// Some brief description of the method. With a reference to another
  /// message [Metadata][test.v1.Metadata].
  ///
  /// <variable parameter comments>
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return the result of the RPC. The response message type
  ///     ([test.v1.Response])
  ///     is mapped to a C++ class using the [Protobuf mapping rules].
  ///     If the request fails, the [`StatusOr`] contains the error details.
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [test.v1.Metadata]: @googleapis_reference_link{test/v1/common.proto#L13}
  /// [test.v1.Request]: @googleapis_reference_link{test/v1/common.proto#L9}
  /// [test.v1.Response]: @googleapis_reference_link{test/v1/common.proto#L11}
  ///
  // clang-format on
)""");
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

  auto const actual = FormatMethodComments(*method, "", false);
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

  auto const actual = FormatMethodComments(*method, "", false);
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

  auto const actual = FormatMethodComments(*method, "", false);
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

  auto const actual = FormatMethodComments(*method, "", false);
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

  auto const actual = FormatMethodComments(*method, "", false);
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

  auto const actual = FormatMethodComments(*method, "", false);
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

  auto const actual = FormatMethodComments(*method, "", false);
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(
      lines,
      AllOf(Contains(StartsWith(
                "  /// [Long Running Operation]: https://google.aip.dev/151")),
            Contains(StartsWith(
                "  /// [test.v1.Request]: "
                "@googleapis_reference_link{test/v1/common.proto#L")),
            Contains(StartsWith(
                "  /// [test.v1.Response]: "
                "@googleapis_reference_link{test/v1/common.proto#L"))));
}

TEST_F(FormatMethodCommentsTest, HttpLongRunningWithResponse) {
  auto constexpr kExtendedOperationsProto = R"""(
syntax = "proto3";
package google.cloud;
import "google/protobuf/descriptor.proto";

extend google.protobuf.FieldOptions {
  OperationResponseMapping operation_field = 1149;
  string operation_request_field = 1150;
  string operation_response_field = 1151;
}

extend google.protobuf.MethodOptions {
  string operation_service = 1249;
  bool operation_polling_method = 1250;
}

enum OperationResponseMapping {
  UNDEFINED = 0;
  NAME = 1;
  STATUS = 2;
  ERROR_CODE = 3;
  ERROR_MESSAGE = 4;
}
)""";

  auto constexpr kContents = R"""(
syntax = "proto3";
import "google/cloud/extended_operations.proto";
import "test/v1/common.proto";
package test.v1;

message Operation {}

service Service {
    // Some brief description of the method.
    rpc Method(Request) returns (Operation) {
    option (google.cloud.operation_service) = "ZoneOperations";
    }
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("google/cloud/extended_operations.proto",
                           kExtendedOperationsProto));
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* method = pool().FindMethodByName("test.v1.Service.Method");
  ASSERT_THAT(method, NotNull());

  auto const actual = FormatMethodComments(*method, "", true);
  // This is the only test where we will check all the lines in detail.
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines,
              AllOf(Contains(StartsWith(
                        "  /// [Long Running Operation]: "
                        "http://cloud/compute/docs/api/how-tos/"
                        "api-requests-responses#handling_api_responses")),
                    Contains(StartsWith(
                        "  /// [test.v1.Request]: "
                        "@cloud_cpp_reference_link{test/v1/common.proto#L"))));
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

  auto const actual = FormatMethodComments(*method, "", false);
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
