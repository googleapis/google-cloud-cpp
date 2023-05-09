// Copyright 2023 Google LLC
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

#include "generator/internal/longrunning.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::FakeSourceTree;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::MethodDescriptor;

TEST(LongrunningTest, IsLongrunningOperation) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.longrunning"
    message_type { name: "Bar" }
    message_type { name: "Operation" }
    service {
      name: "Service"
      method {
        name: "Lro"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "NonLro"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(1)));
}

TEST(LongrunningTest, IsLongrunningMetadataTypeUsedAsResponseEmptyResponse) {
  FileDescriptorProto longrunning_file;
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operation.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    dependency: "google/longrunning/operation.proto"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
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
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

TEST(LongrunningTest, IsLongrunningMetadataTypeUsedAsResponseNonEmptyResponse) {
  FileDescriptorProto longrunning_file;
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operation.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    dependency: "google/longrunning/operation.proto"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
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
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

TEST(LongrunningTest, IsLongrunningMetadataTypeUsedAsResponseNotLongrunning) {
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Empty"
        options {
          [google.api.http] {
            patch: "/v1/{parent=projects/*/instances/*}/databases"
          }
        }
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

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
    "}\n";

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

char const* const kServiceProto =
    "syntax = \"proto3\";\n"
    "package google.protobuf;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/longrunning/operation.proto\";\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  string parent = 1;\n"
    "}\n"
    "// Leading comments about message Empty.\n"
    "message Empty {}\n"
    "// Leading comments about service Service.\n"
    "service Service {\n"
    "  // Leading comments about rpc Method0$.\n"
    "  rpc Method0(Bar) returns (google.longrunning.Operation) {\n"
    "    option (google.api.http) = {\n"
    "       patch: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "    option (google.longrunning.operation_info) = {\n"
    "      response_type: \"google.protobuf.Bar\"\n"
    "      metadata_type: \"google.protobuf.Method2Metadata\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method1.\n"
    "  rpc Method1(Bar) returns (google.longrunning.Operation) {\n"
    "    option (google.api.http) = {\n"
    "       put: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "    };\n"
    "    option (google.longrunning.operation_info) = {\n"
    "      response_type: \"google.protobuf.Empty\"\n"
    "      metadata_type: \"google.protobuf.Struct\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method2.\n"
    "  rpc Method2(Bar) returns (google.longrunning.Operation) {\n"
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

class LongrunningMethodVarsTest : public testing::Test {
 public:
  LongrunningMethodVarsTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/client.proto"), kClientProto},
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/longrunning/operation.proto"),
             kLongrunningOperationsProto},
            {std::string("google/foo/v1/service.proto"), kServiceProto}}),
        source_tree_db_(&source_tree_),
        merged_db_(&simple_db_, &source_tree_db_),
        pool_(&merged_db_, &collector_) {
    // we need descriptor.proto to be accessible by the pool
    // since our test file imports it
    FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto_);
    simple_db_.Add(file_proto_);
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
};

TEST_F(LongrunningMethodVarsTest, FilesParseSuccessfully) {
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/client.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/http.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/annotations.proto"));
  EXPECT_NE(nullptr,
            pool_.FindFileByName("google/longrunning/operation.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/foo/v1/service.proto"));
}

TEST_F(LongrunningMethodVarsTest,
       SetLongrunningOperationMethodVarsResponseAndMetadata) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(0);
  VarsDictionary vars;
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars, ::testing::Contains(
                        ::testing::Pair("longrunning_metadata_type",
                                        "google::protobuf::Method2Metadata")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "longrunning_response_type", "google::protobuf::Bar")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "longrunning_deduced_response_message_type",
                        "google.protobuf.Bar")));
  EXPECT_THAT(
      vars, ::testing::Contains(::testing::Pair(
                "longrunning_deduced_response_type", "google::protobuf::Bar")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "method_longrunning_deduced_return_doxygen_link",
                        "@googleapis_link{google::protobuf::Bar,google/foo/v1/"
                        "service.proto#L8}")));
}

TEST_F(LongrunningMethodVarsTest,
       SetLongrunningOperationMethodVarsEmptyResponseAndMetadata) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(1);
  VarsDictionary vars;
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars,
              ::testing::Contains(::testing::Pair("longrunning_metadata_type",
                                                  "google::protobuf::Struct")));
  EXPECT_THAT(vars,
              ::testing::Contains(::testing::Pair("longrunning_response_type",
                                                  "google::protobuf::Empty")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "longrunning_deduced_response_message_type",
                        "google.protobuf.Struct")));
  EXPECT_THAT(vars, ::testing::Contains(
                        ::testing::Pair("longrunning_deduced_response_type",
                                        "google::protobuf::Struct")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "method_longrunning_deduced_return_doxygen_link",
                        "google::protobuf::Struct")));
}

TEST_F(LongrunningMethodVarsTest,
       SetLongrunningOperationMethodVarsUnqualifiedResponseAndMetadata) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(2);
  VarsDictionary vars;
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars, ::testing::Contains(
                        ::testing::Pair("longrunning_metadata_type",
                                        "google::protobuf::Method2Metadata")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "longrunning_response_type", "google::protobuf::Bar")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "longrunning_deduced_response_message_type",
                        "google.protobuf.Bar")));
  EXPECT_THAT(
      vars, ::testing::Contains(::testing::Pair(
                "longrunning_deduced_response_type", "google::protobuf::Bar")));
  EXPECT_THAT(vars, ::testing::Contains(::testing::Pair(
                        "method_longrunning_deduced_return_doxygen_link",
                        "@googleapis_link{google::protobuf::Bar,google/foo/v1/"
                        "service.proto#L8}")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
