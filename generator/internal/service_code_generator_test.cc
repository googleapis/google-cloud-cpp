// Copyright 2021 Google LLC
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

#include "generator/internal/service_code_generator.h"
#include "google/cloud/log.h"
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

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::Return;

class MockGeneratorContext
    : public google::protobuf::compiler::GeneratorContext {
 public:
  ~MockGeneratorContext() override = default;
  MOCK_METHOD(google::protobuf::io::ZeroCopyOutputStream*, Open,
              (std::string const&), (override));
};

class MockZeroCopyOutputStream
    : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  ~MockZeroCopyOutputStream() override = default;
  MOCK_METHOD(bool, Next, (void**, int*), (override));
  MOCK_METHOD(void, BackUp, (int), (override));
  MOCK_METHOD(int64_t, ByteCount, (), (const, override));
  MOCK_METHOD(bool, WriteAliasedRaw, (void const*, int), (override));
  MOCK_METHOD(bool, AllowsAliasing, (), (const, override));
};

class StringSourceTree : public google::protobuf::compiler::SourceTree {
 public:
  explicit StringSourceTree(std::map<std::string, std::string> files)
      : files_(std::move(files)) {}

  google::protobuf::io::ZeroCopyInputStream* Open(
      const std::string& filename) override {
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

  void AddError(const std::string& filename, const std::string& element_name,
                const google::protobuf::Message*, ErrorLocation,
                const std::string& error_message) override {
    GCP_LOG(FATAL) << "AddError() called unexpectedly: " << filename << " ["
                   << element_name << "]: " << error_message;
  }
};

class TestGenerator : public ServiceCodeGenerator {
 public:
  TestGenerator(google::protobuf::ServiceDescriptor const* service_descriptor,
                google::protobuf::compiler::GeneratorContext* context)
      : ServiceCodeGenerator("header_path_key", service_descriptor,
                             {{"header_path_key", "header_path"}}, {},
                             context) {}

  using ServiceCodeGenerator::HasLongrunningMethod;
  using ServiceCodeGenerator::HasMessageWithMapField;
  using ServiceCodeGenerator::HasPaginatedMethod;
  using ServiceCodeGenerator::HasStreamingReadMethod;

  Status GenerateHeader() override { return {}; }
  Status GenerateCc() override { return {}; }
};

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

const char* const kFooServiceProto =
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
  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasMessageWithMapField());
}

TEST_F(HasMessageWithMapFieldTest, HasResponseMessageWithMapField) {
  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(2), generator_context.get());
  EXPECT_TRUE(g.HasMessageWithMapField());
}

TEST_F(HasMessageWithMapFieldTest, HasNoMessageWithMapField) {
  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/service.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(1), generator_context.get());
  EXPECT_FALSE(g.HasMessageWithMapField());
}

const char* const kStreamingServiceProto =
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
  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/streaming.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(0), generator_context.get());
  EXPECT_TRUE(g.HasStreamingReadMethod());
}

TEST_F(StreamingReadTest, HasStreamingReadFalse) {
  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/cloud/foo/streaming.proto");
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*generator_context, Open("header_path"))
      .WillOnce(Return(output.release()));
  TestGenerator g(service_file_descriptor->service(1), generator_context.get());
  EXPECT_FALSE(g.HasStreamingReadMethod());
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
