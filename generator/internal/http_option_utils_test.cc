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

#include "generator/internal/http_option_utils.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include <google/api/annotations.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
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
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;

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

char const* const kWellKnownProto = R"""(
syntax = "proto3";
package google.protobuf;
// Leading comments about message Empty.
message Empty {}
message DoubleValue {
  // The double value.
  double value = 1;
}
message FloatValue {
  // The float value.
  float value = 1;
}
message Int64Value {
  // The int64 value.
  int64 value = 1;
}
message UInt64Value {
  // The uint64 value.
  uint64 value = 1;
}
message Int32Value {
  // The int32 value.
  int32 value = 1;
}
message UInt32Value {
  // The uint32 value.
  uint32 value = 1;
}
message BoolValue {
  // The bool value.
  bool value = 1;
}
message StringValue {
  // The string value.
  string value = 1;
}
message BytesValue {
  // The bytes value.
  bytes value = 1;
}
)""";

char const* const kServiceProto =
    "syntax = \"proto3\";\n"
    "package my.package.v1;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "import \"google/longrunning/operation.proto\";\n"
    "import \"google/protobuf/well_known.proto\";\n"
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
    "  bool include_foo = 4;\n"
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
    "}\n"
    "// Leading comments about message Implicit.\n"
    "message Implicit {\n"
    "  string project = 1;\n"
    "  string instance = 2;\n"
    "  Foo body = 3;\n"
    "}\n"
    "// Leading comments about message AnnotatedRequestField.\n"
    "message AnnotatedRequestField {\n"
    "  string project = 1;\n"
    "  string instance = 2;\n"
    "  Namespace namespace = 3 [json_name = \"__json_request_body\"];\n"
    "}\n"
    "// Leading comments about service Service.\n"
    "service Service {\n"
    "  // Leading comments about rpc Method0$.\n"
    "  rpc Method0(Bar) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/simple\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method1.\n"
    "  rpc Method1(Bar) returns (Bar) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/{name=projects/*/instances/*/backups/*}\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method2.\n"
    "  rpc Method2(Bar) returns (google.protobuf.Empty) {\n"
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
    "  // Leading comments about rpc Method3.\n"
    "  rpc Method3(PaginatedInput) returns (PaginatedOutput) {\n"
    "    option (google.api.http) = {\n"
    "       get: \"/v1/foo\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc $Method4.\n"
    "  rpc Method4(Foo) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       get: \"/v1/{name=projects/*/instances/*/databases/*}\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method5.\n"
    "  rpc Method5(Baz) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: "
    "\"/v1/projects/{project=project}/instances/{instance=instance}/"
    "databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method6.\n"
    "  rpc Method6(Implicit) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: "
    "\"/v1/projects/{project}/databases\"\n"
    "       body: \"body\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method7.\n"
    "  rpc Method7(Implicit) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: "
    "\"/v1/projects/{project}/instances/{instance}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    "  // Leading comments about rpc Method8.\n"
    "  rpc Method8(AnnotatedRequestField) returns (google.protobuf.Empty) {\n"
    "    option (google.api.http) = {\n"
    "       post: "
    "\"/v1/projects/{project}/instances/{instance}/databases\"\n"
    "       body: \"*\"\n"
    "    };\n"
    "  }\n"
    "}\n";

char const* const kServiceProtoWithoutVersionInPackage =
    "syntax = \"proto3\";\n"
    "package my.service;\n"
    "import \"google/api/annotations.proto\";\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/api/http.proto\";\n"
    "// Leading comments about message Foo.\n"
    "message Foo {\n"
    "  // name field$ comment.\n"
    "  string name = 1;\n"
    "}\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  string name = 2;\n"
    "}\n"
    "// Leading comments about service Service.\n"
    "service Service {\n"
    "  // Leading comments about rpc Method0$.\n"
    "  rpc Method0(Foo) returns (Bar) {\n"
    "    option (google.api.http) = {\n"
    "       delete: \"/v1/simple\"\n"
    "    };\n"
    "  }\n"
    "}\n";

char const* const kBigQueryServiceProto = R"""(
syntax = "proto3";
package my.package.v1;
import "google/api/annotations.proto";
import "google/api/client.proto";
import "google/api/http.proto";
import "google/protobuf/well_known.proto";

service BigQueryLikeService {
  // RPC to get the results of a query job.
  rpc GetQueryResults(GetQueryResultsRequest)
      returns (GetQueryResultsResponse) {
    option (google.api.http) = {
      get: "/bigquery/v2/projects/{project_id=*}/queries/{job_id=*}"
    };
  }
}

// Request object of GetQueryResults.
message GetQueryResultsRequest {
  // Required. Project ID of the query job.
  string project_id = 1;

  // Required. Job ID of the query job.
  string job_id = 2;

  // Zero-based index of the starting row.
  google.protobuf.UInt64Value start_index = 3;

  // Page token, returned by a previous call, to request the next page of
  // results.
  google.protobuf.StringValue page_token = 4;

  // Maximum number of results to read.
  google.protobuf.UInt32Value max_results = 5;

  // The geographic location of the job.
  google.protobuf.BoolValue include_location = 7;

  // Double field.
  google.protobuf.DoubleValue double_value = 8;

  // Float field.
  google.protobuf.FloatValue float_value = 9;

  // Int32 field.
  google.protobuf.Int32Value int32_value = 10;

  // Int64 field.
  google.protobuf.Int64Value int64_value = 11;

  // Non supported message type that is not a query param.
  google.protobuf.Empty non_supported_type = 12;
}

// Response object of GetQueryResults.
message GetQueryResultsResponse {}
)""";

auto constexpr kServiceProtoWithoutVersionInUrl = R"""(
syntax = "proto3";
package my.package.v1;
import "google/api/annotations.proto";
import "google/api/http.proto";

message Request {}
message Response {}

service Foo {
  rpc Bar(Request) returns (Response) {
    option (google.api.http) = {
      get: "/no/version/id/in/url"
    };
  }
}
)""";

struct MethodVarsTestValues {
  MethodVarsTestValues(std::string m, std::string k, std::string v)
      : method(std::move(m)),
        vars_key(std::move(k)),
        expected_value(std::move(v)) {}
  std::string method;
  std::string vars_key;
  std::string expected_value;
};

class HttpOptionUtilsTest
    : public testing::TestWithParam<MethodVarsTestValues> {
 public:
  HttpOptionUtilsTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/client.proto"), kClientProto},
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/longrunning/operation.proto"),
             kLongrunningOperationsProto},
            {std::string("google/protobuf/well_known.proto"), kWellKnownProto},
            {std::string("google/foo/v1/service.proto"), kServiceProto},
            {std::string(
                 "google/foo/v1/service_without_version_in_package.proto"),
             kServiceProtoWithoutVersionInPackage},
            {std::string("google/foo/v1/big_query_service.proto"),
             kBigQueryServiceProto},
            {std::string("google/foo/v1/service_without_version_in_url.proto"),
             kServiceProtoWithoutVersionInUrl}}),
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

TEST_F(HttpOptionUtilsTest, FilesParseSuccessfully) {
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/client.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/http.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/api/annotations.proto"));
  EXPECT_NE(nullptr,
            pool_.FindFileByName("google/longrunning/operation.proto"));
  EXPECT_NE(nullptr, pool_.FindFileByName("google/foo/v1/service.proto"));
}

TEST_F(HttpOptionUtilsTest, ParseHttpExtensionWithPrefixAndSuffix) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(2);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path,
              Eq("/v1/{parent=projects/*/instances/*}/databases"));
  EXPECT_THAT(info.field_substitutions,
              ElementsAre(Pair("parent", "projects/*/instances/*")));
  EXPECT_THAT(info.body, Eq("*"));
  EXPECT_THAT(info.http_verb, Eq("Post"));
}

TEST_F(HttpOptionUtilsTest,
       ParseHttpExtensionImplicitSingleWithPrefixAndSuffix) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(6);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path, Eq("/v1/projects/{project}/databases"));
  EXPECT_THAT(info.field_substitutions,
              ElementsAre(Pair("project", "project")));
  EXPECT_THAT(info.body, Eq("body"));
  EXPECT_THAT(info.http_verb, Eq("Post"));
}

TEST_F(HttpOptionUtilsTest,
       ParseHttpExtensionImplicitMultipleWithPrefixAndSuffix) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(7);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path,
              Eq("/v1/projects/{project}/instances/{instance}/databases"));
  EXPECT_THAT(
      info.field_substitutions,
      ElementsAre(Pair("project", "project"), Pair("instance", "instance")));
  EXPECT_THAT(info.body, Eq("*"));
  EXPECT_THAT(info.http_verb, Eq("Post"));
}

TEST_F(HttpOptionUtilsTest, ParseHttpExtensionWithOnlyPrefix) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(1);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path, Eq("/v1/{name=projects/*/instances/*/backups/*}"));
  EXPECT_THAT(info.field_substitutions,
              ElementsAre(Pair("name", "projects/*/instances/*/backups/*")));
  EXPECT_THAT(info.body, Eq(""));
  EXPECT_THAT(info.http_verb, Eq("Delete"));
}

TEST_F(HttpOptionUtilsTest, ParseHttpExtensionSimple) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(3);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path, Eq("/v1/foo"));
  EXPECT_THAT(info.field_substitutions, IsEmpty());
  EXPECT_THAT(info.body, Eq(""));
  EXPECT_THAT(info.http_verb, Eq("Get"));
}

TEST_F(HttpOptionUtilsTest,
       ParseHttpExtensionMultipleSubstitutionsWithPrefixAndSuffix) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(5);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path,
              Eq("/v1/projects/{project=project}/instances/{instance=instance}/"
                 "databases"));
  EXPECT_THAT(info.body, Eq("*"));
  EXPECT_THAT(info.http_verb, Eq("Post"));
}

TEST_F(HttpOptionUtilsTest, ParseHttpExtensionWithoutVersionInPackage) {
  FileDescriptor const* service_file = pool_.FindFileByName(
      "google/foo/v1/service_without_version_in_package.proto");
  MethodDescriptor const* method = service_file->service(0)->method(0);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path, Eq("/v1/simple"));
}

TEST_F(HttpOptionUtilsTest, ParseHttpExtensionWithoutVersionInUrl) {
  FileDescriptor const* service_file = pool_.FindFileByName(
      "google/foo/v1/service_without_version_in_url.proto");
  MethodDescriptor const* method = service_file->service(0)->method(0);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  EXPECT_THAT(info.url_path, Eq("/no/version/id/in/url"));
}

TEST_F(HttpOptionUtilsTest, SetHttpDerivedMethodVarsSimpleInfo) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(0);
  VarsDictionary vars;
  SetHttpDerivedMethodVars(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars.at("method_http_verb"), Eq("Delete"));
  EXPECT_THAT(
      vars.at("method_rest_path"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", options), "/", "simple"))"""));
  EXPECT_THAT(
      vars.at("method_rest_path_async"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", *options), "/", "simple"))"""));
}

TEST_F(HttpOptionUtilsTest, SetHttpDerivedMethodVarsExtensionInfoSingleParam) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(2);
  VarsDictionary vars;
  SetHttpDerivedMethodVars(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars.at("method_request_params"),
              Eq("\"parent=\", internal::UrlEncode(request.parent())"));
  EXPECT_THAT(vars.at("method_request_body"), Eq("*"));
  EXPECT_THAT(vars.at("method_http_verb"), Eq("Post"));
  EXPECT_THAT(
      vars.at("method_rest_path"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", options), "/", request.parent(), "/", "databases"))"""));
  EXPECT_THAT(
      vars.at("method_rest_path_async"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", *options), "/", request.parent(), "/", "databases"))"""));
}

TEST_F(HttpOptionUtilsTest,
       SetHttpDerivedMethodVarsExtensionInfoMultipleParams) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(7);
  VarsDictionary vars;
  SetHttpDerivedMethodVars(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars.at("method_request_params"),
              Eq("\"project=\", internal::UrlEncode(request.project()), "
                 "\"&\",\"instance=\", "
                 "internal::UrlEncode(request.instance())"));
  EXPECT_THAT(vars.at("method_request_body"), Eq("*"));
  EXPECT_THAT(vars.at("method_http_verb"), Eq("Post"));
  EXPECT_THAT(
      vars.at("method_rest_path"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", options), "/", "projects", "/", request.project(), "/", "instances", "/", request.instance(), "/", "databases"))"""));
  EXPECT_THAT(
      vars.at("method_rest_path_async"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", *options), "/", "projects", "/", request.project(), "/", "instances", "/", request.instance(), "/", "databases"))"""));
}

TEST_F(HttpOptionUtilsTest,
       SetHttpDerivedMethodVarsExtensionInfoMultipleSubstitutions) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(5);
  VarsDictionary vars;
  SetHttpDerivedMethodVars(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars.at("method_request_body"), Eq("*"));
  EXPECT_THAT(vars.at("method_http_verb"), Eq("Post"));
  EXPECT_THAT(
      vars.at("method_rest_path"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", options), "/", "projects", "/", request.project(), "/", "instances", "/", request.instance(), "/", "databases"))"""));
  EXPECT_THAT(
      vars.at("method_rest_path_async"),
      Eq(R"""(absl::StrCat("/", rest_internal::DetermineApiVersion("v1", *options), "/", "projects", "/", request.project(), "/", "instances", "/", request.instance(), "/", "databases"))"""));
}

TEST_F(HttpOptionUtilsTest, SetHttpQueryParametersNoParams) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(5);
  VarsDictionary vars;
  SetHttpQueryParameters(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars.at("method_http_query_parameters"), Eq(""));
}

TEST_F(HttpOptionUtilsTest, SetHttpQueryParametersGetWithParams) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(4);
  VarsDictionary vars;
  SetHttpQueryParameters(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars.at("method_http_query_parameters"), Eq(""));
}

TEST_F(HttpOptionUtilsTest, SetHttpGetQueryParametersGetPaginated) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(3);
  VarsDictionary vars;
  SetHttpQueryParameters(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(vars,
              Contains(Pair("method_http_query_parameters",
                            AllOf(HasSubstr("TrimEmptyQueryParameters"),
                                  HasSubstr("page_token"), HasSubstr("name"),
                                  HasSubstr("include_foo")))));
}

TEST_F(HttpOptionUtilsTest,
       SetHttpGetQueryParametersGetWellKnownTypesPaginated) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/big_query_service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(0);
  VarsDictionary vars;
  SetHttpQueryParameters(
      ParseHttpExtension(method->options().GetExtension(google::api::http)),
      *method, vars);
  EXPECT_THAT(
      vars,
      Contains(Pair(
          "method_http_query_parameters",
          AllOf(HasSubstr("TrimEmptyQueryParameters"), HasSubstr("page_token"),
                HasSubstr("max_results"), HasSubstr("include_location"),
                HasSubstr("double_value"), HasSubstr("float_value"),
                HasSubstr("int32_value"), HasSubstr("int64_value")))));
}

TEST_F(HttpOptionUtilsTest, HasHttpAnnotationRoutingHeaderSuccess) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  EXPECT_TRUE(
      HasHttpRoutingHeader(*service_file_descriptor->service(0)->method(1)));
  EXPECT_TRUE(
      HasHttpAnnotation(*service_file_descriptor->service(0)->method(1)));
}

TEST_F(HttpOptionUtilsTest, HasSimpleHttpAnnotationSuccess) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");

  EXPECT_TRUE(
      HasHttpAnnotation(*service_file_descriptor->service(0)->method(0)));
}

TEST_F(HttpOptionUtilsTest, HasRoutingHeaderWrongUrlFormat) {
  google::protobuf::FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.foo.v1"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "Method0"
        input_type: "google.foo.v1.Bar"
        output_type: "google.foo.v1.Empty"
        options {
          [google.api.http] { post: "/v2/entries:list" body: "*" }
        }
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(
      HasHttpRoutingHeader(*service_file_descriptor->service(0)->method(0)));
}

TEST_F(HttpOptionUtilsTest, HasNoHttpAnnotation) {
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
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(
      HasHttpRoutingHeader(*service_file_descriptor->service(0)->method(0)));
  EXPECT_FALSE(
      HasHttpAnnotation(*service_file_descriptor->service(0)->method(0)));
}

TEST_F(HttpOptionUtilsTest, FormatRequestResourceFailedParse) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(2);
  auto result =
      FormatRequestResource(*method->input_type(), HttpExtensionInfo{});
  EXPECT_THAT(result, Eq("request"));
}

TEST_F(HttpOptionUtilsTest, FormatRequestResourceWholeMessage) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(2);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  auto result = FormatRequestResource(*method->input_type(), info);
  EXPECT_THAT(result, Eq("request"));
}

TEST_F(HttpOptionUtilsTest, FormatRequestResourceMessageField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(6);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  auto result = FormatRequestResource(*method->input_type(), info);
  EXPECT_THAT(result, Eq("request.body()"));
}

TEST_F(HttpOptionUtilsTest, FormatRequestResourceAnnotatedRequestField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  MethodDescriptor const* method =
      service_file_descriptor->service(0)->method(8);
  auto info =
      ParseHttpExtension(method->options().GetExtension(google::api::http));
  auto result = FormatRequestResource(*method->input_type(), info);
  EXPECT_THAT(result, Eq("request.namespace_()"));
}

TEST_F(HttpOptionUtilsTest, FormatApiVersionFromUrlPattern) {
  std::string url_pattern_v1 = "/v1/foo/bar";
  EXPECT_THAT(FormatApiVersionFromUrlPattern(url_pattern_v1),
              Optional(std::string("v1")));
  std::string url_pattern_v2 = "/foo/v2/bar";
  EXPECT_THAT(FormatApiVersionFromUrlPattern(url_pattern_v2),
              Optional(std::string("v2")));
}

TEST_F(HttpOptionUtilsTest, FormatApiVersionFromUrlPatternNonExist) {
  std::string url_pattern = "/foo/bar";
  EXPECT_THAT(FormatApiVersionFromUrlPattern(url_pattern), Eq(absl::nullopt));
}

TEST_F(HttpOptionUtilsTest, FormatApiVersionFromUrlPattern) {
  std::string url_pattern_v1 = "/v1/foo/bar";
  EXPECT_THAT(FormatApiVersionFromUrlPattern(url_pattern_v1),
              Optional(std::string("v1")));
  std::string url_pattern_v2 = "/foo/v2/bar";
  EXPECT_THAT(FormatApiVersionFromUrlPattern(url_pattern_v2),
              Optional(std::string("v2")));
}

TEST_F(HttpOptionUtilsTest, FormatApiVersionFromUrlPatternNonExist) {
  std::string url_pattern = "/foo/bar";
  EXPECT_THAT(FormatApiVersionFromUrlPattern(url_pattern), Eq(absl::nullopt));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
