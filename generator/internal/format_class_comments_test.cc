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

#include "generator/internal/format_class_comments.h"
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
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::StartsWith;

class FormatClassCommentsTest
    : public generator_testing::DescriptorPoolFixture {};

TEST_F(FormatClassCommentsTest, Basic) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

// A brief description of the service.
//
// A longer description of the service.
service Service {
    rpc Method(Request) returns (Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* service = pool().FindServiceByName("test.v1.Service");
  ASSERT_THAT(service, NotNull());

  auto const actual = FormatClassCommentsFromServiceComments(
      *service, std::string{service->name()}, absl::nullopt);

  // Verify it has exactly one trailing `///` comment.
  EXPECT_THAT(actual, AllOf(EndsWith("\n///"), Not(EndsWith("\n///\n///"))));
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(lines, AllOf(Contains("/// A brief description of the service."),
                           Contains("/// A longer description of the service."),
                           Contains("/// @par Equality"),
                           Contains("/// @par Performance"),
                           Contains("/// @par Thread Safety")));
}

TEST_F(FormatClassCommentsTest, NoLeadingComment) {
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
  auto const* service = pool().FindServiceByName("test.v1.Service");
  ASSERT_THAT(service, NotNull());

  auto const actual = FormatClassCommentsFromServiceComments(
      *service, std::string{service->name()}, absl::nullopt);

  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(
      lines, AllOf(Contains("/// ServiceClient"), Contains("/// @par Equality"),
                   Contains("/// @par Performance"),
                   Contains("/// @par Thread Safety")));
}

TEST_F(FormatClassCommentsTest, WithReferences) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package test.v1;

// A brief description of the service.
//
// This references the [Request][test.v1.Request] message, another service
// [OtherService][test.v1.OtherService], and even a field
// [Resource.parent][test.v1.Resource.parent]
service Service {
    rpc Method(Request) returns (Response) {}
}

// Just to test references.
service OtherService {}
message Resource {
    string parent = 1;
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("test/v1/service.proto", kContents));
  auto const* service = pool().FindServiceByName("test.v1.Service");
  ASSERT_THAT(service, NotNull());

  auto const actual = FormatClassCommentsFromServiceComments(
      *service, std::string{service->name()}, absl::nullopt);

  // Verify the first reference is separated by an empty line and that it ends
  // with a single `///` comment.
  EXPECT_THAT(actual, AllOf(HasSubstr("///\n/// [test.v1.OtherService]: "),
                            EndsWith("\n///"), Not(EndsWith("\n///\n///"))));
  auto const lines = std::vector<std::string>{absl::StrSplit(actual, '\n')};
  EXPECT_THAT(
      lines,
      AllOf(Contains("/// A brief description of the service."),
            Contains(StartsWith(
                "/// [test.v1.Request]: "
                "@googleapis_reference_link{test/v1/common.proto#L")),
            Contains(StartsWith(
                "/// [test.v1.OtherService]: "
                "@googleapis_reference_link{test/v1/service.proto#L")),
            Contains(StartsWith(
                "/// [test.v1.Resource.parent]: "
                "@googleapis_reference_link{test/v1/service.proto#L")),
            Contains("/// @par Equality"), Contains("/// @par Performance"),
            Contains("/// @par Thread Safety")));
}

TEST_F(FormatClassCommentsTest, MonitoringFixup) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package google.monitoring.v3;

message Group {}

// A brief description of the service.
//
// Use a relative link [groups](#google.monitoring.v3.Group).
service Service {
    rpc Method(test.v1.Request) returns (test.v1.Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("google/monitoring/v3/service.proto", kContents));
  auto const* service =
      pool().FindServiceByName("google.monitoring.v3.Service");
  ASSERT_THAT(service, NotNull());

  auto const actual = FormatClassCommentsFromServiceComments(
      *service, std::string{service->name()}, absl::nullopt);
  // Verify the relative link is converted to an absolute link.
  EXPECT_THAT(actual,
              AllOf(HasSubstr("[groups][google.monitoring.v3.Group]"),
                    HasSubstr("///\n/// [google.monitoring.v3.Group]: "),
                    EndsWith("\n///"), Not(EndsWith("\n///\n///"))));
}

TEST_F(FormatClassCommentsTest, ReplacesServiceName) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package google.monitoring.v3;

message Group {}

// A brief description of the Service.
//
// Use a relative link [groups](#google.monitoring.v3.Group).
service Service {
    rpc Method(test.v1.Request) returns (test.v1.Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("google/monitoring/v3/service.proto", kContents));
  auto const* service =
      pool().FindServiceByName("google.monitoring.v3.Service");
  ASSERT_THAT(service, NotNull());

  auto const actual = FormatClassCommentsFromServiceComments(
      *service, "NewService", absl::nullopt);
  EXPECT_THAT(actual, AllOf(HasSubstr("NewService")));
}

TEST_F(FormatClassCommentsTest, ReplacesServiceComment) {
  auto constexpr kContents = R"""(
syntax = "proto3";
import "test/v1/common.proto";
package google.monitoring.v3;

message Group {}

// A brief description of the Service.
//
// Use a relative link [groups](#google.monitoring.v3.Group).
service Service {
    rpc Method(test.v1.Request) returns (test.v1.Response) {}
}
)""";

  ASSERT_THAT(FindFile("test/v1/common.proto"), NotNull());
  ASSERT_TRUE(AddProtoFile("google/monitoring/v3/service.proto", kContents));
  auto const* service =
      pool().FindServiceByName("google.monitoring.v3.Service");
  ASSERT_THAT(service, NotNull());

  auto const actual = FormatClassCommentsFromServiceComments(
      *service, std::string{service->name()}, "New Service comments");
  EXPECT_THAT(actual, AllOf(HasSubstr("New Service comments")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
