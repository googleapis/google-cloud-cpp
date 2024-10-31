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
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/text_format.h"
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
using ::google::protobuf::TextFormat;
using ::testing::Contains;
using ::testing::NotNull;
using ::testing::Pair;

TEST(LongrunningTest, IsGRPCLongrunningOperation) {
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
        name: "GrpcLro"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Operation"
        options {
          [google.longrunning.operation_info] {}
        }
      }
      method {
        name: "NonLro1"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "NonLro2"
        input_type: "google.longrunning.Bar"
        output_type: "google.longrunning.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(TextFormat::ParseFromString(kServiceText, &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsGRPCLongrunningOperation(
      *service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(IsHttpLongrunningOperation(
      *service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(IsGRPCLongrunningOperation(
      *service_file_descriptor->service(0)->method(1)));
  EXPECT_FALSE(IsGRPCLongrunningOperation(
      *service_file_descriptor->service(0)->method(2)));
  EXPECT_TRUE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(1)));
  EXPECT_FALSE(
      IsLongrunningOperation(*service_file_descriptor->service(0)->method(2)));
}

TEST(LongrunningTest, IsLongrunningMetadataTypeUsedAsResponseEmptyResponse) {
  FileDescriptorProto longrunning_file;
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operation.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(kLongrunningText, &longrunning_file));
  FileDescriptorProto service_file;
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
  ASSERT_TRUE(TextFormat::ParseFromString(kServiceText, &service_file));
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
  ASSERT_TRUE(TextFormat::ParseFromString(kLongrunningText, &longrunning_file));
  FileDescriptorProto service_file;
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
  ASSERT_TRUE(TextFormat::ParseFromString(kServiceText, &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsLongrunningMetadataTypeUsedAsResponse(
      *service_file_descriptor->service(0)->method(0)));
}

TEST(LongrunningTest, IsLongrunningMetadataTypeUsedAsResponseNotLongrunning) {
  FileDescriptorProto service_file;
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
  ASSERT_TRUE(TextFormat::ParseFromString(kServiceText, &service_file));
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

char const* const kExtendedOperationsProto =
    "syntax = \"proto3\";\n"
    "package google.cloud;\n"
    "import \"google/protobuf/descriptor.proto\";\n"
    "extend google.protobuf.FieldOptions {\n"
    "  OperationResponseMapping operation_field = 1149;\n"
    "  string operation_request_field = 1150;\n"
    "  string operation_response_field = 1151;\n"
    "}\n"
    "extend google.protobuf.MethodOptions {\n"
    "  string operation_service = 1249;\n"
    "}\n"
    "enum OperationResponseMapping {\n"
    "  UNDEFINED = 0;\n"
    "  NAME = 1;\n"
    "  STATUS = 2;\n"
    "  ERROR_CODE = 3;\n"
    "  ERROR_MESSAGE = 4;\n"
    "}\n";

char const* const kWellKnownProto = R"""(
syntax = "proto3";
package google.protobuf;
// Leading comments about message Empty.
message Empty {}
)""";

char const* const kServiceProto =
    "syntax = \"proto3\";\n"
    "package my.service.v1;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/protobuf/well_known.proto\";\n"
    "import \"google/longrunning/operation.proto\";\n"
    "import \"google/cloud/extended_operations.proto\";\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  string parent = 1;\n"
    "}\n"
    "// Leading comments about message Disk.\n"
    "message Disk {\n"
    "  string name = 1;\n"
    "}\n"
    "// Leading comments about message ErrorInfo.\n"
    "message ErrorInfo {\n"
    "  optional string domain = 1;\n"
    "  map<string, string> metadatas = 2;\n"
    "  optional string reason = 3;\n"
    "}"
    "// Leading comments about message Operation.\n"
    "message Operation {\n"
    "  optional string client_operation_id = 1;\n"
    "  optional string creation_timestamp = 2;\n"
    "  optional string description = 3;\n"
    "  optional string end_time = 4;\n"
    "  message Error {\n"
    "    message ErrorsItem {\n"
    "      optional string code = 1;\n"
    "      message ErrorDetailsItem {\n"
    "        optional ErrorInfo error_info = 1;\n"
    "      }\n"
    "      repeated ErrorDetailsItem error_details = 2;\n"
    "      optional string location = 3;\n"
    "      optional string message = 4;\n"
    "    }\n"
    "    repeated ErrorsItem errors = 1;\n"
    "  }\n"
    "  optional Error error = 5;\n"
    "  optional string http_error_message = 6 [(google.cloud.operation_field) "
    "= ERROR_MESSAGE];\n"
    "  optional int32 http_error_status_code = 7 "
    "[(google.cloud.operation_field) = ERROR_CODE];\n"
    "  optional string id = 8;\n"
    "  optional string insert_time = 9;\n"
    "  optional string kind = 10;\n"
    "  optional string name = 11 [(google.cloud.operation_field) = NAME];\n"
    "  optional string operation_group_id = 12;\n"
    "  optional string operation_type = 13;\n"
    "  optional int32 progress = 14;\n"
    "  optional string region = 15;\n"
    "  optional string self_link = 16;\n"
    "  optional string start_time = 17;\n"
    "  // [Output Only] The status of the operation, which can be one of the\n"
    "  // following: `PENDING`, `RUNNING`, or `DONE`.\n"
    "  // DONE:\n"
    "  // PENDING:\n"
    "  // RUNNING:\n"
    "  optional string status = 18 [(google.cloud.operation_field) = STATUS];\n"
    "  optional string status_message = 19;\n"
    "  optional string target_id = 20;\n"
    "  optional string target_link = 21;\n"
    "  optional string user = 22;\n"
    "  message WarningsItem {\n"
    "    optional string code = 1;\n"
    "    message DataItem {\n"
    "      optional string key = 1;\n"
    "      optional string value = 2;\n"
    "    }\n"
    "    repeated DataItem data = 2;\n"
    "    optional string message = 3;\n"
    "  }\n"
    "  repeated WarningsItem warnings = 23;\n"
    "  optional string zone = 24;\n"
    "}"
    "// Leading comments about message DiskRequest.\n"
    "message DiskRequest {\n"
    "  optional Disk disk_resource = 1\n"
    "      [json_name = \"resource\"];\n"
    "  string project = 2 [\n"
    "    (google.cloud.operation_request_field) = \"project\"\n"
    "  ];\n"
    "  optional string request_id = 3;\n"
    "  optional string source_image = 4;\n"
    "  string zone = 5 [\n"
    "    (google.cloud.operation_request_field) = \"zone\"\n"
    "  ];\n"
    "}"
    "// Leading comments about service Service0.\n"
    "service Service0 {\n"
    "  // Leading comments about rpc Method0$.\n"
    "  rpc Method0(Bar) returns (google.longrunning.Operation) {\n"
    "    option (google.api.http) = {\n"
    "       patch: \"/v1/{parent=projects/*/instances/*}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "    option (google.longrunning.operation_info) = {\n"
    "      response_type: \"my.service.v1.Bar\"\n"
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
    "}\n"
    "// Leading comments about service Service1.\n"
    "service Service1 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(DiskRequest) returns (Operation) {\n"
    "    option (google.api.http) = {\n"
    "      post: "
    "\"/compute/v1/projects/{project=project}/zones/{zone=zone}/disks\"\n"
    "      body: \"disk_resource\"\n"
    "    };\n"
    "    option (google.api.method_signature) = "
    "\"project,zone,disk_resource\";\n"
    "    option (google.cloud.operation_service) = \"ZoneOperations\";"
    "  }\n"
    "}\n"
    "// Leading comments about service Service2.\n"
    "service Service2 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(DiskRequest) returns (Operation) {\n"
    "    option (google.api.http) = {\n"
    "      post: "
    "\"/compute/v1/projects/{project=project}/regions/{region=region}/disks\"\n"
    "      body: \"disk_resource\"\n"
    "    };\n"
    "    option (google.api.method_signature) = "
    "\"project,zone,disk_resource\";\n"
    "    option (google.cloud.operation_service) = \"RegionOperations\";"
    "  }\n"
    "}\n"
    "// Leading comments about service Service3.\n"
    "service Service3 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(DiskRequest) returns (Operation) {\n"
    "    option (google.api.http) = {\n"
    "      post: "
    "\"/compute/v1/projects/{project=project}/global/disks\"\n"
    "      body: \"disk_resource\"\n"
    "    };\n"
    "    option (google.api.method_signature) = "
    "\"project,zone,disk_resource\";\n"
    "    option (google.cloud.operation_service) = \"GlobalOperations\";"
    "  }\n"
    "}\n"
    "// Leading comments about service Service4.\n"
    "service Service4 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(DiskRequest) returns (Operation) {\n"
    "    option (google.api.http) = {\n"
    "      post: "
    "\"/compute/v1/locations/global/disks\"\n"
    "      body: \"disk_resource\"\n"
    "    };\n"
    "    option (google.api.method_signature) = "
    "\"project,zone,disk_resource\";\n"
    "    option (google.cloud.operation_service) = "
    "\"GlobalOrganizationOperations\";"
    "  }\n"
    "}\n";

class LongrunningVarsTest : public testing::Test {
 public:
  LongrunningVarsTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/client.proto"), kClientProto},
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/longrunning/operation.proto"),
             kLongrunningOperationsProto},
            {std::string("google/cloud/extended_operations.proto"),
             kExtendedOperationsProto},
            {std::string("google/protobuf/well_known.proto"), kWellKnownProto},
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

TEST_F(LongrunningVarsTest, FilesParseSuccessfully) {
  EXPECT_THAT(pool_.FindFileByName("google/api/client.proto"), NotNull());
  EXPECT_THAT(pool_.FindFileByName("google/api/http.proto"), NotNull());
  EXPECT_THAT(pool_.FindFileByName("google/api/annotations.proto"), NotNull());
  EXPECT_THAT(pool_.FindFileByName("google/longrunning/operation.proto"),
              NotNull());
  EXPECT_THAT(pool_.FindFileByName("google/foo/v1/service.proto"), NotNull());
}

TEST_F(LongrunningVarsTest,
       SetLongrunningOperationMethodVarsResponseAndMetadata) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(0);
  VarsDictionary vars;
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars, Contains(Pair("longrunning_metadata_type",
                                  "google::protobuf::Method2Metadata")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_response_type",
                                  "my::service::v1::Bar")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_message_type",
                                  "my.service.v1.Bar")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_type",
                                  "my::service::v1::Bar")));
  EXPECT_THAT(
      vars, Contains(Pair("method_longrunning_deduced_return_doxygen_link",
                          "@googleapis_link{my::service::v1::Bar,google/foo/v1/"
                          "service.proto#L10}")));
}

TEST_F(LongrunningVarsTest,
       SetLongrunningOperationMethodVarsEmptyResponseAndMetadata) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(1);
  VarsDictionary vars;
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars, Contains(Pair("longrunning_operation_type",
                                  "google::longrunning::Operation")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_metadata_type",
                                  "google::protobuf::Struct")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_response_type",
                                  "google::protobuf::Empty")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_message_type",
                                  "google.protobuf.Struct")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_type",
                                  "google::protobuf::Struct")));
  EXPECT_THAT(vars,
              Contains(Pair("method_longrunning_deduced_return_doxygen_link",
                            "google::protobuf::Struct")));
}

TEST_F(LongrunningVarsTest,
       SetLongrunningOperationMethodVarsUnqualifiedResponseAndMetadata) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(2);
  VarsDictionary vars;
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars, Contains(Pair("longrunning_metadata_type",
                                  "google::protobuf::Method2Metadata")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_response_type",
                                  "my::service::v1::Bar")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_message_type",
                                  "my.service.v1.Bar")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_type",
                                  "my::service::v1::Bar")));
  EXPECT_THAT(
      vars, Contains(Pair("method_longrunning_deduced_return_doxygen_link",
                          "@googleapis_link{my::service::v1::Bar,google/foo/v1/"
                          "service.proto#L10}")));
}

TEST_F(LongrunningVarsTest, SetLongrunningOperationMethodVarsBespokeLRO) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(1)->method(0);
  VarsDictionary vars;
  EXPECT_TRUE(IsLongrunningOperation(*method));
  EXPECT_FALSE(IsGRPCLongrunningOperation(*method));
  EXPECT_TRUE(IsHttpLongrunningOperation(*method));
  SetLongrunningOperationMethodVars(*method, vars);
  EXPECT_THAT(vars, Contains(Pair("longrunning_operation_type",
                                  "my::service::v1::Operation")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_response_type",
                                  "my::service::v1::Operation")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_message_type",
                                  "my.service.v1.Operation")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_deduced_response_type",
                                  "my::service::v1::Operation")));
  EXPECT_THAT(vars,
              Contains(Pair("method_longrunning_deduced_return_doxygen_link",
                            "@googleapis_link{my::service::v1::Operation,"
                            "google/foo/v1/service.proto#L23}")));
}

TEST_F(LongrunningVarsTest, SetLongrunningOperationServiceVarsGRPC) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  VarsDictionary vars;
  SetLongrunningOperationServiceVars(*service_file_descriptor->service(0),
                                     vars);
  EXPECT_THAT(vars, Contains(Pair("longrunning_operation_include_header",
                                  "google/longrunning/operations.pb.h")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_response_type",
                                  "google::longrunning::Operation")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_request_type",
                                  "google::longrunning::GetOperationRequest")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_cancel_operation_request_type",
                            "google::longrunning::CancelOperationRequest")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_get_operation_path",
                            R"""(absl::StrCat("/v1/", request.name()))""")));
  EXPECT_THAT(
      vars,
      Contains(Pair("longrunning_cancel_operation_path",
                    R"""(absl::StrCat("/v1/", request.name(), ":cancel"))""")));
}

TEST_F(LongrunningVarsTest, SetLongrunningOperationServiceVarsNonGRPCGlobal) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  VarsDictionary vars;
  SetLongrunningOperationServiceVars(*service_file_descriptor->service(3),
                                     vars);
  EXPECT_THAT(
      vars,
      Contains(Pair(
          "longrunning_operation_include_header",
          "google/cloud/compute/global_operations/v1/global_operations.pb.h")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_request_type",
                                  "google::cloud::cpp::compute::global_"
                                  "operations::v1::GetOperationRequest")));
  EXPECT_THAT(
      vars, Contains(Pair("longrunning_cancel_operation_request_type",
                          "google::cloud::cpp::compute::global_operations::v1::"
                          "DeleteOperationRequest")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_set_operation_fields", R"""(
      r.set_project(request.project());
      r.set_operation(op);
)""")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_await_set_operation_fields", R"""(
      r.set_project(info.project);
      r.set_operation(info.operation);
)""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/projects/", request.project(),
                              "/global/operations/", request.operation()))""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_cancel_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/projects/", request.project(),
                              "/global/operations/", request.operation()))""")));
}

TEST_F(LongrunningVarsTest,
       SetLongrunningOperationServiceVarsNonGRPCGlobalOrg) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  VarsDictionary vars;
  SetLongrunningOperationServiceVars(*service_file_descriptor->service(4),
                                     vars);
  EXPECT_THAT(
      vars, Contains(Pair("longrunning_operation_include_header",
                          "google/cloud/compute/global_organization_operations/"
                          "v1/global_organization_operations.pb.h")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_request_type",
                                  "google::cloud::cpp::compute::global_"
                                  "organization_operations::v1::"
                                  "GetOperationRequest")));
  EXPECT_THAT(
      vars,
      Contains(Pair(
          "longrunning_cancel_operation_request_type",
          "google::cloud::cpp::compute::global_organization_operations::v1::"
          "DeleteOperationRequest")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_set_operation_fields", R"""(
      r.set_operation(op);
)""")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_await_set_operation_fields", R"""(
      r.set_operation(info.operation);
)""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/locations/global/operations/", request.operation()))""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_cancel_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/locations/global/operations/", request.operation()))""")));
}

TEST_F(LongrunningVarsTest, SetLongrunningOperationServiceVarsNonGRPCRegion) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  VarsDictionary vars;
  SetLongrunningOperationServiceVars(*service_file_descriptor->service(2),
                                     vars);
  EXPECT_THAT(
      vars,
      Contains(Pair(
          "longrunning_operation_include_header",
          "google/cloud/compute/region_operations/v1/region_operations.pb.h")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_request_type",
                                  "google::cloud::cpp::compute::region_"
                                  "operations::v1::GetOperationRequest")));
  EXPECT_THAT(
      vars, Contains(Pair("longrunning_cancel_operation_request_type",
                          "google::cloud::cpp::compute::region_operations::v1::"
                          "DeleteOperationRequest")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_set_operation_fields", R"""(
      r.set_project(request.project());
      r.set_region(request.region());
      r.set_operation(op);
)""")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_await_set_operation_fields", R"""(
      r.set_project(info.project);
      r.set_region(info.region);
      r.set_operation(info.operation);
)""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/projects/", request.project(),
                              "/regions/", request.region(),
                              "/operations/", request.operation()))""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_cancel_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/projects/", request.project(),
                              "/regions/", request.region(),
                              "/operations/", request.operation()))""")));
}

TEST_F(LongrunningVarsTest, SetLongrunningOperationServiceVarsNonGRPCZone) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  VarsDictionary vars;
  SetLongrunningOperationServiceVars(*service_file_descriptor->service(1),
                                     vars);
  EXPECT_THAT(
      vars,
      Contains(Pair(
          "longrunning_operation_include_header",
          "google/cloud/compute/zone_operations/v1/zone_operations.pb.h")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_request_type",
                                  "google::cloud::cpp::compute::zone_"
                                  "operations::v1::GetOperationRequest")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_cancel_operation_request_type",
                            "google::cloud::cpp::compute::zone_operations::v1::"
                            "DeleteOperationRequest")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_set_operation_fields", R"""(
      r.set_project(request.project());
      r.set_zone(request.zone());
      r.set_operation(op);
)""")));
  EXPECT_THAT(vars,
              Contains(Pair("longrunning_await_set_operation_fields", R"""(
      r.set_project(info.project);
      r.set_zone(info.zone);
      r.set_operation(info.operation);
)""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_get_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/projects/", request.project(),
                              "/zones/", request.zone(),
                              "/operations/", request.operation()))""")));
  EXPECT_THAT(vars, Contains(Pair("longrunning_cancel_operation_path_rest",
                                  R"""(absl::StrCat("/compute/",
                              rest_internal::DetermineApiVersion("v1", *options),
                              "/projects/", request.project(),
                              "/zones/", request.zone(),
                              "/operations/", request.operation()))""")));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
