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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

TEST(GeneratedFileSuffix, Success) {
  EXPECT_EQ(".gcpcxx.pb", GeneratedFileSuffix());
}

TEST(LocalInclude, Success) {
  EXPECT_EQ("#include \"google/cloud/status.h\"\n",
            LocalInclude("google/cloud/status.h"));
}

TEST(SystemInclude, Success) {
  EXPECT_EQ("#include <vector>\n", SystemInclude("vector"));
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

TEST(ProtoNameToCppName, Success) {
  EXPECT_EQ("::google::spanner::admin::database::v1::Request",
            ProtoNameToCppName("google.spanner.admin.database.v1.Request"));
}

TEST(BuildNamespace, EmptyVars) {
  VarsDictionary vars;
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  EXPECT_EQ(result.status().code(), StatusCode::kNotFound);
  EXPECT_EQ(result.status().message(), "product_path must be present in vars.");
}

TEST(BuildNamespace, NoTrailingSlash) {
  VarsDictionary vars;
  vars["product_path"] = "google/cloud/spanner";
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(result.status().message(), "vars[product_path] must end with '/'.");
}

TEST(BuildNamespace, ProductPathTooShort) {
  VarsDictionary vars;
  vars["product_path"] = std::string("/");
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(result.status().message(),
            "vars[product_path] contain at least 2 characters.");
}

TEST(BuildNamespaces, Internal) {
  VarsDictionary vars;
  vars["product_path"] = "google/cloud/spanner/";
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 4);
  EXPECT_EQ("google", (*result)[0]);
  EXPECT_EQ("cloud", (*result)[1]);
  EXPECT_EQ("spanner_internal", (*result)[2]);
  EXPECT_EQ("SPANNER_CLIENT_NS", (*result)[3]);
}

TEST(BuildNamespaces, NotInternal) {
  VarsDictionary vars;
  vars["product_path"] = "google/cloud/translation/";
  auto result = BuildNamespaces(vars);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 4);
  EXPECT_EQ("google", (*result)[0]);
  EXPECT_EQ("cloud", (*result)[1]);
  EXPECT_EQ("translation", (*result)[2]);
  EXPECT_EQ("TRANSLATION_CLIENT_NS", (*result)[3]);
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
  auto result = ProcessCommandLineArgs("product_path=/google/cloud/pubsub");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->front().second, "google/cloud/pubsub/");
}

TEST(ProcessCommandLineArgs, ProductPathAlreadyFormatted) {
  auto result = ProcessCommandLineArgs("product_path=google/cloud/pubsub/");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->front().second, "google/cloud/pubsub/");
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
