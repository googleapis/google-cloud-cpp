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
#include <google/protobuf/descriptor.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::ServiceDescriptorProto;

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
  std::map<std::string, std::string> vars;
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  EXPECT_EQ(result.status().code(), StatusCode::kNotFound);
  EXPECT_EQ(result.status().message(), "product_path must be present in vars.");
}

TEST(BuildNamespace, NoTrailingSlash) {
  std::map<std::string, std::string> vars;
  vars["product_path"] = "google/cloud/spanner";
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(result.status().message(), "vars[product_path] must end with '/'.");
}

TEST(BuildNamespace, ProductPathTooShort) {
  std::map<std::string, std::string> vars;
  vars["product_path"] = std::string("/");
  auto result = BuildNamespaces(vars, NamespaceType::kInternal);
  EXPECT_EQ(result.status().code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(result.status().message(),
            "vars[product_path] contain at least 2 characters.");
}

TEST(BuildNamespaces, Internal) {
  std::map<std::string, std::string> vars;
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
  std::map<std::string, std::string> vars;
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

class CreateServiceVarsTest
    : public testing::TestWithParam<std::pair<std::string, std::string>> {
 protected:
  static void SetUpTestSuite() {
    proto_file_.set_name("google/cloud/frobber/v1/frobber.proto");
    proto_file_.set_package("google.cloud.frobber.v1");
    ServiceDescriptorProto* s = proto_file_.add_service();
    s->set_name("FrobberService");
    const FileDescriptor* file_descriptor = pool_.BuildFile(proto_file_);
    vars_ = CreateServiceVars(
        *file_descriptor->service(0),
        {std::make_pair("product_path", "google/cloud/frobber/")});
  }

  static DescriptorPool pool_;
  static FileDescriptorProto proto_file_;
  static std::map<std::string, std::string> vars_;
};

DescriptorPool CreateServiceVarsTest::pool_;
FileDescriptorProto CreateServiceVarsTest::proto_file_;
std::map<std::string, std::string> CreateServiceVarsTest::vars_;

TEST_P(CreateServiceVarsTest, KeySetCorrectly) {
  auto iter = vars_.find(GetParam().first);
  EXPECT_TRUE(iter != vars_.end());
  EXPECT_EQ(iter->second, GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(
    ServiceVars, CreateServiceVarsTest,
    testing::Values(
        std::make_pair("class_comment_block", "// TODO: pull in comments"),
        std::make_pair("client_class_name", "FrobberServiceClient"),
        std::make_pair("grpc_stub_fqn",
                       "::google::cloud::frobber::v1::FrobberService"),
        std::make_pair("logging_class_name", "FrobberServiceLogging"),
        std::make_pair("metadata_class_name", "FrobberServiceMetadata"),
        std::make_pair("proto_file_name",
                       "google/cloud/frobber/v1/frobber.proto"),
        std::make_pair("service_endpoint", ""),
        std::make_pair(
            "stub_cc_path",
            "google/cloud/frobber/internal/frobber_stub.gcpcxx.pb.cc"),
        std::make_pair("stub_class_name", "FrobberServiceStub"),
        std::make_pair(
            "stub_header_path",
            "google/cloud/frobber/internal/frobber_stub.gcpcxx.pb.h")),
    [](const testing::TestParamInfo<CreateServiceVarsTest::ParamType>& info) {
      return std::get<0>(info.param);
    });
}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
