// Copyright 2021 Google LLC
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

#include "generator/internal/service_code_generator.h"
#include "google/cloud/log.h"
#include "generator/testing/printer_mocks.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::MockGeneratorContext;
using ::google::cloud::generator_testing::MockZeroCopyOutputStream;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Return;

class StringSourceTree : public google::protobuf::compiler::SourceTree {
 public:
  explicit StringSourceTree(std::map<std::string, std::string> files)
      : files_(std::move(files)) {}

  google::protobuf::io::ZeroCopyInputStream* Open(
      std::string const& filename) override {
    auto iter = files_.find(filename);
    return iter == files_.end() ? nullptr
                                : new google::protobuf::io::ArrayInputStream(
                                      iter->second.data(),
                                      static_cast<int>(iter->second.size()));
  }

 private:
  std::map<std::string, std::string> files_;
};

class AbortingErrorCollector : public DescriptorPool::ErrorCollector {
 public:
  AbortingErrorCollector() = default;
  AbortingErrorCollector(AbortingErrorCollector const&) = delete;
  AbortingErrorCollector& operator=(AbortingErrorCollector const&) = delete;

  void AddError(std::string const& filename, std::string const& element_name,
                google::protobuf::Message const*, ErrorLocation,
                std::string const& error_message) override {
    GCP_LOG(FATAL) << "AddError() called unexpectedly: " << filename << " ["
                   << element_name << "]: " << error_message;
  }
};

class TestGenerator : public ServiceCodeGenerator {
 public:
  TestGenerator(google::protobuf::ServiceDescriptor const* service_descriptor,
                google::protobuf::compiler::GeneratorContext* context,
                VarsDictionary service_vars = {{"header_path_key",
                                                "header_path"}})
      : ServiceCodeGenerator("header_path_key", service_descriptor,
                             std::move(service_vars), {}, context) {}

  using ServiceCodeGenerator::HasBidirStreamingMethod;
  using ServiceCodeGenerator::HasExplicitRoutingMethod;
  using ServiceCodeGenerator::HasLongrunningMethod;
  using ServiceCodeGenerator::HasMessageWithMapField;
  using ServiceCodeGenerator::HasPaginatedMethod;
  using ServiceCodeGenerator::HasStreamingReadMethod;
  using ServiceCodeGenerator::HasStreamingWriteMethod;
  using ServiceCodeGenerator::IsExperimental;
  using ServiceCodeGenerator::MethodSignatureWellKnownProtobufTypeIncludes;

  Status GenerateHeader() override { return {}; }
  Status GenerateCc() override { return {}; }
};

TEST(PredicateUtilsTest, IsExperimental) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.foo.v1"
    service { name: "Service" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillRepeatedly(Return(nullptr));

  TestGenerator g1(service_file_descriptor->service(0),
                   generator_context.get());
  EXPECT_FALSE(g1.IsExperimental());

  TestGenerator g2(
      service_file_descriptor->service(0), generator_context.get(),
      {{"header_path_key", "header_path"}, {"experimental", "true"}});

  TestGenerator g3(
      service_file_descriptor->service(0), generator_context.get(),
      {{"header_path_key", "header_path"}, {"experimental", "false"}});
  EXPECT_FALSE(g3.IsExperimental());
}

TEST(PredicateUtilsTest, HasLongRunningMethodNone) {
  FileDescriptorProto longrunning_file;
  /// @cond
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operations.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));

  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    dependency: "google/longrunning/operations.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "One"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Empty"
      }
      method {
        name: "Two"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_FALSE(g.HasLongrunningMethod());
}

TEST(PredicateUtilsTest, HasLongRunningMethodOne) {
  FileDescriptorProto longrunning_file;
  /// @cond
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operations.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    dependency: "google/longrunning/operations.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "One"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "Two"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasLongrunningMethod());
}

TEST(PredicateUtilsTest, HasLongRunningMethodMoreThanOne) {
  FileDescriptorProto longrunning_file;
  /// @cond
  auto constexpr kLongrunningText = R"pb(
    name: "google/longrunning/operations.proto"
    package: "google.longrunning"
    message_type { name: "Operation" }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kLongrunningText,
                                                            &longrunning_file));
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    dependency: "google/longrunning/operations.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Empty" }
    service {
      name: "Service"
      method {
        name: "One"
        input_type: "google.protobuf.Bar"
        output_type: "google.protobuf.Bar"
      }
      method {
        name: "Two"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
      }
      method {
        name: "Three"
        input_type: "google.protobuf.Bar"
        output_type: "google.longrunning.Operation"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  pool.BuildFile(longrunning_file);
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasLongrunningMethod());
}

TEST(PredicateUtilsTest, HasPaginatedMethodTrue) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
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
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasPaginatedMethod());
}

TEST(PredicateUtilsTest, HasPaginatedMethodFalse) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Input" }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_FALSE(g.HasPaginatedMethod());
}

char const* const kFooServiceProto =
    "syntax = \"proto3\";\n"
    "package google.protobuf;\n"
    "// Leading comments about message Foo.\n"
    "message Foo {\n"
    "  string baz = 1;\n"
    "  map<string, string> labels = 2;\n"
    "}\n"
    "// Leading comments about message Empty.\n"
    "message Empty {}\n"
    "// Leading comments about service Service0.\n"
    "service Service0 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Foo) returns (Empty) {\n"
    "  }\n"
    "}\n"
    "// Leading comments about service Service1.\n"
    "service Service1 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Empty) returns (Empty) {\n"
    "  }\n"
    "}\n"
    "// Leading comments about service Service.\n"
    "service Service2 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Empty) returns (Foo) {\n"
    "  }\n"
    "}\n";

class HasMessageWithMapFieldTest : public testing::Test {
 public:
  HasMessageWithMapFieldTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/cloud/foo/service.proto"), kFooServiceProto}}),
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
  AbortingErrorCollector collector_;
  StringSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  DescriptorPool pool_;
};

TEST_F(HasMessageWithMapFieldTest, HasRequestMessageWithMapField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasMessageWithMapField());
}

TEST_F(HasMessageWithMapFieldTest, HasResponseMessageWithMapField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(2), generator_context.get());
  EXPECT_TRUE(g.HasMessageWithMapField());
}

TEST_F(HasMessageWithMapFieldTest, HasNoMessageWithMapField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(1), generator_context.get());
  EXPECT_FALSE(g.HasMessageWithMapField());
}

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

char const* const kDurationProto =
    "syntax = \"proto3\";\n"
    "package google.protobuf;\n"
    "message Duration {\n"
    "  int64 seconds = 1;\n"
    "  int32 nanos = 2;\n"
    "}\n";

char const* const kMethodSignatureServiceProto =
    "syntax = \"proto3\";\n"
    "package google.protobuf;\n"
    "import \"google/api/client.proto\";\n"
    "import \"google/protobuf/duration.proto\";\n"

    "// Leading comments about message Foo.\n"
    "message Foo {\n"
    "  google.protobuf.Duration duration = 1;\n"
    "}\n"
    "// Leading comments about message Bar.\n"
    "message Bar {\n"
    "  string name = 1;\n"
    "}\n"
    "// Leading comments about message Empty.\n"
    "message Empty {}\n"
    "// Leading comments about service Service0.\n"
    "service Service0 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Foo) returns (Empty) {\n"
    "    option (google.api.method_signature) = \"duration\";\n"
    "  }\n"
    "}\n"
    "// Leading comments about service Service1.\n"
    "service Service1 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Bar) returns (Empty) {\n"
    "    option (google.api.method_signature) = \" name\";\n"
    "  }\n"
    "}\n";

class MethodSignatureWellKnownProtobufTypeIncludesTest : public testing::Test {
 public:
  MethodSignatureWellKnownProtobufTypeIncludesTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/client.proto"), kClientProto},
            {std::string("google/protobuf/duration.proto"), kDurationProto},
            {std::string("google/cloud/foo/service.proto"),
             kMethodSignatureServiceProto}}),
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
  AbortingErrorCollector collector_;
  StringSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  DescriptorPool pool_;
};

TEST_F(MethodSignatureWellKnownProtobufTypeIncludesTest,
       HasSignatureWithDurationField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  auto includes = g.MethodSignatureWellKnownProtobufTypeIncludes();
  EXPECT_THAT(includes, Contains("google/protobuf/duration.pb.h"));
}

TEST_F(MethodSignatureWellKnownProtobufTypeIncludesTest,
       HasSignatureWithoutWellKnownTypeField) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(1), generator_context.get());
  auto includes = g.MethodSignatureWellKnownProtobufTypeIncludes();
  EXPECT_THAT(includes, IsEmpty());
}

char const* const kStreamingServiceProto =
    "syntax = \"proto3\";\n"
    "package google.protobuf;\n"
    "// Leading comments about message Foo.\n"
    "message Foo {\n"
    "  string baz = 1;\n"
    "  map<string, string> labels = 2;\n"
    "}\n"
    "// Leading comments about message Empty.\n"
    "message Bar {\n"
    "  int32 x = 1;\n"
    "}\n"
    "// Leading comments about service Service0.\n"
    "service Service0 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(Foo) returns (stream Bar) {\n"
    "  }\n"
    "  // Leading comments about rpc Method1.\n"
    "  rpc Method1(stream Foo) returns (Bar) {\n"
    "  }\n"
    "  // Leading comments about rpc Method2.\n"
    "  rpc Method2(stream Foo) returns (stream Bar) {\n"
    "  }\n"
    "  // Leading comments about rpc Method3.\n"
    "  rpc Method3(Foo) returns (Bar) {\n"
    "  }\n"
    "}\n"
    "service Service1 {\n"
    "  // Leading comments about rpc Method0.\n"
    "  rpc Method0(stream Foo) returns (Bar) {\n"
    "  }\n"
    "  // Leading comments about rpc Method1.\n"
    "  rpc Method1(stream Foo) returns (stream Bar) {\n"
    "  }\n"
    "  // Leading comments about rpc Method2.\n"
    "  rpc Method2(Foo) returns (Bar) {\n"
    "  }\n"
    "}\n";

class StreamingReadTest : public testing::Test {
 public:
  StreamingReadTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/cloud/foo/streaming.proto"),
             kStreamingServiceProto}}),
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
  AbortingErrorCollector collector_;
  StringSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  DescriptorPool pool_;
};

TEST_F(StreamingReadTest, HasStreamingReadTrue) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/streaming.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasStreamingReadMethod());
}

TEST_F(StreamingReadTest, HasStreamingReadFalse) {
  FileDescriptor const* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/streaming.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(1), generator_context.get());
  EXPECT_FALSE(g.HasStreamingReadMethod());
}

TEST(ServiceCodeGeneratorTest, HasWriteStreaming) {
  auto constexpr kBidirStreamingServiceProto = R"""(
syntax = "proto3";
package google.protobuf;
// Leading comments about message Foo.
message Foo {
  string baz = 1;
  map<string, string> labels = 2;
}
// Leading comments about message Empty.
message Bar {
  int32 x = 1;
}

// This service has a streaming write RPC.
service Service0 {
  // Leading comments about rpc Method0.
  rpc Method0(Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method1.
  rpc Method1(stream Foo) returns (Bar) {
  }
  // Leading comments about rpc Method2.
  rpc Method2(stream Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method3.
  rpc Method3(Foo) returns (Bar) {
  }
}

// This service has client-streaming (aka streaming-reads) and bidir-streaming
// (aka streaming-read-write) RPCs, but does not have streaming write RPCs.
service Service1 {
  // Leading comments about rpc Method0.
  rpc Method0(Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method1.
  rpc Method1(stream Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method2.
  rpc Method2(Foo) returns (Bar) {
  }
}
)""";

  StringSourceTree source_tree(std::map<std::string, std::string>{
      {"google/cloud/foo/streaming.proto", kBidirStreamingServiceProto}});
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db(
      &source_tree);
  google::protobuf::SimpleDescriptorDatabase simple_db;
  FileDescriptorProto file_proto;
  // we need descriptor.proto to be accessible by the pool
  // since our test file imports it
  FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto);
  simple_db.Add(file_proto);
  google::protobuf::MergedDescriptorDatabase merged_db(&simple_db,
                                                       &source_tree_db);
  AbortingErrorCollector collector;
  DescriptorPool pool(&merged_db, &collector);

  FileDescriptor const* service_file_descriptor =
      pool.FindFileByName("google/cloud/foo/streaming.proto");

  auto generator_context = absl::make_unique<MockGeneratorContext>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .Times(2)
      .WillRepeatedly(
          [](std::string const&) { return new MockZeroCopyOutputStream(); });

  TestGenerator g_service_0(service_file_descriptor->service(0),
                            generator_context.get());
  EXPECT_TRUE(g_service_0.HasStreamingWriteMethod());

  TestGenerator g_service_1(service_file_descriptor->service(1),
                            generator_context.get());
  EXPECT_FALSE(g_service_1.HasStreamingWriteMethod());
}

TEST(ServiceCodeGeneratorTest, HasBidirStreaming) {
  auto constexpr kBidirStreamingServiceProto = R"""(
syntax = "proto3";
package google.protobuf;
// Leading comments about message Foo.
message Foo {
  string baz = 1;
  map<string, string> labels = 2;
}
// Leading comments about message Empty.
message Bar {
  int32 x = 1;
}

// This service has a bidir streaming RPC.
service Service0 {
  // Leading comments about rpc Method0.
  rpc Method0(Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method1.
  rpc Method1(stream Foo) returns (Bar) {
  }
  // Leading comments about rpc Method2.
  rpc Method2(stream Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method3.
  rpc Method3(Foo) returns (Bar) {
  }
}

// This service has client-streaming (aka streaming-writes) and server-streaming
// (aka streaming-write) RPCs, but does not have bidir-streaming RPCs.
service Service1 {
  // Leading comments about rpc Method0.
  rpc Method0(stream Foo) returns (Bar) {
  }
  // Leading comments about rpc Method1.
  rpc Method1(Foo) returns (stream Bar) {
  }
  // Leading comments about rpc Method2.
  rpc Method2(Foo) returns (Bar) {
  }
}
)""";

  StringSourceTree source_tree(std::map<std::string, std::string>{
      {"google/cloud/foo/streaming.proto", kBidirStreamingServiceProto}});
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db(
      &source_tree);
  google::protobuf::SimpleDescriptorDatabase simple_db;
  FileDescriptorProto file_proto;
  // we need descriptor.proto to be accessible by the pool
  // since our test file imports it
  FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto);
  simple_db.Add(file_proto);
  google::protobuf::MergedDescriptorDatabase merged_db(&simple_db,
                                                       &source_tree_db);
  AbortingErrorCollector collector;
  DescriptorPool pool(&merged_db, &collector);

  FileDescriptor const* service_file_descriptor =
      pool.FindFileByName("google/cloud/foo/streaming.proto");

  auto generator_context = absl::make_unique<MockGeneratorContext>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .Times(2)
      .WillRepeatedly(
          [](std::string const&) { return new MockZeroCopyOutputStream(); });

  TestGenerator g_service_0(service_file_descriptor->service(0),
                            generator_context.get());
  EXPECT_TRUE(g_service_0.HasBidirStreamingMethod());

  TestGenerator g_service_1(service_file_descriptor->service(1),
                            generator_context.get());
  EXPECT_FALSE(g_service_1.HasBidirStreamingMethod());
}

TEST(ServiceCodeGeneratorTest, HasExplicitRoutingMethod) {
  auto constexpr kHttpProto = R"""(
syntax = "proto3";
package google.api;
import "google/protobuf/descriptor.proto";

extend google.protobuf.MethodOptions {
  HttpRule http = 72295728;
}
message HttpRule {
  string post = 4;
  string body = 7;
}
)""";

  auto constexpr kRoutingProto = R"""(
syntax = "proto3";
package google.api;
import "google/protobuf/descriptor.proto";

extend google.protobuf.MethodOptions {
  google.api.RoutingRule routing = 72295729;
}
message RoutingRule {
  repeated RoutingParameter routing_parameters = 2;
}
message RoutingParameter {
  string field = 1;
  string path_template = 2;
}
)""";

  auto constexpr kExplicitRoutingServiceProto = R"""(
syntax = "proto3";
package google.protobuf;
import "google/api/http.proto";
import "google/api/routing.proto";

message Foo {
  string foo = 1;
}

// This service has a method with explicit routing.
service Service0 {
  // Leading comments about rpc Method0.
  rpc Method0(Foo) returns (Foo) {
    option (google.api.http) = {
      post: "{foo=*}:method0"
      body: "*"
    };
    option (google.api.routing) = {
      routing_parameters {
        field: "foo"
      }
    };
  }
}

// This service does not have a method with explicit routing.
service Service1 {
  rpc Method0(Foo) returns (Foo) {
    option (google.api.http) = {
      post: "{foo=*}:method0"
      body: "*"
    };
  }
}
)""";

  StringSourceTree source_tree(std::map<std::string, std::string>{
      {"google/api/http.proto", kHttpProto},
      {"google/api/routing.proto", kRoutingProto},
      {"google/cloud/foo/explicit_routing_service.proto",
       kExplicitRoutingServiceProto}});
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db(
      &source_tree);
  google::protobuf::SimpleDescriptorDatabase simple_db;
  FileDescriptorProto file_proto;
  // we need descriptor.proto to be accessible by the pool
  // since our test file imports it
  FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto);
  simple_db.Add(file_proto);
  google::protobuf::MergedDescriptorDatabase merged_db(&simple_db,
                                                       &source_tree_db);
  AbortingErrorCollector collector;
  DescriptorPool pool(&merged_db, &collector);

  FileDescriptor const* service_file_descriptor =
      pool.FindFileByName("google/cloud/foo/explicit_routing_service.proto");

  auto generator_context = absl::make_unique<MockGeneratorContext>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .Times(2)
      .WillRepeatedly(
          [](std::string const&) { return new MockZeroCopyOutputStream(); });

  TestGenerator g_service_0(service_file_descriptor->service(0),
                            generator_context.get());
  EXPECT_TRUE(g_service_0.HasExplicitRoutingMethod());

  TestGenerator g_service_1(service_file_descriptor->service(1),
                            generator_context.get());
  EXPECT_FALSE(g_service_1.HasExplicitRoutingMethod());
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
