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
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include "generator/testing/printer_mocks.h"
#include <google/api/client.pb.h>
#include <google/longrunning/operations.pb.h>
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
        std::make_pair("client_class_name", "FrobberServiceClient"),
        std::make_pair("client_cc_path",
                       "google/cloud/frobber/"
                       "frobber_client.gcpcxx.pb.cc"),
        std::make_pair("client_header_path",
                       "google/cloud/frobber/"
                       "frobber_client.gcpcxx.pb.h"),
        std::make_pair("connection_class_name", "FrobberServiceConnection"),
        std::make_pair("connection_cc_path",
                       "google/cloud/frobber/"
                       "frobber_connection.gcpcxx.pb.cc"),
        std::make_pair("connection_header_path",
                       "google/cloud/frobber/"
                       "frobber_connection.gcpcxx.pb.h"),
        std::make_pair("connection_options_name",
                       "FrobberServiceConnectionOptions"),
        std::make_pair("connection_options_traits_name",
                       "FrobberServiceConnectionOptionsTraits"),
        std::make_pair("grpc_stub_fqn",
                       "::google::cloud::frobber::v1::FrobberService"),
        std::make_pair("idempotency_class_name",
                       "FrobberServiceConnectionIdempotencyPolicy"),
        std::make_pair("idempotency_policy_cc_path",
                       "google/cloud/frobber/"
                       "frobber_connection_idempotency_policy.gcpcxx.pb.cc"),
        std::make_pair("idempotency_policy_header_path",
                       "google/cloud/frobber/"
                       "frobber_connection_idempotency_policy.gcpcxx.pb.h"),
        std::make_pair("limited_error_count_retry_policy_name",
                       "FrobberServiceLimitedErrorCountRetryPolicy"),
        std::make_pair("limited_time_retry_policy_name",
                       "FrobberServiceLimitedTimeRetryPolicy"),
        std::make_pair("logging_class_name", "FrobberServiceLogging"),
        std::make_pair("logging_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_logging_decorator.gcpcxx.pb.cc"),
        std::make_pair("logging_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_logging_decorator.gcpcxx.pb.h"),
        std::make_pair("metadata_class_name", "FrobberServiceMetadata"),
        std::make_pair("metadata_cc_path",
                       "google/cloud/frobber/internal/"
                       "frobber_metadata_decorator.gcpcxx.pb.cc"),
        std::make_pair("metadata_header_path",
                       "google/cloud/frobber/internal/"
                       "frobber_metadata_decorator.gcpcxx.pb.h"),
        std::make_pair("mock_connection_class_name",
                       "MockFrobberServiceConnection"),
        std::make_pair("mock_connection_header_path",
                       "google/cloud/frobber/mocks/"
                       "mock_frobber_connection.gcpcxx.pb.h"),
        std::make_pair("product_namespace", "frobber"),
        std::make_pair("product_internal_namespace", "frobber_internal"),
        std::make_pair("proto_file_name",
                       "google/cloud/frobber/v1/frobber.proto"),
        std::make_pair("proto_grpc_header_path",
                       "google/cloud/frobber/v1/frobber.grpc.pb.h"),
        std::make_pair("retry_policy_name", "FrobberServiceRetryPolicy"),
        std::make_pair("retry_traits_name", "FrobberServiceRetryTraits"),
        std::make_pair("retry_traits_header_path",
                       "google/cloud/frobber/retry_traits.h"),
        std::make_pair("service_endpoint", ""),
        std::make_pair("stub_class_name", "FrobberServiceStub"),
        std::make_pair(
            "stub_cc_path",
            "google/cloud/frobber/internal/frobber_stub.gcpcxx.pb.cc"),
        std::make_pair(
            "stub_header_path",
            "google/cloud/frobber/internal/frobber_stub.gcpcxx.pb.h"),
        std::make_pair(
            "stub_factory_cc_path",
            "google/cloud/frobber/internal/frobber_stub_factory.gcpcxx.pb.cc"),
        std::make_pair(
            "stub_factory_header_path",
            "google/cloud/frobber/internal/frobber_stub_factory.gcpcxx.pb.h")),
    [](const testing::TestParamInfo<CreateServiceVarsTest::ParamType>& info) {
      return std::get<0>(info.param);
    });

const char* const kHttpProto =
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

const char* const kAnnotationsProto =
    "syntax = \"proto3\";\n"
    "package google.api;\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/protobuf/descriptor.proto\";\n"
    "extend google.protobuf.MethodOptions {\n"
    "  // See `HttpRule`.\n"
    "  HttpRule http = 72295728;\n"
    "}\n";

const char* const kClientProto =
    "syntax = \"proto3\";\n"
    "package google.api;\n"
    "import \"google/protobuf/descriptor.proto\";\n"
    "extend google.protobuf.MethodOptions {\n"
    "  repeated string method_signature = 1051;\n"
    "}\n"
    "extend google.protobuf.ServiceOptions {\n"
    "  string default_host = 1049;\n"
    "  string oauth_scopes = 1050;\n"
    "}\n";

const char* const kLongrunningOperationsProto =
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

const char* const kSourceLocationTestInput =
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

const char* const kServiceProto =
    "syntax = \"proto3\";\n"
    "package google.protobuf;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/longrunning/operation.proto\";\n"
    "// Leading comments about message Foo.\n"
    "message Foo {\n"
    "  string baz = 1;\n"
    "}\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  int32 number = 1;\n"
    "  string name = 2;\n"
    "  Foo widget = 3;\n"
    "  bool toggle = 4;\n"
    "  string title = 5;\n"
    "}\n"
    "// Leading comments about message Empty.\n"
    "message Empty {}\n"
    "// Leading comments about message PaginatedInput.\n"
    "message PaginatedInput {\n"
    "  int32 page_size = 1;\n"
    "  string page_token = 2;\n"
    "}\n"
    "// Leading comments about message PaginatedOutput.\n"
    "message PaginatedOutput {\n"
    "  string next_page_token = 1;\n"
    "  repeated Bar repeated_field = 2;\n"
    "}\n"
    "// Leading comments about service Service.\n"
    "service Service {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Bar) returns (Empty) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/{name=projects/*/instances/*/backups/*}\"\n"
    "    };\n"
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
    "      response_type: \"google.protobuf.Bar\"\n"
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
    "  rpc Method5(Bar) returns (Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "    option (google.api.method_signature) = \"name\";\n"
    "    option (google.api.method_signature) = \"number,widget\";\n"
    "    option (google.api.method_signature) = \"toggle\";\n"
    "    option (google.api.method_signature) = \"name,title\";\n"
    "  }\n"
    "  // Leading comments about rpc Method6.\n"
    "  rpc Method6(Bar) returns (Empty) {\n"
    "    option (google.api.http) = {\n"
    "       get: \"/v1/{name=projects/*/instances/*/databases/*}\"\n"
    "    };\n"
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
    "}\n";

class StringSourceTree : public google::protobuf::compiler::SourceTree {
 public:
  explicit StringSourceTree(std::map<std::string, std::string> files)
      : files_(std::move(files)) {}

  google::protobuf::io::ZeroCopyInputStream* Open(
      const std::string& filename) override {
    return files_.count(filename) == 1
               ? new google::protobuf::io::ArrayInputStream(
                     files_[filename].data(),
                     static_cast<int>(files_[filename].size()))
               : nullptr;
  }

 private:
  std::map<std::string, std::string> files_;
};

class AbortingErrorCollector : public DescriptorPool::ErrorCollector {
 public:
  AbortingErrorCollector() = default;
  AbortingErrorCollector(AbortingErrorCollector const&) = delete;
  AbortingErrorCollector& operator=(AbortingErrorCollector const&) = delete;

  void AddError(const std::string& filename, const std::string& element_name,
                const google::protobuf::Message*, ErrorLocation,
                const std::string& error_message) override {
    GCP_LOG(FATAL) << "AddError() called unexpectedly: " << filename << " ["
                   << element_name << "]: " << error_message;
  }
};

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
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/longrunning/operation.proto"),
             kLongrunningOperationsProto},
            {std::string("test/test.proto"), kSourceLocationTestInput},
            {std::string("google/foo/v1/service.proto"), kServiceProto}}),
        source_tree_db_(&source_tree_),
        merged_db_(&simple_db_, &source_tree_db_),
        pool_(&merged_db_, &collector_) {
    // we need descriptor.proto to be accessible by the pool
    // since our test file imports it
    FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto_);
    simple_db_.Add(file_proto_);
    service_vars_ = {{"googleapis_commit_hash", "foo"}};
  }

 private:
  FileDescriptorProto file_proto_;
  AbortingErrorCollector collector_;
  StringSourceTree source_tree_;
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

TEST_P(CreateMethodVarsTest, KeySetCorrectly) {
  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  vars_ = CreateMethodVars(*service_file_descriptor->service(0), service_vars_);
  auto method_iter = vars_.find(GetParam().method);
  EXPECT_TRUE(method_iter != vars_.end());
  auto iter = method_iter->second.find(GetParam().vars_key);
  EXPECT_EQ(iter->second, GetParam().expected_value);
}

INSTANTIATE_TEST_SUITE_P(
    MethodVars, CreateMethodVarsTest,
    testing::Values(
        // Method0
        MethodVarsTestValues("google.protobuf.Service.Method0", "method_name",
                             "Method0"),
        MethodVarsTestValues("google.protobuf.Service.Method0",
                             "method_name_snake", "method0"),
        MethodVarsTestValues("google.protobuf.Service.Method0", "request_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method0",
                             "response_message_type", "google.protobuf.Empty"),
        MethodVarsTestValues("google.protobuf.Service.Method0", "response_type",
                             "::google::protobuf::Empty"),
        MethodVarsTestValues("google.protobuf.Service.Method0",
                             "default_idempotency", "kNonIdempotent"),
        MethodVarsTestValues(
            "google.protobuf.Service.Method0", "method_return_doxygen_link",
            "[::google::protobuf::Empty](https://github.com/googleapis/"
            "googleapis/blob/foo/google/foo/v1/service.proto#L20)"),
        // Method1
        MethodVarsTestValues("google.protobuf.Service.Method1", "method_name",
                             "Method1"),
        MethodVarsTestValues("google.protobuf.Service.Method1",
                             "method_name_snake", "method1"),
        MethodVarsTestValues("google.protobuf.Service.Method1", "request_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method1", "response_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues(
            "google.protobuf.Service.Method1", "method_return_doxygen_link",
            "[::google::protobuf::Bar](https://github.com/googleapis/"
            "googleapis/blob/foo/google/foo/v1/service.proto#L12)"),
        // Method2
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_metadata_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_response_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_deduced_response_message_type",
                             "google.protobuf.Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "longrunning_deduced_response_type",
                             "::google::protobuf::Bar"),
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
        MethodVarsTestValues("google.protobuf.Service.Method2",
                             "default_idempotency", "kNonIdempotent"),
        MethodVarsTestValues(
            "google.protobuf.Service.Method2",
            "method_longrunning_deduced_return_doxygen_link",
            "[::google::protobuf::Bar](https://github.com/googleapis/"
            "googleapis/blob/foo/google/foo/v1/service.proto#L12)"),
        // Method3
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_metadata_type",
                             "::google::protobuf::Struct"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_response_type",
                             "::google::protobuf::Empty"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "longrunning_deduced_response_type",
                             "::google::protobuf::Struct"),
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
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "default_idempotency", "kIdempotent"),
        MethodVarsTestValues("google.protobuf.Service.Method3",
                             "method_longrunning_deduced_return_doxygen_link",
                             "::google::protobuf::Struct"),
        // Method4
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
        MethodVarsTestValues("google.protobuf.Service.Method4",
                             "default_idempotency", "kNonIdempotent"),
        // Method5
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_signature0", "std::string const& name"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_signature1",
                             "std::int32_t number, "
                             "::google::protobuf::Foo const& widget"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_signature2", "bool toggle"),
        MethodVarsTestValues(
            "google.protobuf.Service.Method5", "method_signature3",
            "std::string const& name, std::string const& title"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_setters0",
                             "  request.set_name(name);\n"),
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "method_request_setters1",
                             "  request.set_number(number);\n"
                             "  *request.mutable_widget() = widget;\n"),
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
        MethodVarsTestValues("google.protobuf.Service.Method5",
                             "default_idempotency", "kNonIdempotent"),
        // Method6
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_param_key", "name"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_param_value", "name()"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_url_path",
                             "/v1/{name=projects/*/instances/*/databases/*}"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "method_request_url_substitution",
                             "projects/*/instances/*/databases/*"),
        MethodVarsTestValues("google.protobuf.Service.Method6",
                             "default_idempotency", "kIdempotent"),
        // Method7
        MethodVarsTestValues("google.protobuf.Service.Method7",
                             "longrunning_metadata_type",
                             "::google::protobuf::Method2Metadata"),
        MethodVarsTestValues("google.protobuf.Service.Method7",
                             "longrunning_response_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method7",
                             "longrunning_deduced_response_message_type",
                             "google.protobuf.Bar"),
        MethodVarsTestValues("google.protobuf.Service.Method7",
                             "longrunning_deduced_response_type",
                             "::google::protobuf::Bar"),
        MethodVarsTestValues(
            "google.protobuf.Service.Method7",
            "method_longrunning_deduced_return_doxygen_link",
            "[::google::protobuf::Bar](https://github.com/googleapis/"
            "googleapis/blob/foo/google/foo/v1/service.proto#L12)")),
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
