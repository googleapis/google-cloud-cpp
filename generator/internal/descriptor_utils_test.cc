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

#include "generator/internal/descriptor_utils.h"
#include "generator/internal/codegen_utils.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include "generator/testing/printer_mocks.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::FakeSourceTree;
using ::google::cloud::generator_testing::MockGeneratorContext;
using ::google::cloud::generator_testing::MockZeroCopyOutputStream;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::MethodDescriptor;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;

char const* const kHttpProto =
    "syntax = \"proto3\";\n"
    "package google.api;\n"
    "option cc_enable_arenas = true;\n"
    "message Http {\n"
    "  repeated HttpRule rules = 1;\n"
    "  bool fully_decode_reserved_expansion = 2;\n"
    "}\n"
    "message HttpRule {\n"
    "  string selector = 1;\n"
    "  oneof pattern {\n"
    "    string get = 2;\n"
    "    string put = 3;\n"
    "    string post = 4;\n"
    "    string delete = 5;\n"
    "    string patch = 6;\n"
    "    CustomHttpPattern custom = 8;\n"
    "  }\n"
    "  string body = 7;\n"
    "  string response_body = 12;\n"
    "  repeated HttpRule additional_bindings = 11;\n"
    "}\n"
    "message CustomHttpPattern {\n"
    "  string kind = 1;\n"
    "  string path = 2;\n"
    "}\n";

char const* const kAnnotationsProto =
    "syntax = \"proto3\";\n"
    "package google.api;\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/protobuf/descriptor.proto\";\n"
    "extend google.protobuf.MethodOptions {\n"
    "  // See `HttpRule`.\n"
    "  HttpRule http = 72295728;\n"
    "}\n";

char const* const kClientProto =
    "syntax = \"proto3\";\n"
    "package google.api;\n"
    "import \"google/protobuf/descriptor.proto\";\n"
    "extend google.protobuf.MethodOptions {\n"
    "  repeated string method_signature = 1051;\n"
    "}\n"
    "extend google.protobuf.ServiceOptions {\n"
    "  string default_host = 1049;\n"
    "  string oauth_scopes = 1050;\n"
    "  string api_version = 525000001;\n"
    "}\n";

char const* const kFrobberServiceProto =
    "syntax = \"proto3\";\n"
    "package google.cloud.frobber.v1;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  int32 number = 1;\n"
    "}\n"
    "// Leading comments about message Empty.\n"
    "message Empty {}\n"
    "// Leading comments about service FrobberService.\n"
    "// $Delimiter escapes$ $\n"
    "service FrobberService {\n"
    "  option (google.api.api_version) = \"test-api-version\";\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Bar) returns (Empty) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/{name=projects/*/instances/*/backups/*}\"\n"
    "    };\n"
    "  }\n"
    "}\n";

class CreateServiceVarsTest
    : public testing::TestWithParam<std::pair<std::string, std::string>> {
 public:
  CreateServiceVarsTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/client.proto"), kClientProto},
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/cloud/frobber/v1/frobber.proto"),
             kFrobberServiceProto}}),
        source_tree_db_(&source_tree_),
        merged_db_(&simple_db_, &source_tree_db_),
        pool_(&merged_db_, &collector_) {
    // we need descriptor.proto to be accessible by the pool
    // since our test file imports it
    FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto_);
    simple_db_.Add(file_proto_);
    service_vars_ = {};
  }

 private:
  FileDescriptorProto file_proto_;
  generator_testing::ErrorCollector collector_;
  FakeSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  DescriptorPool pool_;
  VarsDictionary service_vars_;
};

TEST_F(CreateServiceVarsTest, FilesParseSuccessfully) {
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/http.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/annotations.proto"));
  EXPECT_NE(nullptr,
            pool_.FindFileByName("google/cloud/frobber/v1/frobber.proto"));
}

TEST_F(CreateServiceVarsTest, RetryStatusCodeExpressionNotFound) {
  auto iter = service_vars_.find("retry_status_code_expression");
  EXPECT_TRUE(iter == service_vars_.end());
}

TEST_F(CreateServiceVarsTest, AdditionalGrpcHeaderPathsEmpty) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/frobber/v1/frobber.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair("product_path", "google/cloud/frobber/")});
  auto iter = service_vars_.find("additional_pb_header_paths");
  EXPECT_TRUE(iter != service_vars_.end());
  EXPECT_THAT(iter->second, Eq(""));
}

TEST_F(CreateServiceVarsTest, ForwardingHeaderPaths) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/frobber/v1/frobber.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair("product_path", "google/cloud/frobber/v1/"),
       std::make_pair("forwarding_product_path", "google/cloud/frobber/")});
  EXPECT_THAT(
      service_vars_,
      AllOf(Contains(Pair("forwarding_client_header_path",
                          "google/cloud/frobber/frobber_client.h")),
            Contains(Pair("forwarding_connection_header_path",
                          "google/cloud/frobber/frobber_connection.h")),
            Contains(Pair("forwarding_idempotency_policy_header_path",
                          "google/cloud/frobber/"
                          "frobber_connection_idempotency_policy.h")),
            Contains(
                Pair("forwarding_mock_connection_header_path",
                     "google/cloud/frobber/mocks/mock_frobber_connection.h")),
            Contains(Pair("forwarding_options_header_path",
                          "google/cloud/frobber/frobber_options.h"))));
  EXPECT_THAT(
      service_vars_,
      AllOf(Contains(Pair("client_header_path",
                          "google/cloud/frobber/v1/frobber_client.h")),
            Contains(Pair("connection_header_path",
                          "google/cloud/frobber/v1/frobber_connection.h")),
            Contains(Pair("idempotency_policy_header_path",
                          "google/cloud/frobber/v1/"
                          "frobber_connection_idempotency_policy.h")),
            Contains(Pair(
                "mock_connection_header_path",
                "google/cloud/frobber/v1/mocks/mock_frobber_connection.h")),
            Contains(Pair("options_header_path",
                          "google/cloud/frobber/v1/frobber_options.h"))));
}

TEST_P(CreateServiceVarsTest, KeySetCorrectly) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/frobber/v1/frobber.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair("product_path", "google/cloud/frobber/"),
       std::make_pair("additional_proto_files",
                      "google/cloud/add1.proto,google/cloud/add2.proto")});
  auto iter = service_vars_.find(GetParam().first);
  EXPECT_TRUE(iter != service_vars_.end());
  EXPECT_THAT(iter->second, HasSubstr(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(
    ServiceVars, CreateServiceVarsTest,
    testing::Values(
        std::make_pair("product_options_page", "google-cloud-frobber-options"),
        std::make_pair("additional_pb_header_paths",
                       "google/cloud/add1.pb.h,google/cloud/add2.pb.h"),
        std::make_pair("api_version", "test-api-version"),
        std::make_pair("class_comment_block",
                       "///\n/// Leading comments about service "
                       "FrobberService.\n/// $Delimiter escapes$ $\n///"),
        std::make_pair("client_class_name", "FrobberServiceClient"),
        std::make_pair("client_cc_path",
                       "google/cloud/frobber/"
                       "frobber_client.cc"),
        std::make_pair("client_header_path",
                       "google/cloud/frobber/"
                       "frobber_client.h"),
        std::make_pair("connection_class_name", "FrobberServiceConnection"),
        std::make_pair("connection_cc_path",
                       "google/cloud/frobber/"
                       "frobber_connection.cc"),
        std::make_pair("connection_header_path",
                       "google/cloud/frobber/"
                       "frobber_connection.h"),
        std::make_pair("connection_rest_cc_path",
                       "google/cloud/frobber/"
                       "frobber_rest_connection.cc"),
        std::make_pair("connection_rest_header_path",
                       "google/cloud/frobber/"
                       "frobber_rest_connection.h"),
        std::make_pair("connection_options_name",
                       "FrobberServiceConnectionOptions"),
        std::make_pair("connection_options_traits_name",
                       "FrobberServiceConnectionOptionsTraits"),
        std::make_pair("grpc_service",
                       "google.cloud.frobber.v1.FrobberService"),
        std::make_pair("grpc_stub_fqn",
                       "google::cloud::frobber::v1::FrobberService"),
        std::make_pair("idempotency_class_name",
                       "FrobberServiceConnectionIdempotencyPolicy"),
        std::make_pair("idempotency_policy_cc_path",
                       "google/cloud/frobber/"
                       "frobber_connection_idempotency_policy.cc"),
        std::make_pair("idempotency_policy_header_path",
                       "google/cloud/frobber/"
                       "frobber_connection_idempotency_policy.h"),
        std::make_pair("limited_error_count_retry_policy_name",
                       "FrobberServiceLimitedErrorCountRetryPolicy"),
        std::make_pair("limited_time_retry_policy_name",
                       "FrobberServiceLimitedTimeRetryPolicy"),
        std::make_pair("logging_class_name", "FrobberServiceLogging"),
        std::make_pair("logging_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_logging_decorator.cc"),
        std::make_pair("logging_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_logging_decorator.h"),
        std::make_pair("metadata_class_name", "FrobberServiceMetadata"),
        std::make_pair("metadata_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_metadata_decorator.cc"),
        std::make_pair("metadata_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_metadata_decorator.h"),
        std::make_pair("mock_connection_class_name",
                       "MockFrobberServiceConnection"),
        std::make_pair("mock_connection_header_path",
                       "google/cloud/frobber/mocks/"
                       "mock_frobber_connection.h"),
        std::make_pair("option_defaults_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_option_defaults.cc"),
        std::make_pair("option_defaults_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_option_defaults.h"),
        std::make_pair("options_header_path",
                       "google/cloud/frobber/frobber_options.h"),
        std::make_pair("product_namespace", "frobber"),
        std::make_pair("product_internal_namespace", "frobber_internal"),
        std::make_pair("proto_file_name",
                       "google/cloud/frobber/v1/frobber.proto"),
        std::make_pair("proto_grpc_header_path",
                       "google/cloud/frobber/v1/frobber.grpc.pb.h"),
        std::make_pair("retry_policy_name", "FrobberServiceRetryPolicy"),
        std::make_pair("retry_traits_name", "FrobberServiceRetryTraits"),
        std::make_pair("retry_traits_header_path",
                       "google/cloud/frobber/internal/frobber_retry_traits.h"),
        std::make_pair("service_endpoint", ""),
        std::make_pair("service_endpoint_env_var",
                       "GOOGLE_CLOUD_CPP_FROBBER_SERVICE_ENDPOINT"),
        std::make_pair("service_name", "FrobberService"),
        std::make_pair("sources_cc_path",
                       "google/cloud/frobber/internal/frobber_sources.cc"),
        std::make_pair("streaming_cc_path",
                       "google/cloud/frobber/internal/frobber_streaming.cc"),
        std::make_pair("stub_class_name", "FrobberServiceStub"),
        std::make_pair("stub_cc_path",
                       "google/cloud/frobber/internal/frobber_stub.cc"),
        std::make_pair("stub_header_path",
                       "google/cloud/frobber/internal/frobber_stub.h"),
        std::make_pair("stub_factory_cc_path",
                       "google/cloud/frobber/internal/frobber_stub_factory.cc"),
        std::make_pair("stub_factory_header_path",
                       "google/cloud/frobber/internal/frobber_stub_factory.h"),
        std::make_pair("tracing_connection_class_name",
                       "FrobberServiceTracingConnection"),
        std::make_pair(
            "tracing_connection_cc_path",
            "google/cloud/frobber/internal/frobber_tracing_connection.cc"),
        std::make_pair(
            "tracing_connection_header_path",
            "google/cloud/frobber/internal/frobber_tracing_connection.h"),
        std::make_pair("tracing_stub_class_name", "FrobberServiceTracingStub"),
        std::make_pair("tracing_stub_cc_path",
                       "google/cloud/frobber/internal/frobber_tracing_stub.cc"),
        std::make_pair("tracing_stub_header_path",
                       "google/cloud/frobber/internal/frobber_tracing_stub.h")),
    [](testing::TestParamInfo<CreateServiceVarsTest::ParamType> const& info) {
      return std::get<0>(info.param);
    });

using CreateServiceNameMappingTest = CreateServiceVarsTest;

TEST_P(CreateServiceNameMappingTest, KeySetCorrectly) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/frobber/v1/frobber.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair("product_path", "google/cloud/frobber/"),
       std::make_pair("additional_proto_files",
                      "google/cloud/add1.proto,google/cloud/add2.proto"),
       std::make_pair("service_name_mappings",
                      "FrobberService=NewFrobberService")});
  auto iter = service_vars_.find(GetParam().first);
  EXPECT_TRUE(iter != service_vars_.end());
  EXPECT_THAT(iter->second, HasSubstr(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(
    ServiceVars, CreateServiceNameMappingTest,
    testing::Values(
        std::make_pair("product_options_page", "google-cloud-frobber-options"),
        std::make_pair("additional_pb_header_paths",
                       "google/cloud/add1.pb.h,google/cloud/add2.pb.h"),
        std::make_pair("class_comment_block",
                       "///\n/// Leading comments about service "
                       "NewFrobberService.\n/// $Delimiter escapes$ $\n///"),
        std::make_pair("client_class_name", "NewFrobberServiceClient"),
        std::make_pair("client_cc_path",
                       "google/cloud/frobber/new_frobber_client.cc"),
        std::make_pair("client_header_path",
                       "google/cloud/frobber/"
                       "new_frobber_client.h"),
        std::make_pair("connection_class_name", "NewFrobberServiceConnection"),
        std::make_pair("connection_cc_path",
                       "google/cloud/frobber/new_frobber_connection.cc"),
        std::make_pair("connection_header_path",
                       "google/cloud/frobber/new_frobber_connection.h"),
        std::make_pair("connection_rest_cc_path",
                       "google/cloud/frobber/new_frobber_rest_connection.cc"),
        std::make_pair("connection_rest_header_path",
                       "google/cloud/frobber/new_frobber_rest_connection.h"),
        std::make_pair("connection_options_name",
                       "NewFrobberServiceConnectionOptions"),
        std::make_pair("connection_options_traits_name",
                       "NewFrobberServiceConnectionOptionsTraits"),
        // The grpc service uses the full descriptor name so it does not
        // change.
        std::make_pair("grpc_service",
                       "google.cloud.frobber.v1.FrobberService"),
        std::make_pair("grpc_stub_fqn",
                       "google::cloud::frobber::v1::FrobberService"),
        std::make_pair("idempotency_class_name",
                       "NewFrobberServiceConnectionIdempotencyPolicy"),
        std::make_pair("idempotency_policy_cc_path",
                       "google/cloud/frobber/"
                       "new_frobber_connection_idempotency_policy.cc"),
        std::make_pair("idempotency_policy_header_path",
                       "google/cloud/frobber/"
                       "new_frobber_connection_idempotency_policy.h"),
        std::make_pair("limited_error_count_retry_policy_name",
                       "NewFrobberServiceLimitedErrorCountRetryPolicy"),
        std::make_pair("limited_time_retry_policy_name",
                       "NewFrobberServiceLimitedTimeRetryPolicy"),
        std::make_pair("logging_class_name", "NewFrobberServiceLogging"),
        std::make_pair("logging_cc_path",
                       "google/cloud/frobber/internal/"
                       "new_frobber_logging_decorator.cc"),
        std::make_pair("logging_header_path",
                       "google/cloud/frobber/internal/"
                       "new_frobber_logging_decorator.h"),
        std::make_pair("metadata_class_name", "NewFrobberServiceMetadata"),
        std::make_pair("metadata_cc_path",
                       "google/cloud/frobber/internal/"
                       "new_frobber_metadata_decorator.cc"),
        std::make_pair("metadata_header_path",
                       "google/cloud/frobber/internal/"
                       "new_frobber_metadata_decorator.h"),
        std::make_pair("mock_connection_class_name",
                       "MockNewFrobberServiceConnection"),
        std::make_pair("mock_connection_header_path",
                       "google/cloud/frobber/mocks/"
                       "mock_new_frobber_connection.h"),
        std::make_pair("option_defaults_cc_path",
                       "google/cloud/frobber/internal/"
                       "new_frobber_option_defaults.cc"),
        std::make_pair("option_defaults_header_path",
                       "google/cloud/frobber/internal/"
                       "new_frobber_option_defaults.h"),
        std::make_pair("options_header_path",
                       "google/cloud/frobber/new_frobber_options.h"),
        std::make_pair("product_namespace", "frobber"),
        // The namespace does not use the mapping.
        std::make_pair("product_internal_namespace", "frobber_internal"),
        std::make_pair("proto_file_name",
                       "google/cloud/frobber/v1/frobber.proto"),
        std::make_pair("proto_grpc_header_path",
                       "google/cloud/frobber/v1/frobber.grpc.pb.h"),
        std::make_pair("retry_policy_name", "NewFrobberServiceRetryPolicy"),
        std::make_pair("retry_traits_name", "NewFrobberServiceRetryTraits"),
        std::make_pair(
            "retry_traits_header_path",
            "google/cloud/frobber/internal/new_frobber_retry_traits.h"),
        std::make_pair("service_endpoint", ""),
        // This uses the same endpoint variable as the existing service.
        std::make_pair("service_endpoint_env_var",
                       "GOOGLE_CLOUD_CPP_FROBBER_SERVICE_ENDPOINT"),
        std::make_pair("service_name", "NewFrobberService"),
        std::make_pair("sources_cc_path",
                       "google/cloud/frobber/internal/new_frobber_sources.cc"),
        std::make_pair("stub_class_name", "NewFrobberServiceStub"),
        std::make_pair("stub_cc_path",
                       "google/cloud/frobber/internal/new_frobber_stub.cc"),
        std::make_pair("stub_header_path",
                       "google/cloud/frobber/internal/new_frobber_stub.h"),
        std::make_pair(
            "stub_factory_cc_path",
            "google/cloud/frobber/internal/new_frobber_stub_factory.cc"),
        std::make_pair(
            "stub_factory_header_path",
            "google/cloud/frobber/internal/new_frobber_stub_factory.h"),
        std::make_pair("tracing_connection_class_name",
                       "NewFrobberServiceTracingConnection"),
        std::make_pair(
            "tracing_connection_cc_path",
            "google/cloud/frobber/internal/new_frobber_tracing_connection.cc"),
        std::make_pair(
            "tracing_connection_header_path",
            "google/cloud/frobber/internal/new_frobber_tracing_connection.h"),
        std::make_pair("tracing_stub_class_name",
                       "NewFrobberServiceTracingStub"),
        std::make_pair(
            "tracing_stub_cc_path",
            "google/cloud/frobber/internal/new_frobber_tracing_stub.cc"),
        std::make_pair(
            "tracing_stub_header_path",
            "google/cloud/frobber/internal/new_frobber_tracing_stub.h")),
    [](testing::TestParamInfo<CreateServiceVarsTest::ParamType> const& info) {
      return std::get<0>(info.param);
    });

using CreateServiceNameToCommentMappingTest = CreateServiceVarsTest;

TEST_P(CreateServiceNameToCommentMappingTest, KeySetCorrectly) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/frobber/v1/frobber.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair("product_path", "google/cloud/frobber/"),
       std::make_pair("additional_proto_files",
                      "google/cloud/add1.proto,google/cloud/add2.proto"),
       std::make_pair("service_name_to_comments",
                      "FrobberService= New leading comments about service "
                      "FrobberService.\n")});
  auto iter = service_vars_.find(GetParam().first);
  EXPECT_TRUE(iter != service_vars_.end());
  EXPECT_THAT(iter->second, HasSubstr(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(
    ServiceVars, CreateServiceNameToCommentMappingTest,
    testing::Values(
        std::make_pair("product_options_page", "google-cloud-frobber-options"),
        std::make_pair("additional_pb_header_paths",
                       "google/cloud/add1.pb.h,google/cloud/add2.pb.h"),
        // Only field that should be modified.
        std::make_pair("class_comment_block",
                       "///\n/// New leading comments about service "
                       "FrobberService.\n"),
        std::make_pair("client_class_name", "FrobberServiceClient"),
        std::make_pair("client_cc_path",
                       "google/cloud/frobber/"
                       "frobber_client.cc"),
        std::make_pair("client_header_path",
                       "google/cloud/frobber/"
                       "frobber_client.h"),
        std::make_pair("connection_class_name", "FrobberServiceConnection"),
        std::make_pair("connection_cc_path",
                       "google/cloud/frobber/"
                       "frobber_connection.cc"),
        std::make_pair("connection_header_path",
                       "google/cloud/frobber/"
                       "frobber_connection.h"),
        std::make_pair("connection_rest_cc_path",
                       "google/cloud/frobber/"
                       "frobber_rest_connection.cc"),
        std::make_pair("connection_rest_header_path",
                       "google/cloud/frobber/"
                       "frobber_rest_connection.h"),
        std::make_pair("connection_options_name",
                       "FrobberServiceConnectionOptions"),
        std::make_pair("connection_options_traits_name",
                       "FrobberServiceConnectionOptionsTraits"),
        std::make_pair("grpc_service",
                       "google.cloud.frobber.v1.FrobberService"),
        std::make_pair("grpc_stub_fqn",
                       "google::cloud::frobber::v1::FrobberService"),
        std::make_pair("idempotency_class_name",
                       "FrobberServiceConnectionIdempotencyPolicy"),
        std::make_pair("idempotency_policy_cc_path",
                       "google/cloud/frobber/"
                       "frobber_connection_idempotency_policy.cc"),
        std::make_pair("idempotency_policy_header_path",
                       "google/cloud/frobber/"
                       "frobber_connection_idempotency_policy.h"),
        std::make_pair("limited_error_count_retry_policy_name",
                       "FrobberServiceLimitedErrorCountRetryPolicy"),
        std::make_pair("limited_time_retry_policy_name",
                       "FrobberServiceLimitedTimeRetryPolicy"),
        std::make_pair("logging_class_name", "FrobberServiceLogging"),
        std::make_pair("logging_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_logging_decorator.cc"),
        std::make_pair("logging_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_logging_decorator.h"),
        std::make_pair("metadata_class_name", "FrobberServiceMetadata"),
        std::make_pair("metadata_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_metadata_decorator.cc"),
        std::make_pair("metadata_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_metadata_decorator.h"),
        std::make_pair("mock_connection_class_name",
                       "MockFrobberServiceConnection"),
        std::make_pair("mock_connection_header_path",
                       "google/cloud/frobber/mocks/"
                       "mock_frobber_connection.h"),
        std::make_pair("option_defaults_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_option_defaults.cc"),
        std::make_pair("option_defaults_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_option_defaults.h"),
        std::make_pair("options_header_path",
                       "google/cloud/frobber/frobber_options.h"),
        std::make_pair("product_namespace", "frobber"),
        std::make_pair("product_internal_namespace", "frobber_internal"),
        std::make_pair("proto_file_name",
                       "google/cloud/frobber/v1/frobber.proto"),
        std::make_pair("proto_grpc_header_path",
                       "google/cloud/frobber/v1/frobber.grpc.pb.h"),
        std::make_pair("retry_policy_name", "FrobberServiceRetryPolicy"),
        std::make_pair("retry_traits_name", "FrobberServiceRetryTraits"),
        std::make_pair("retry_traits_header_path",
                       "google/cloud/frobber/internal/frobber_retry_traits.h"),
        std::make_pair("service_endpoint", ""),
        std::make_pair("service_endpoint_env_var",
                       "GOOGLE_CLOUD_CPP_FROBBER_SERVICE_ENDPOINT"),
        std::make_pair("service_name", "FrobberService"),
        std::make_pair("sources_cc_path",
                       "google/cloud/frobber/internal/frobber_sources.cc"),
        std::make_pair("stub_class_name", "FrobberServiceStub"),
        std::make_pair("stub_cc_path",
                       "google/cloud/frobber/internal/frobber_stub.cc"),
        std::make_pair("stub_header_path",
                       "google/cloud/frobber/internal/frobber_stub.h"),
        std::make_pair("stub_factory_cc_path",
                       "google/cloud/frobber/internal/frobber_stub_factory.cc"),
        std::make_pair("stub_factory_header_path",
                       "google/cloud/frobber/internal/frobber_stub_factory.h"),
        std::make_pair("tracing_connection_class_name",
                       "FrobberServiceTracingConnection"),
        std::make_pair(
            "tracing_connection_cc_path",
            "google/cloud/frobber/internal/frobber_tracing_connection.cc"),
        std::make_pair(
            "tracing_connection_header_path",
            "google/cloud/frobber/internal/frobber_tracing_connection.h"),
        std::make_pair("tracing_stub_class_name", "FrobberServiceTracingStub"),
        std::make_pair("tracing_stub_cc_path",
                       "google/cloud/frobber/internal/frobber_tracing_stub.cc"),
        std::make_pair("tracing_stub_header_path",
                       "google/cloud/frobber/internal/frobber_tracing_stub.h")),
    [](testing::TestParamInfo<CreateServiceVarsTest::ParamType> const& info) {
      return std::get<0>(info.param);
    });

char const* const kIamProto =
    "syntax = \"proto3\";\n"
    "package google.iam.v1;\n"
    "message Policy {}\n"
    "message GetIamPolicyRequest {}\n"
    "message TestIamPermissionsRequest {}\n"
    "message TestIamPermissionsResponse {}\n";

char const* const kLongrunningOperationsProto =
    "syntax = \"proto3\";\n"
    "package google.longrunning;\n"
    "import \"google/protobuf/descriptor.proto\";\n"
    "extend google.protobuf.MethodOptions {\n"
    "  google.longrunning.OperationInfo operation_info = 1049;\n"
    "}\n"
    "message Operation {\n"
    "  string blah = 1;\n"
    "}\n"
    "message OperationInfo {\n"
    "  string response_type = 1;\n"
    "  string metadata_type = 2;\n"
    "}\n";

char const* const kSourceLocationTestInput =
    "syntax = \"proto3\";\n"
    "import \"google/api/annotations.proto\";\n"
    "message A {\n"
    "  int32 a = 1;\n"
    "}\n"
    "message B {\n"
    "  int32 b = 1;\n"
    "}\n"
    "service S {\n"
    "  rpc Method(A) returns (B) {\n"
    "    option (google.api.http) = {\n"
    "      get: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "    };\n"
    "  }\n"
    "  rpc OtherMethod(A) returns (A) {\n"
    "    option (google.api.http) = {\n"
    "      get: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "    };\n"
    "  }\n"
    "}\n";

char const* const kWellKnownProto = R"""(
syntax = "proto3";
package google.protobuf;
// Leading comments about message Empty.
message Empty {}
)""";

auto constexpr kFieldInfoProto = R"""(
syntax = "proto3";
package google.api;
import "google/protobuf/descriptor.proto";

extend google.protobuf.FieldOptions {
  google.api.FieldInfo field_info = 291403980;
}
message FieldInfo {
  enum Format {
    FORMAT_UNSPECIFIED = 0;
    UUID4 = 1;
    IPV4 = 2;
    IPV6 = 3;
    IPV4_OR_IPV6 = 4;
  }
  Format format = 1;
}
)""";

auto constexpr kServiceProto =
    "syntax = \"proto3\";\n"
    "package my.service.v1;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/field_info.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/iam/v1/fake_iam.proto\";\n"
    "import \"google/protobuf/well_known.proto\";\n"
    "import \"google/longrunning/operation.proto\";\n"
    "// Leading comments about message Foo.\n"
    "message Foo {\n"
    "  // name field$ comment.\n"
    "  string name = 1;\n"
    "  // labels $field comment.\n"
    "  map<string, string> labels = 2;\n"
    "  string not_used_anymore = 3 [deprecated = true];\n"
    "}\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  enum SwallowType {\n"
    "    I_DONT_KNOW = 0;\n"
    "    AFRICAN = 1;\n"
    "    EUROPEAN = 2;\n"
    "  }\n"
    "  int32 number = 1;\n"
    "  string name = 2;\n"
    "  Foo widget = 3;\n"
    "  bool toggle = 4;\n"
    "  string title = 5;\n"
    "  repeated SwallowType swallow_types = 6;\n"
    "  string parent = 7;\n"
    "}\n"
    "// Leading comments about message PaginatedInput.\n"
    "message PaginatedInput {\n"
    "  int32 page_size = 1;\n"
    "  string page_token = 2;\n"
    "  string name = 3;\n"
    "}\n"
    "// Leading comments about message PaginatedOutput.\n"
    "message PaginatedOutput {\n"
    "  string next_page_token = 1;\n"
    "  repeated Bar repeated_field = 2;\n"
    "}\n"
    "message Namespace {\n"
    "  string name = 1;\n"
    "}\n"
    "message NamespaceRequest {\n"
    "  // namespace $field comment.\n"
    "  Namespace namespace = 1;\n"
    "}\n"
    "// Leading comments about message Baz.\n"
    "message Baz {\n"
    "  string project = 1;\n"
    "  string instance = 2;\n"
    "  Foo foo_resource = 3 [json_name=\"__json_request_body\"];\n"
    "}\n"
    "// Leading comments about service Service.\n"
    "service Service {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Bar) returns (google.protobuf.Empty) {\n"
    "  }\n"
    "  // Leading comments about rpc Method1.\n"
    "  rpc Method1(Bar) returns (Bar) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/{name=projects/*/instances/*/backups/*}\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method2.\n"
    "  rpc Method2(Bar) returns (google.longrunning.Operation) {\n"
    "    option (google.api.http) = {\n"
    "       patch: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "    option (google.longrunning.operation_info) = {\n"
    "      response_type: \"my.service.v1.Bar\"\n"
    "      metadata_type: \"google.protobuf.Method2Metadata\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method3.\n"
    "  rpc Method3(Bar) returns (google.longrunning.Operation) {\n"
    "    option (google.api.http) = {\n"
    "       put: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "    };\n"
    "    option (google.longrunning.operation_info) = {\n"
    "      response_type: \"google.protobuf.Empty\"\n"
    "      metadata_type: \"google.protobuf.Struct\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method4.\n"
    "  rpc Method4(PaginatedInput) returns (PaginatedOutput) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/{name=projects/*/instances/*/backups/*}\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method5.\n"
    "  rpc Method5(Bar) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "    option (google.api.method_signature) = \"name\";\n"
    "    option (google.api.method_signature) = \"number, widget\";\n"
    "    option (google.api.method_signature) = \"toggle\";\n"
    "    option (google.api.method_signature) = \"name,title\";\n"
    "    option (google.api.method_signature) = \"name,swallow_types\";\n"
    "    option (google.api.method_signature) = \"\";\n"
    "  }\n"
    "  // Leading comments about rpc Method6.\n"
    "  rpc Method6(Foo) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       get: \"/v1/{name=projects/*/instances/*/databases/*}\"\n"
    "    };\n"
    "    option (google.api.method_signature) = \"name,not_used_anymore\";\n"
    "    option (google.api.method_signature) = \"labels\";\n"
    "    option (google.api.method_signature) = \"not_used_anymore,labels\";\n"
    "    option (google.api.method_signature) = \"name,labels\";\n"
    "  }\n"
    "  // Leading comments about rpc Method7.\n"
    "  rpc Method7(Bar) returns (google.longrunning.Operation) {\n"
    "    option (google.api.http) = {\n"
    "       patch: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "    option (google.longrunning.operation_info) = {\n"
    "      response_type: \"Bar\"\n"
    "      metadata_type: \"google.protobuf.Method2Metadata\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method8.\n"
    "  rpc Method8(NamespaceRequest) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "      patch: "
    "\"/v1/{namespace.name=projects/*/locations/*/namespaces/*}\"\n"
    "      body: \"namespace\"\n"
    "    };\n"
    "    option (google.api.method_signature) = \"namespace\";\n"
    "  }\n"
    "  // Leading comments about rpc Method9.\n"
    "  rpc Method9(PaginatedInput) returns (PaginatedOutput) {\n"
    "    option (google.api.http) = {\n"
    "       get: \"/v1/foo\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method10.\n"
    "  rpc Method10(Bar) returns (google.protobuf.Empty) {\n"
    "    option (google.api.method_signature) = \"name\";\n"
    "    option (google.api.method_signature) = \"parent\";\n"
    "    option (google.api.method_signature) = \"name,parent,number\";\n"
    "    option (google.api.method_signature) = \"name,title,number\";\n"
    "  }\n"
    "  // Leading comments about rpc Method11.\n"
    "  rpc Method11(Baz) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: "
    "\"/v1/projects/{project=project}/instances/{instance=instance}/"
    "databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    "  // Test that the method defaults to kIdempotent.\n"
    "  rpc GetIamPolicy(google.iam.v1.GetIamPolicyRequest)\n"
    "      returns (google.iam.v1.Policy) {\n"
    "    option (google.api.http) = {\n"
    "       post: \"/v1/foo\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    "  // Test that the method defaults to kIdempotent.\n"
    "  rpc TestIamPermissions(google.iam.v1.TestIamPermissionsRequest)\n"
    "      returns (google.iam.v1.TestIamPermissionsResponse) {\n"
    "    option (google.api.http) = {\n"
    "       post: \"/v1/foo\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    // Continue in raw string format.
    R"""(
  rpc WithRequestId(WithRequestIdRequest) returns (google.protobuf.Empty) {}
  rpc WithoutRequestId(WithoutRequestIdRequest) returns (google.protobuf.Empty) {}
}

message WithRequestIdRequest {
  string field = 1 [ (google.api.field_info).format = UUID4 ];
}

message WithoutRequestIdRequest {
  string field = 1;
}
)""";

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

auto constexpr kHttpServiceProto = R"""(
syntax = "proto3";
package google.protobuf;
import "google/api/annotations.proto";
import "google/api/client.proto";
import "google/api/http.proto";
import "google/cloud/extended_operations.proto";
// Leading comments about message Bar.
message Bar {
  int32 number = 1;
  string name = 2;
}
// Leading comments about message Operation.
message Operation {}
// Leading comments about service Service.
service Service {
  // Leading comments about rpc Method0.
  rpc Method0(Bar) returns (Operation) {
    option (google.api.http) = {
       patch: "/v1/{parent=projects/*/instances/*}/databases"
       body: "*"
    };
    option (google.cloud.operation_service) = "ZoneOperations";
  }
}
)""";

char const* const kMethod6Deprecated1 =  // name,not_used_anymore
    "Method6(std::string const&, std::string const&)";
char const* const kMethod6Deprecated2 =  // not_used_anymore,labels
    "Method6(std::string const&, std::map<std::string, std::string> const&)";

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
 public:
  CreateMethodVarsTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/client.proto"), kClientProto},
            {std::string("google/api/field_info.proto"), kFieldInfoProto},
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/iam/v1/fake_iam.proto"), kIamProto},
            {std::string("google/longrunning/operation.proto"),
             kLongrunningOperationsProto},
            {std::string("google/cloud/extended_operations.proto"),
             kExtendedOperationsProto},
            {std::string("test/test.proto"), kSourceLocationTestInput},
            {std::string("google/protobuf/well_known.proto"), kWellKnownProto},
            {std::string("google/foo/v1/service.proto"), kServiceProto},
            {std::string("google/foo/v1/http_service.proto"),
             kHttpServiceProto}}),
        source_tree_db_(&source_tree_),
        merged_db_(&simple_db_, &source_tree_db_),
        pool_(&merged_db_, &collector_) {
    // we need descriptor.proto to be accessible by the pool
    // since our test file imports it
    FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto_);
    simple_db_.Add(file_proto_);
    service_vars_ = {};
  }

 private:
  FileDescriptorProto file_proto_;
  generator_testing::ErrorCollector collector_;
  FakeSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  DescriptorPool pool_;
  std::map<std::string, VarsDictionary> vars_;
  VarsDictionary service_vars_;
};

TEST_F(CreateMethodVarsTest, FilesParseSuccessfully) {
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/client.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/http.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/annotations.proto"));
  EXPECT_NE(nullptr,
            pool_.FindFileByName("google/longrunning/operation.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("test/test.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/foo/v1/service.proto"));
}

TEST_F(CreateMethodVarsTest, FormatMethodCommentsProtobufRequest) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");

  auto const actual = FormatMethodCommentsProtobufRequest(
      *service_file_descriptor->service(0)->method(0), false);
  EXPECT_EQ(actual, R"""(  // clang-format off
  ///
  /// Leading comments about rpc Method0.
  ///
  /// @param request Unary RPCs, such as the one wrapped by this
  ///     function, receive a single `request` proto message which includes all
  ///     the inputs for the RPC. In this case, the proto message is a
  ///     [my.service.v1.Bar].
  ///     Proto messages are converted to C++ classes by Protobuf, using the
  ///     [Protobuf mapping rules].
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return a [`Status`] object. If the request failed, the
  ///     status contains the details of the failure.
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [my.service.v1.Bar]: @googleapis_reference_link{google/foo/v1/service.proto#L19}
  ///
  // clang-format on
)""");
}

TEST_F(CreateMethodVarsTest,
       FormatMethodCommentsProtobufRequestGRPCLongRunning) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");

  auto const actual = FormatMethodCommentsProtobufRequest(
      *service_file_descriptor->service(0)->method(7), false);
  EXPECT_EQ(actual, R"""(  // clang-format off
  ///
  /// Leading comments about rpc Method7.
  ///
  /// @param request Unary RPCs, such as the one wrapped by this
  ///     function, receive a single `request` proto message which includes all
  ///     the inputs for the RPC. In this case, the proto message is a
  ///     [my.service.v1.Bar].
  ///     Proto messages are converted to C++ classes by Protobuf, using the
  ///     [Protobuf mapping rules].
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return A [`future`] that becomes satisfied when the LRO
  ///     ([Long Running Operation]) completes or the polling policy in effect
  ///     for this call is exhausted. The future is satisfied with an error if
  ///     the LRO completes with an error or the polling policy is exhausted.
  ///     In this case the [`StatusOr`] returned by the future contains the
  ///     error. If the LRO completes successfully the value of the future
  ///     contains the LRO's result. For this RPC the result is a
  ///     [$longrunning_deduced_response_message_type$] proto message.
  ///     The C++ class representing this message is created by Protobuf, using
  ///     the [Protobuf mapping rules].
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [Long Running Operation]: https://google.aip.dev/151
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [my.service.v1.Bar]: @googleapis_reference_link{google/foo/v1/service.proto#L19}
  ///
  // clang-format on
)""");
}

TEST_F(CreateMethodVarsTest,
       FormatMethodCommentsProtobufRequestHttpLongRunning) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/http_service.proto");

  auto const actual = FormatMethodCommentsProtobufRequest(
      *service_file_descriptor->service(0)->method(0), true);
  EXPECT_EQ(actual, R"""(  // clang-format off
  ///
  /// Leading comments about rpc Method0.
  ///
  /// @param request Unary RPCs, such as the one wrapped by this
  ///     function, receive a single `request` proto message which includes all
  ///     the inputs for the RPC. In this case, the proto message is a
  ///     [google.protobuf.Bar].
  ///     Proto messages are converted to C++ classes by Protobuf, using the
  ///     [Protobuf mapping rules].
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return A [`future`] that becomes satisfied when the LRO
  ///     ([Long Running Operation]) completes or the polling policy in effect
  ///     for this call is exhausted. The future is satisfied with an error if
  ///     the LRO completes with an error or the polling policy is exhausted.
  ///     In this case the [`StatusOr`] returned by the future contains the
  ///     error. If the LRO completes successfully the value of the future
  ///     contains the LRO's result. For this RPC the result is a
  ///     [$longrunning_deduced_response_message_type$] proto message.
  ///     The C++ class representing this message is created by Protobuf, using
  ///     the [Protobuf mapping rules].
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [Long Running Operation]: http://cloud/compute/docs/api/how-tos/api-requests-responses#handling_api_responses
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [google.protobuf.Bar]: @cloud_cpp_reference_link{google/foo/v1/http_service.proto#L9}
  ///
  // clang-format on
)""");
}

TEST_F(CreateMethodVarsTest, FormatMethodCommentsMethodSignature) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");

  auto const actual = FormatMethodCommentsMethodSignature(
      *service_file_descriptor->service(0)->method(6), "labels", false);
  EXPECT_EQ(actual, R"""(  // clang-format off
  ///
  /// Leading comments about rpc Method6.
  ///
  /// @param labels  labels $$field comment.
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return a [`Status`] object. If the request failed, the
  ///     status contains the details of the failure.
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [my.service.v1.Foo]: @googleapis_reference_link{google/foo/v1/service.proto#L11}
  ///
  // clang-format on
)""");
}

TEST_F(CreateMethodVarsTest, SkipMethodsWithDeprecatedFields) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair(
          "omitted_rpcs",
          absl::StrCat(SafeReplaceAll(kMethod6Deprecated1, ",", "@"), ",",
                       SafeReplaceAll(kMethod6Deprecated2, ",", "@")))});
  vars_ = CreateMethodVars(*service_file_descriptor->service(0), YAML::Node{},
                           service_vars_);
  auto method_vars = vars_.find("my.service.v1.Service.Method6");
  ASSERT_NE(method_vars, vars_.end());
  EXPECT_THAT(method_vars->second, Not(Contains(Pair("method_signature0", _))));
  EXPECT_THAT(method_vars->second, Contains(Pair("method_signature1", _)));
  EXPECT_THAT(method_vars->second, Not(Contains(Pair("method_signature2", _))));
  EXPECT_THAT(method_vars->second, Not(Contains(Pair("method_signature3", _))));
}

TEST_F(CreateMethodVarsTest, SkipMethodOverloadsWithDuplicateSignatures) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair(
          "omitted_rpcs",
          absl::StrCat(SafeReplaceAll(kMethod6Deprecated1, ",", "@"), ",",
                       SafeReplaceAll(kMethod6Deprecated2, ",", "@")))});
  vars_ = CreateMethodVars(*service_file_descriptor->service(0), YAML::Node{},
                           service_vars_);
  auto method_vars = vars_.find("my.service.v1.Service.Method10");
  ASSERT_NE(method_vars, vars_.end());
  EXPECT_THAT(method_vars->second, Contains(Pair("method_signature0", _)));
  EXPECT_THAT(method_vars->second, Not(Contains(Pair("method_signature1", _))));
  EXPECT_THAT(method_vars->second, Contains(Pair("method_signature2", _)));
  EXPECT_THAT(method_vars->second, Not(Contains(Pair("method_signature3", _))));
}

TEST_F(CreateMethodVarsTest, WithRequestId) {
  auto constexpr kServiceConfigYaml = R"""(publishing:
  method_settings:
  - selector: my.service.v1.Service.WithRequestId
    auto_populated_fields:
    - field
)""";
  auto const service_config = YAML::Load(kServiceConfigYaml);
  ASSERT_EQ(service_config.Type(), YAML::NodeType::Map);

  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair(
          "omitted_rpcs",
          absl::StrCat(SafeReplaceAll(kMethod6Deprecated1, ",", "@"), ",",
                       SafeReplaceAll(kMethod6Deprecated2, ",", "@")))});
  vars_ = CreateMethodVars(*service_file_descriptor->service(0), service_config,
                           service_vars_);
  auto mv0 = vars_.find("my.service.v1.Service.WithRequestId");
  ASSERT_NE(mv0, vars_.end());
  EXPECT_THAT(mv0->second, Contains(Pair("request_id_field_name", "field")));

  auto mv1 = vars_.find("my.service.v1.Service.WithoutRequestId");
  ASSERT_NE(mv1, vars_.end());
  EXPECT_THAT(mv1->second, Not(Contains(Pair("request_id_field_name", _))));
}

TEST_P(CreateMethodVarsTest, KeySetCorrectly) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  service_vars_ = CreateServiceVars(
      *service_file_descriptor->service(0),
      {std::make_pair(
          "omitted_rpcs",
          absl::StrCat(SafeReplaceAll(kMethod6Deprecated1, ",", "@"), ",",
                       SafeReplaceAll(kMethod6Deprecated2, ",", "@")))});
  vars_ = CreateMethodVars(*service_file_descriptor->service(0), YAML::Node{},
                           service_vars_);
  auto method_iter = vars_.find(GetParam().method);
  ASSERT_TRUE(method_iter != vars_.end());
  EXPECT_THAT(method_iter->second,
              Contains(Pair(GetParam().vars_key, GetParam().expected_value)));
}

INSTANTIATE_TEST_SUITE_P(
    MethodVars, CreateMethodVarsTest,
    testing::Values(
        // Method0
        MethodVarsTestValues("my.service.v1.Service.Method0", "method_name",
                             "Method0"),
        MethodVarsTestValues("my.service.v1.Service.Method0",
                             "method_name_snake", "method0"),
        MethodVarsTestValues("my.service.v1.Service.Method0", "request_type",
                             "my::service::v1::Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method0",
                             "response_message_type", "google.protobuf.Empty"),
        MethodVarsTestValues("my.service.v1.Service.Method0", "response_type",
                             "google::protobuf::Empty"),
        MethodVarsTestValues("my.service.v1.Service.Method0", "idempotency",
                             "kNonIdempotent"),
        // Method1
        MethodVarsTestValues("my.service.v1.Service.Method1", "method_name",
                             "Method1"),
        MethodVarsTestValues("my.service.v1.Service.Method1",
                             "method_name_snake", "method1"),
        MethodVarsTestValues("my.service.v1.Service.Method1", "request_type",
                             "my::service::v1::Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method1", "response_type",
                             "my::service::v1::Bar"),
        // Method2
        MethodVarsTestValues("my.service.v1.Service.Method2",
                             "longrunning_metadata_type",
                             "google::protobuf::Method2Metadata"),
        MethodVarsTestValues("my.service.v1.Service.Method2",
                             "longrunning_response_type",
                             "my::service::v1::Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method2",
                             "longrunning_deduced_response_message_type",
                             "my.service.v1.Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method2",
                             "longrunning_deduced_response_type",
                             "my::service::v1::Bar"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method2", "method_request_params",
            "\"parent=\", internal::UrlEncode(request.parent())"),
        MethodVarsTestValues("my.service.v1.Service.Method2", "idempotency",
                             "kNonIdempotent"),
        MethodVarsTestValues("my.service.v1.Service.Method2",
                             "method_longrunning_deduced_return_doxygen_link",
                             "@googleapis_link{my::service::v1::Bar,google/"
                             "foo/v1/service.proto#L19}"),
        MethodVarsTestValues("my.service.v1.Service.Method2",
                             "method_http_query_parameters", ""),
        // Method3
        MethodVarsTestValues("my.service.v1.Service.Method3",
                             "longrunning_metadata_type",
                             "google::protobuf::Struct"),
        MethodVarsTestValues("my.service.v1.Service.Method3",
                             "longrunning_response_type",
                             "google::protobuf::Empty"),
        MethodVarsTestValues("my.service.v1.Service.Method3",
                             "longrunning_deduced_response_type",
                             "google::protobuf::Struct"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method3", "method_request_params",
            "\"parent=\", internal::UrlEncode(request.parent())"),
        MethodVarsTestValues("my.service.v1.Service.Method3", "idempotency",
                             "kIdempotent"),
        MethodVarsTestValues("my.service.v1.Service.Method3",
                             "method_longrunning_deduced_return_doxygen_link",
                             "google::protobuf::Struct"),
        // Method4
        MethodVarsTestValues("my.service.v1.Service.Method4",
                             "range_output_field_name", "repeated_field"),
        MethodVarsTestValues("my.service.v1.Service.Method4",
                             "range_output_type", "my::service::v1::Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method4",
                             "method_request_params",
                             "\"name=\", internal::UrlEncode(request.name())"),
        MethodVarsTestValues("my.service.v1.Service.Method4", "idempotency",
                             "kNonIdempotent"),
        // Method5
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_signature0", "std::string const& name, "),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_signature1",
                             "std::int32_t number, "
                             "my::service::v1::Foo const& widget, "),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_signature2", "bool toggle, "),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_signature3",
                             "std::string const& name, "
                             "std::string const& title, "),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_signature4",
                             "std::string const& name, "
                             "std::vector<my::service::v1::Bar::SwallowType>"
                             " const& swallow_types, "),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_signature5", ""),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_request_setters0",
                             "  request.set_name(name);\n"),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_request_setters1",
                             "  request.set_number(number);\n"
                             "  *request.mutable_widget() = widget;\n"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method5", "method_request_params",
            "\"parent=\", internal::UrlEncode(request.parent())"),
        MethodVarsTestValues("my.service.v1.Service.Method5",
                             "method_request_body", "*"),
        MethodVarsTestValues("my.service.v1.Service.Method5", "idempotency",
                             "kNonIdempotent"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method5", "method_rest_path",
            R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", options), "/", request.parent(), "/", "databases"))"""),
        // Method6
        MethodVarsTestValues("my.service.v1.Service.Method6",
                             "method_request_params",
                             "\"name=\", internal::UrlEncode(request.name())"),
        MethodVarsTestValues("my.service.v1.Service.Method6", "idempotency",
                             "kIdempotent"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method6", "method_signature1",
            "std::map<std::string, std::string> const& labels, "),
        MethodVarsTestValues(
            "my.service.v1.Service.Method6", "method_request_setters1",
            "  *request.mutable_labels() = {labels.begin(), labels.end()};\n"),
        MethodVarsTestValues("my.service.v1.Service.Method6",
                             "method_http_query_parameters", ""),
        // Method7
        MethodVarsTestValues("my.service.v1.Service.Method7",
                             "longrunning_metadata_type",
                             "google::protobuf::Method2Metadata"),
        MethodVarsTestValues("my.service.v1.Service.Method7",
                             "longrunning_response_type",
                             "my::service::v1::Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method7",
                             "longrunning_deduced_response_message_type",
                             "my.service.v1.Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method7",
                             "longrunning_deduced_response_type",
                             "my::service::v1::Bar"),
        MethodVarsTestValues("my.service.v1.Service.Method7",
                             "method_longrunning_deduced_return_doxygen_link",
                             "@googleapis_link{my::service::v1::Bar,google/"
                             "foo/v1/service.proto#L19}"),
        // Method8
        MethodVarsTestValues("my.service.v1.Service.Method8",
                             "method_signature0",
                             "my::service::v1::Namespace const& namespace_, "),
        MethodVarsTestValues("my.service.v1.Service.Method8",
                             "method_request_setters0",
                             "  *request.mutable_namespace_() = namespace_;\n"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method8", "method_request_params",
            "\"namespace.name=\", "
            "internal::UrlEncode(request.namespace_().name())"),
        MethodVarsTestValues("my.service.v1.Service.Method8",
                             "request_resource", "request.namespace_()"),
        MethodVarsTestValues(
            "my.service.v1.Service.Method8", "method_rest_path",
            R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", options), "/", request.namespace_().name()))"""),
        // Method9
        MethodVarsTestValues("my.service.v1.Service.Method9",
                             "method_http_query_parameters", ""),
        // Method11
        MethodVarsTestValues("my.service.v1.Service.Method11",
                             "request_resource", "request.foo_resource()"),
        // IAM idempotency defaults
        MethodVarsTestValues("my.service.v1.Service.GetIamPolicy",
                             "idempotency", "kIdempotent"),
        MethodVarsTestValues("my.service.v1.Service.TestIamPermissions",
                             "idempotency", "kIdempotent")),
    [](testing::TestParamInfo<CreateMethodVarsTest::ParamType> const& info) {
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

  auto generator_context = std::make_unique<MockGeneratorContext>();
  auto output = std::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");

  auto result = PrintMethod(*service_file_descriptor->service(0)->method(0),
                            printer, {}, {}, "some_file", 42);
  EXPECT_THAT(result, StatusIs(Not(StatusCode::kOk),
                               HasSubstr("no matching patterns")));
}

TEST_F(PrintMethodTest, MoreThanOneMatchingPattern) {
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file_);

  auto generator_context = std::make_unique<MockGeneratorContext>();
  auto output = std::make_unique<MockZeroCopyOutputStream>();
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
  EXPECT_THAT(result, StatusIs(Not(StatusCode::kOk),
                               HasSubstr("more than one pattern")));
}

TEST_F(PrintMethodTest, ExactlyOnePattern) {
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file_);

  auto generator_context = std::make_unique<MockGeneratorContext>();
  auto output = std::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*output, Next);
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
  EXPECT_THAT(result, IsOk());
}

class FormatMethodReturnTypeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    /// @cond
    auto constexpr kServiceText = R"pb(
      name: "google/foo/v1/service.proto"
      package: "google.protobuf"
      message_type { name: "Bar" }
      message_type { name: "Empty" }
      service {
        name: "Service"
        method {
          name: "Empty"
          input_type: "google.protobuf.Bar"
          output_type: "google.protobuf.Empty"
        }
        method {
          name: "NonEmpty"
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

TEST_F(FormatMethodReturnTypeTest, EmptyReturnType) {
    DescriptorPool pool;
    FileDescriptor const* file = pool.BuildFile(service_file_);
    MethodDescriptor const* empty_return_method = file->service(0)->method(0);

    EXPECT_EQ(FormatMethodReturnType(*empty_return_method, false, false, "", ""),
    R"""(Status)""");
    EXPECT_EQ(FormatMethodReturnType(*empty_return_method, false, true, "", ""),
    R"""(Status)""");
    EXPECT_EQ(FormatMethodReturnType(*empty_return_method, true, false, "", ""),
    R"""(future<Status>)""");
    EXPECT_EQ(FormatMethodReturnType(*empty_return_method, true, true, "", ""),
    R"""(future<Status>)""");
}

TEST_F(FormatMethodReturnTypeTest, NonEmptyReturnType) {
    DescriptorPool pool;
    FileDescriptor const* file = pool.BuildFile(service_file_);
    MethodDescriptor const* nonempty_return_method = file->service(0)->method(1);

    EXPECT_EQ(FormatMethodReturnType(*nonempty_return_method, false, false, "", ""),
    R"""(StatusOr<$response_type$>)""");
    EXPECT_EQ(FormatMethodReturnType(*nonempty_return_method, false, true, "", ""),
    R"""(StatusOr<$longrunning_operation_type$>)""");
    EXPECT_EQ(FormatMethodReturnType(*nonempty_return_method, true, false, "", ""),
    R"""(future<StatusOr<$response_type$>>)""");
    EXPECT_EQ(FormatMethodReturnType(*nonempty_return_method, true, true, "", ""),
    R"""(future<StatusOr<$longrunning_deduced_response_type$>>)""");
}

TEST_F(FormatMethodReturnTypeTest, ReturnTypeWithPrefixAndSuffix) {
    DescriptorPool pool;
    FileDescriptor const* file = pool.BuildFile(service_file_);
    MethodDescriptor const* method = file->service(0)->method(0);

    EXPECT_EQ(FormatMethodReturnType(*method, false, false, "abc ", " xyz"),
    R"""(abc Status xyz)""");
    EXPECT_EQ(FormatMethodReturnType(*method, false, false, "abc\n", "\nxyz"),
    R"""(abc
Status
xyz)""");
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
