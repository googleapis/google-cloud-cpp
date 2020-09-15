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

#include "generator/internal/descriptor_utils.h"
#include "absl/strings/str_split.h"
#include "generator/testing/printer_mocks.h"
#include <google/api/client.pb.h>
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

class CreateServiceVarsTest
    : public testing::TestWithParam<std::pair<std::string, std::string>> {
 protected:
  static void SetUpTestSuite() {
    FileDescriptorProto proto_file;
    auto constexpr kText = R"pb(
      name: "google/cloud/frobber/v1/frobber.proto"
      package: "google.cloud.frobber.v1"
      service { name: "FrobberService" }
    )pb";
    ASSERT_TRUE(
        google::protobuf::TextFormat::ParseFromString(kText, &proto_file));

    DescriptorPool pool;
    const FileDescriptor* file_descriptor = pool.BuildFile(proto_file);
    vars_ = CreateServiceVars(
        *file_descriptor->service(0),
        {std::make_pair("product_path", "google/cloud/frobber/")});
  }

  static VarsDictionary vars_;
};

VarsDictionary CreateServiceVarsTest::vars_;

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

struct MethodVarsTestValues {
  MethodVarsTestValues(std::string m, std::string k, std::string v)
      : method(std::move(m)),
        vars_key(std::move(k)),
        expected_value(std::move(v)) {}
  std::string method;
  std::string vars_key;
  std::string expected_value;
};

class CreateMethodVarsTest
    : public testing::TestWithParam<MethodVarsTestValues> {
 protected:
  static void SetUpTestSuite() {
    FileDescriptorProto longrunning_file;
    auto constexpr kLongrunningText = R"pb(
      name: "google/longrunning/operation.proto"
      package: "google.longrunning"
      message_type { name: "Operation" }
    )pb";
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        kLongrunningText, &longrunning_file));

    google::protobuf::FileDescriptorProto service_file;
    /// @cond
    auto constexpr kServiceText = R"pb(
      name: "google/foo/v1/service.proto"
      package: "google.protobuf"
      dependency: "google/longrunning/operation.proto"
      message_type {
        name: "Bar"
        field { name: "number" number: 1 type: TYPE_INT32 }
        field { name: "name" number: 2 type: TYPE_STRING }
        field {
          name: "widget"
          number: 3
          type: TYPE_MESSAGE
          type_name: "google.protobuf.Bar"
        }
      }
      message_type { name: "Empty" }
      message_type {
        name: "PaginatedInput"
        field { name: "page_size" number: 1 type: TYPE_INT32 }
        field { name: "page_token" number: 2 type: TYPE_STRING }
      }
      message_type {
        name: "PaginatedOutput"
        field { name: "next_page_token" number: 1 type: TYPE_STRING }
        field {
          name: "repeated_field"
          number: 2
          label: LABEL_REPEATED
          type: TYPE_MESSAGE
          type_name: "google.protobuf.Bar"
        }
      }
      service {
        name: "Service"
        method {
          name: "Method0"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Empty"
        }
        method {
          name: "Method1"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Bar"
        }
        method {
          name: "Method2"
          input_type: "google.protobuf.Bar"
          output_type: "google.longrunning.Operation"
          options {
            [google.longrunning.operation_info] {
              response_type: "google.protobuf.Method2Response"
              metadata_type: "google.protobuf.Method2Metadata"
            }
            [google.api.http] {
              patch: "/v1/{parent=projects/*/instances/*}/databases"
            }
          }
        }
        method {
          name: "Method3"
          input_type: "google.protobuf.Bar"
          output_type: "google.longrunning.Operation"
          options {
            [google.longrunning.operation_info] {
              response_type: "google.protobuf.Empty"
              metadata_type: "google.protobuf.Method2Metadata"
            }
            [google.api.http] {
              put: "/v1/{parent=projects/*/instances/*}/databases"
            }
          }
        }
        method {
          name: "Method4"
          input_type: "google.protobuf.PaginatedInput"
          output_type: "google.protobuf.PaginatedOutput"
          options {
            [google.api.http] {
              delete: "/v1/{name=projects/*/instances/*/backups/*}"
            }
          }
        }
        method {
          name: "Method5"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Empty"
          options {
            [google.api.method_signature]: "name"
            [google.api.method_signature]: "number,widget"
            [google.api.http] {
              post: "/v1/{parent=projects/*/instances/*}/databases"
              body: "*"
            }
          }
        }
        method {
          name: "Method6"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Empty"
          options {
            [google.api.http] {
              get: "/v1/{name=projects/*/instances/*/databases/*}"
            }
          }
        }

      }
    )pb";
    /// @endcond
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                              &service_file));

    DescriptorPool pool;
    pool.BuildFile(longrunning_file);
    const FileDescriptor* service_file_descriptor =
        pool.BuildFile(service_file);
    vars_ = CreateMethodVars(*service_file_descriptor->service(0));
  }

  static std::map<std::string, VarsDictionary> vars_;
};

std::map<std::string, VarsDictionary> CreateMethodVarsTest::vars_;

TEST_P(CreateMethodVarsTest, KeySetCorrectly) {
  auto method_iter = vars_.find(GetParam().method);
  EXPECT_TRUE(method_iter != vars_.end());
  auto iter = method_iter->second.find(GetParam().vars_key);
  EXPECT_EQ(iter->second, GetParam().expected_value);
}

INSTANTIATE_TEST_SUITE_P(
    MethodVars, CreateMethodVarsTest,
    testing::Values(
        MethodVarsTestValues("google.protobuf.Service.Method0", "method_name",
                             "Method0"),
        MethodVarsTestValues("google.protobuf.Service.Method0",
                             "method_name_snake", "method0"),
        MethodVarsTestValues("google.protobuf.Service.Method0", "request_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method0", "response_type",
                             "::google::protobuf::Empty"),
        MethodVarsTestValues("google.protobuf.Service.Method1", "method_name",
                             "Method1"),
        MethodVarsTestValues("google.protobuf.Service.Method1",
                             "method_name_snake", "method1"),
        MethodVarsTestValues("google.protobuf.Service.Method1", "request_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method1", "response_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_metadata_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_response_type",
                             "::google::protobuf::Method2Response"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_deduced_response_type",
                             "::google::protobuf::Method2Response"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "method_request_param_key", "parent"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "method_request_param_value", "parent()"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "method_request_url_path",
                             "/v1/{parent=projects/*/instances/*}/databases"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "method_request_url_substitution",
                             "projects/*/instances/*"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_metadata_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_response_type",
                             "::google::protobuf::Empty"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_deduced_response_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "method_request_param_key", "parent"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "method_request_param_value", "parent()"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "method_request_url_path",
                             "/v1/{parent=projects/*/instances/*}/databases"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "method_request_url_substitution",
                             "projects/*/instances/*"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "range_output_field_name", "repeated_field"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "range_output_type", "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "method_request_param_key", "name"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "method_request_param_value", "name()"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "method_request_url_path",
                             "/v1/{name=projects/*/instances/*/backups/*}"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "method_request_url_substitution",
                             "projects/*/instances/*/backups/*"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_signature0", "std::string const& name"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_signature1",
                             "std::int32_t const& number, "
                             "::google::protobuf::Bar const& widget"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_param_key", "parent"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_param_value", "parent()"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_url_path",
                             "/v1/{parent=projects/*/instances/*}/databases"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_url_substitution",
                             "projects/*/instances/*"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_body", "*"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_param_key", "name"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_param_value", "name()"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_url_path",
                             "/v1/{name=projects/*/instances/*/databases/*}"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_url_substitution",
                             "projects/*/instances/*/databases/*")),
    [](const testing::TestParamInfo<CreateMethodVarsTest::ParamType>& info) {
      std::vector<std::string> pieces = absl::StrSplit(info.param.method, '.');
      return pieces.back() + "_" + info.param.vars_key;
    });

class PrintMethodTest : public ::testing::Test {
 protected:
  void SetUp() override {
    /// @cond
    auto constexpr kServiceText = R"pb(
      name: "google/foo/v1/service.proto"
      package: "google.protobuf"
      message_type {
        name: "Bar"
        field { name: "number" number: 1 type: TYPE_INT32 }
        field { name: "name" number: 2 type: TYPE_STRING }
        field {
          name: "widget"
          number: 3
          type: TYPE_MESSAGE
          type_name: "google.protobuf.Bar"
        }
      }
      message_type { name: "Empty" }
      service {
        name: "Service"
        method {
          name: "Method0"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Empty"
        }
        method {
          name: "Method1"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Bar"
        }
      }
    )pb";
    /// @endcond
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                              &service_file_));
  }

  google::protobuf::FileDescriptorProto service_file_;
};

TEST_F(PrintMethodTest, NoMatchingPatterns) {
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file_);

  auto generator_context =
      absl::make_unique<generator_testing::MockGeneratorContext>();
  auto output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");

  auto result = PrintMethod(*service_file_descriptor->service(0)->method(0),
                            printer, {}, {}, "some_file", 42);
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.message(), HasSubstr("no matching patterns"));
}

TEST_F(PrintMethodTest, MoreThanOneMatchingPattern) {
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file_);

  auto generator_context =
      absl::make_unique<generator_testing::MockGeneratorContext>();
  auto output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");

  auto always_matches = [](google::protobuf::MethodDescriptor const&) {
    return true;
  };

  std::vector<MethodPattern> patterns = {
      MethodPattern({{"always matches"}}, always_matches),
      MethodPattern({{"also always matches"}}, always_matches)};

  auto result = PrintMethod(*service_file_descriptor->service(0)->method(0),
                            printer, {}, patterns, "some_file", 42);
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.message(), HasSubstr("more than one pattern"));
}

TEST_F(PrintMethodTest, ExactlyOnePattern) {
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file_);

  auto generator_context =
      absl::make_unique<generator_testing::MockGeneratorContext>();
  auto output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  EXPECT_CALL(*output, Next(_, _));
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");

  auto always_matches = [](google::protobuf::MethodDescriptor const&) {
    return true;
  };
  auto never_matches = [](google::protobuf::MethodDescriptor const&) {
    return false;
  };

  std::vector<MethodPattern> patterns = {
      MethodPattern({{"matches"}}, always_matches),
      MethodPattern({{"does not match"}}, never_matches)};

  auto result = PrintMethod(*service_file_descriptor->service(0)->method(0),
                            printer, {}, patterns, "some_file", 42);
  EXPECT_TRUE(result.ok());
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
