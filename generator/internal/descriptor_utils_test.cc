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
#include <google/api/client.pb.h>
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FieldDescriptorProto;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::ServiceDescriptorProto;

class CreateServiceVarsTest
    : public testing::TestWithParam<std::pair<std::string, std::string>> {
 protected:
  static void SetUpTestSuite() {
    FileDescriptorProto proto_file;
    proto_file.set_name("google/cloud/frobber/v1/frobber.proto");
    proto_file.set_package("google.cloud.frobber.v1");
    ServiceDescriptorProto* s = proto_file.add_service();
    s->set_name("FrobberService");
    DescriptorPool pool;
    const FileDescriptor* file_descriptor = pool.BuildFile(proto_file);
    vars_ = CreateServiceVars(
        *file_descriptor->service(0),
        {std::make_pair("product_path", "google/cloud/frobber/")});
  }

  static std::map<std::string, std::string> vars_;
};

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
    longrunning_file.set_name("google/longrunning/operation.proto");
    *longrunning_file.mutable_package() = "google.longrunning";
    auto operation_message = longrunning_file.add_message_type();
    operation_message->set_name("Operation");

    FileDescriptorProto service_file;
    service_file.set_name("google/foo/v1/service.proto");
    *service_file.mutable_package() = "google.protobuf";
    service_file.add_dependency("google/longrunning/operation.proto");
    service_file.add_service()->set_name("Service");

    auto input_message = service_file.add_message_type();
    input_message->set_name("Bar");
    auto int32_field = input_message->add_field();
    int32_field->set_name("number");
    int32_field->set_type(protobuf::FieldDescriptorProto_Type_TYPE_INT32);
    int32_field->set_number(1);
    auto string_field = input_message->add_field();
    string_field->set_name("name");
    string_field->set_type(protobuf::FieldDescriptorProto_Type_TYPE_STRING);
    string_field->set_number(2);
    auto message_field = input_message->add_field();
    message_field->set_name("widget");
    message_field->set_type(protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE);
    message_field->set_type_name("google.protobuf.Bar");
    message_field->set_number(3);
    auto output_message = service_file.add_message_type();
    output_message->set_name("Empty");

    // method0
    auto empty_method = service_file.mutable_service(0)->add_method();
    *empty_method->mutable_name() = "Method0";
    *empty_method->mutable_input_type() = "google.protobuf.Bar";
    *empty_method->mutable_output_type() = "google.protobuf.Empty";

    // method1
    auto non_empty_method = service_file.mutable_service(0)->add_method();
    *non_empty_method->mutable_name() = "Method1";
    *non_empty_method->mutable_input_type() = "google.protobuf.Bar";
    *non_empty_method->mutable_output_type() = "google.protobuf.Bar";

    // method2
    auto long_running_method = service_file.mutable_service(0)->add_method();
    *long_running_method->mutable_name() = "Method2";
    *long_running_method->mutable_input_type() = "google.protobuf.Bar";
    *long_running_method->mutable_output_type() =
        "google.longrunning.Operation";
    auto option = long_running_method->mutable_options()->MutableExtension(
        google::longrunning::operation_info);
    option->set_metadata_type("google.protobuf.Method2Metadata");
    option->set_response_type("google.protobuf.Method2Response");

    // method3
    auto empty_long_running_method =
        service_file.mutable_service(0)->add_method();
    *empty_long_running_method->mutable_name() = "Method3";
    *empty_long_running_method->mutable_input_type() = "google.protobuf.Bar";
    *empty_long_running_method->mutable_output_type() =
        "google.longrunning.Operation";
    option = empty_long_running_method->mutable_options()->MutableExtension(
        google::longrunning::operation_info);
    option->set_metadata_type("google.protobuf.Method2Metadata");
    option->set_response_type("google.protobuf.Empty");

    // method4
    auto paginated_input_message = service_file.add_message_type();
    paginated_input_message->set_name("PaginatedInput");
    auto page_size_field = paginated_input_message->add_field();
    page_size_field->set_name("page_size");
    page_size_field->set_type(protobuf::FieldDescriptorProto_Type_TYPE_INT32);
    page_size_field->set_number(1);

    auto page_token_field = paginated_input_message->add_field();
    page_token_field->set_name("page_token");
    page_token_field->set_type(protobuf::FieldDescriptorProto_Type_TYPE_STRING);
    page_token_field->set_number(2);

    auto paginated_output_message = service_file.add_message_type();
    paginated_output_message->set_name("PaginatedOutput");
    auto next_page_token_field = paginated_output_message->add_field();
    next_page_token_field->set_name("next_page_token");
    next_page_token_field->set_type(
        protobuf::FieldDescriptorProto_Type_TYPE_STRING);
    next_page_token_field->set_number(1);

    FieldDescriptorProto* repeated_message_field =
        paginated_output_message->add_field();
    repeated_message_field->set_name("repeated_field");
    repeated_message_field->set_type(
        protobuf::FieldDescriptorProto_Type_TYPE_MESSAGE);
    repeated_message_field->set_label(
        protobuf::FieldDescriptorProto_Label_LABEL_REPEATED);
    repeated_message_field->set_type_name("google.protobuf.Bar");
    repeated_message_field->set_number(2);

    auto paginated_method = service_file.mutable_service(0)->add_method();
    *paginated_method->mutable_name() = "Method4";
    *paginated_method->mutable_input_type() = "google.protobuf.PaginatedInput";
    *paginated_method->mutable_output_type() =
        "google.protobuf.PaginatedOutput";

    // method5
    auto signature_method = service_file.mutable_service(0)->add_method();
    *signature_method->mutable_name() = "Method5";
    *signature_method->mutable_input_type() = "google.protobuf.Bar";
    *signature_method->mutable_output_type() = "google.protobuf.Empty";
    *signature_method->mutable_options()->AddExtension(
        google::api::method_signature) = "name";
    *signature_method->mutable_options()->AddExtension(
        google::api::method_signature) = "number,widget";

    DescriptorPool pool;
    pool.BuildFile(longrunning_file);
    const FileDescriptor* service_file_descriptor =
        pool.BuildFile(service_file);
    vars_ = CreateMethodVars(*service_file_descriptor->service(0));
  }

  static std::map<std::string, std::map<std::string, std::string>> vars_;
};

std::map<std::string, std::map<std::string, std::string>>
    CreateMethodVarsTest::vars_;

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
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_metadata_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_response_type",
                             "::google::protobuf::Empty"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_deduced_response_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "range_output_field_name", "repeated_field"),
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "range_output_type", "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_signature0", "std::string const& name"),
        MethodVarsTestValues(
            "google.protobuf.Service.Method5", "method_signature1",
            "std::int32_t const& number, ::google::protobuf::Bar "
            "const& widget")),
    [](const testing::TestParamInfo<CreateMethodVarsTest::ParamType>& info) {
      std::vector<std::string> pieces = absl::StrSplit(info.param.method, '.');
      return pieces.back() + "_" + info.param.vars_key;
    });

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
