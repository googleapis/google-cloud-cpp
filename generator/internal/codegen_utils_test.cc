// Copyright 2020 Google LLC
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

#include "generator/internal/codegen_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Pair;

TEST(LocalInclude, Success) {
  EXPECT_EQ("#include \"google/cloud/status.h\"\n",
            LocalInclude("google/cloud/status.h"));
}

TEST(LocalInclude, Empty) {
  std::string const empty;
  EXPECT_EQ(empty, LocalInclude(""));
}

TEST(SystemInclude, Success) {
  EXPECT_EQ("#include <vector>\n", SystemInclude("vector"));
}

TEST(SystemInclude, Empty) {
  std::string const empty;
  EXPECT_EQ(empty, SystemInclude(""));
}

TEST(CamelCaseToSnakeCase, Success) {
  EXPECT_EQ("foo_bar_b", CamelCaseToSnakeCase("FooBarB"));
  EXPECT_EQ("foo_bar_baz", CamelCaseToSnakeCase("FooBarBaz"));
  EXPECT_EQ("foo_bar_baz", CamelCaseToSnakeCase("fooBarBaz"));
  EXPECT_EQ("foo_bar_ab", CamelCaseToSnakeCase("fooBarAb"));
  EXPECT_EQ("foo_bar_baaaaa", CamelCaseToSnakeCase("fooBarBAAAAA"));
  EXPECT_EQ("foo_bar_b", CamelCaseToSnakeCase("foo_BarB"));
  EXPECT_EQ("v1", CamelCaseToSnakeCase("v1"));
  EXPECT_EQ("", CamelCaseToSnakeCase(""));
  EXPECT_EQ(" ", CamelCaseToSnakeCase(" "));
  EXPECT_EQ("a", CamelCaseToSnakeCase("A"));
  EXPECT_EQ("a_b", CamelCaseToSnakeCase("aB"));
  EXPECT_EQ("foo123", CamelCaseToSnakeCase("Foo123"));
}

TEST(CamelCaseToSnakeCase, Exceptions) {
  EXPECT_EQ("bigquery_read", CamelCaseToSnakeCase("BigQueryRead"));
}

TEST(ServiceNameToFilePath, TrailingServiceInLastComponent) {
  EXPECT_EQ("google/spanner/admin/database/v1/database_admin",
            ServiceNameToFilePath(
                "google.spanner.admin.database.v1.DatabaseAdminService"));
}

TEST(ServiceNameToFilePath, NoTrailingServiceInLastComponent) {
  EXPECT_EQ(
      "google/spanner/admin/database/v1/database_admin",
      ServiceNameToFilePath("google.spanner.admin.database.v1.DatabaseAdmin"));
}

TEST(ServiceNameToFilePath, TrailingServiceInIntermediateComponent) {
  EXPECT_EQ(
      "google/spanner/admin/database_service/v1/database_admin",
      ServiceNameToFilePath(
          "google.spanner.admin.databaseService.v1.DatabaseAdminService"));
}

TEST(ProtoNameToCppName, MessageType) {
  EXPECT_EQ("google::spanner::admin::database::v1::Request",
            ProtoNameToCppName("google.spanner.admin.database.v1.Request"));
}

TEST(BuildNamespaces, NoDirectoryPathInternal) {
  auto result = BuildNamespaces("/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "_internal",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, OneDirectoryPathInternal) {
  auto result = BuildNamespaces("one/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "one_internal",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, TwoDirectoryPathInternal) {
  auto result = BuildNamespaces("unusual/product/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "unusual_product_internal",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, TwoDirectoryPathNotInternal) {
  auto result = BuildNamespaces("unusual/product/");
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "unusual_product",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, ThreeDirectoryPathInternal) {
  auto result =
      BuildNamespaces("google/cloud/spanner/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "spanner_internal",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, ThreeDirectoryPathMocks) {
  auto result = BuildNamespaces("google/cloud/spanner/", NamespaceType::kMocks);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "spanner_mocks",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, ThreeDirectoryPathNotInternal) {
  auto result = BuildNamespaces("google/cloud/translation/");
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "translation",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, FourDirectoryPathInternal) {
  auto result =
      BuildNamespaces("google/cloud/foo/bar/baz/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "foo_bar_baz_internal",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(BuildNamespaces, FourDirectoryPathNotInternal) {
  auto result = BuildNamespaces("google/cloud/foo/bar/baz/");
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "foo_bar_baz",
                                  "GOOGLE_CLOUD_CPP_NS"));
}

TEST(ProcessCommandLineArgs, NoProductPath) {
  auto result = ProcessCommandLineArgs("");
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInvalidArgument,
               "--cpp_codegen_opt=product_path=<path> must be specified."));
}

TEST(ProcessCommandLineArgs, EmptyProductPath) {
  auto result = ProcessCommandLineArgs("product_path=");
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInvalidArgument,
               "--cpp_codegen_opt=product_path=<path> must be specified."));
}

TEST(ProcessCommandLineArgs, ProductPathNeedsFormatting) {
  auto result = ProcessCommandLineArgs("product_path=/google/cloud/pubsub");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("product_path", "google/cloud/pubsub/")));
}

TEST(ProcessCommandLineArgs, ProductPathAlreadyFormatted) {
  auto result = ProcessCommandLineArgs("product_path=google/cloud/pubsub/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("product_path", "google/cloud/pubsub/")));
}

TEST(ProcessCommandLineArgs, NoCopyrightYearParameterOrValue) {
  auto result = ProcessCommandLineArgs("product_path=google/cloud/pubsub/");
  auto expected_year = CurrentCopyrightYear();
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("copyright_year", expected_year)));
}

TEST(ProcessCommandLineArgs, NoCopyrightYearValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",copyright_year=");
  auto expected_year = CurrentCopyrightYear();
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("copyright_year", expected_year)));
}

TEST(ProcessCommandLineArgs, CopyrightYearWithValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",copyright_year=1995");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("copyright_year", "1995")));
}

TEST(ProcessCommandLineArgs, ServiceEndpointEnvVar) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",service_endpoint_env_var=GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("service_endpoint_env_var",
                            "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT")));
  EXPECT_THAT(*result, Contains(Pair("emulator_endpoint_env_var", "")));
}

TEST(ProcessCommandLineArgs, EmulatorEndpointEnvVar) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("emulator_endpoint_env_var",
                                     "SPANNER_EMULATOR_HOST")));
  EXPECT_THAT(*result, Contains(Pair("service_endpoint_env_var", "")));
}

TEST(ProcessCommandLineArgs, ProcessArgOmitService) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_service=Omitted1"
      ",omit_service=Omitted2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("omitted_services", AllOf(HasSubstr("Omitted1"),
                                                      HasSubstr("Omitted2")))));
}

TEST(ProcessCommandLineArgs, ProcessArgOmitRpc) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",omit_rpc=Omitted1"
      ",omit_rpc=Omitted2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("omitted_rpcs", AllOf(HasSubstr("Omitted1"),
                                                  HasSubstr("Omitted2")))));
}

TEST(ProcessCommandLineArgs, ProcessArgGenAsyncRpc) {
  auto result = ProcessCommandLineArgs(
      "gen_async_rpc=Async1"
      ",product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",gen_async_rpc=Async2");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result,
              Contains(Pair("gen_async_rpcs",
                            AllOf(HasSubstr("Async1"), HasSubstr("Async2")))));
}

TEST(ProcessCommandLineArgs, ProcessArgAsyncOnlyRpc) {
  auto result = ProcessCommandLineArgs(
      ",product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",omit_rpc=AsyncOnly"
      ",gen_async_rpc=AsyncOnly");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omitted_rpcs", HasSubstr("AsyncOnly"))));
  EXPECT_THAT(*result,
              Contains(Pair("gen_async_rpcs", HasSubstr("AsyncOnly"))));
}

TEST(ProcessCommandLineArgs, ProcessArgNamespaceAlias) {
  auto result = ProcessCommandLineArgs(
      ",product_path=google/cloud/spanner/"
      ",emulator_endpoint_env_var=SPANNER_EMULATOR_HOST"
      ",backwards_compatibility_namespace_alias=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("backwards_compatibility_namespace_alias",
                                     HasSubstr("true"))));
}

TEST(ProcessCommandLineArgs, ProcessOmitClient) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_client=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omit_client", "true")));
}

TEST(ProcessCommandLineArgs, ProcessOmitConnection) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_connection=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omit_connection", "true")));
}

TEST(ProcessCommandLineArgs, ProcessOmitStubFactory) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",omit_stub_factory=true");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("omit_stub_factory", "true")));
}

TEST(ProcessCommandLineArgs, ProcessEndpointLocationStyle) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/spanner/"
      ",endpoint_location_style=LOCATION_DEPENDENT_COMPAT");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(*result, Contains(Pair("endpoint_location_style",
                                     "LOCATION_DEPENDENT_COMPAT")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
