// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/codegen_utils.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::testing::ElementsAre;

TEST(GeneratedFileSuffix, Success) {
  EXPECT_EQ(".gcpcxx.pb", GeneratedFileSuffix());
}

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
  EXPECT_EQ("foo_bar_ba", CamelCaseToSnakeCase("fooBarBa"));
  EXPECT_EQ("foo_bar_baaaaa", CamelCaseToSnakeCase("fooBarBAAAAA"));
  EXPECT_EQ("foo_bar_b", CamelCaseToSnakeCase("foo_BarB"));
  EXPECT_EQ("v1", CamelCaseToSnakeCase("v1"));
  EXPECT_EQ("", CamelCaseToSnakeCase(""));
  EXPECT_EQ(" ", CamelCaseToSnakeCase(" "));
  EXPECT_EQ("a", CamelCaseToSnakeCase("A"));
  EXPECT_EQ("a_b", CamelCaseToSnakeCase("aB"));
  EXPECT_EQ("foo123", CamelCaseToSnakeCase("Foo123"));
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
  EXPECT_EQ("::google::spanner::admin::database::v1::Request",
            ProtoNameToCppName("google.spanner.admin.database.v1.Request"));
}

TEST(BuildNamespaces, NoDirectoryPathInternal) {
  auto result = BuildNamespaces("/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "_internal"));
}

TEST(BuildNamespaces, OneDirectoryPathInternal) {
  auto result = BuildNamespaces("one/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "one_internal"));
}

TEST(BuildNamespaces, TwoDirectoryPathInternal) {
  auto result = BuildNamespaces("unusual/product/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "unusual_product_internal"));
}

TEST(BuildNamespaces, TwoDirectoryPathNotInternal) {
  auto result = BuildNamespaces("unusual/product/");
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "unusual_product"));
}

TEST(BuildNamespaces, ThreeDirectoryPathInternal) {
  auto result =
      BuildNamespaces("google/cloud/spanner/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "spanner_internal"));
}

TEST(BuildNamespaces, ThreeDirectoryPathMocks) {
  auto result = BuildNamespaces("google/cloud/spanner/", NamespaceType::kMocks);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "spanner_mocks"));
}

TEST(BuildNamespaces, ThreeDirectoryPathNotInternal) {
  auto result = BuildNamespaces("google/cloud/translation/");
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "translation"));
}

TEST(BuildNamespaces, FourDirectoryPathInternal) {
  auto result =
      BuildNamespaces("google/cloud/foo/bar/baz/", NamespaceType::kInternal);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "foo_bar_baz_internal"));
}

TEST(BuildNamespaces, FourDirectoryPathNotInternal) {
  auto result = BuildNamespaces("google/cloud/foo/bar/baz/");
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(result, ElementsAre("google", "cloud", "GOOGLE_CLOUD_CPP_NS",
                                  "foo_bar_baz"));
}

TEST(ProcessCommandLineArgs, NoProductPath) {
  auto result = ProcessCommandLineArgs("");
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(result.status().message(),
            "--cpp_codegen_opt=product_path=<path> must be specified.");
}

TEST(ProcessCommandLineArgs, EmptyProductPath) {
  auto result = ProcessCommandLineArgs("product_path=");
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(result.status().message(),
            "--cpp_codegen_opt=product_path=<path> must be specified.");
}

TEST(ProcessCommandLineArgs, ProductPathNeedsFormatting) {
  auto result = ProcessCommandLineArgs(
      "product_path=/google/cloud/pubsub,googleapis_commit_hash=foo");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->front().second, "google/cloud/pubsub/");
}

TEST(ProcessCommandLineArgs, ProductPathAlreadyFormatted) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/,googleapis_commit_hash=foo");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->front().second, "google/cloud/pubsub/");
}

TEST(ProcessCommandLineArgs, NoCommitHash) {
  auto result = ProcessCommandLineArgs("product_path=/google/cloud/bar");
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(
      result.status().message(),
      "--cpp_codegen_opt=googleapis_commit_hash=<hash> must be specified.");
}

TEST(ProcessCommandLineArgs, EmptyCommitHash) {
  auto result = ProcessCommandLineArgs(
      "product_path=/google/cloud/bar,googleapis_commit_hash=");
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(
      result.status().message(),
      "--cpp_codegen_opt=googleapis_commit_hash=<hash> must be specified.");
}

TEST(ProcessCommandLineArgs, NoCopyrightYearParameterOrValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/,googleapis_commit_hash=foo");
  auto expected_year = CurrentCopyrightYear();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->back().first, "copyright_year");
  EXPECT_EQ(result->back().second, expected_year);
}

TEST(ProcessCommandLineArgs, NoCopyrightYearValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",googleapis_commit_hash=foo,copyright_year=");
  auto expected_year = CurrentCopyrightYear();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->back().first, "copyright_year");
  EXPECT_EQ(result->back().second, expected_year);
}

TEST(ProcessCommandLineArgs, CopyrightYearWithValue) {
  auto result = ProcessCommandLineArgs(
      "product_path=google/cloud/pubsub/"
      ",googleapis_commit_hash=foo,copyright_year=1995");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->back().first, "copyright_year");
  EXPECT_EQ(result->back().second, "1995");
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
