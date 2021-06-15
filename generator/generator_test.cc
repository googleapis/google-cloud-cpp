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

#include "generator/generator.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"
#include "generator/internal/printer.h"
#include "generator/testing/printer_mocks.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::HasSubstr;
using ::testing::Return;

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
                   << element_name << "]: " << error_message << "\n";
    std::exit(1);
  }
};

const char* const kSuccessServiceProto =
    "syntax = \"proto3\";\n"
    "package google.foo.v1;\n"
    "// Leading comments about service SuccessService.\n"
    "service SuccessService {\n"
    "}\n";

class GeneratorTest : public ::testing::Test {
 public:
  GeneratorTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/foo/v1/service.proto"),
             kSuccessServiceProto}}),
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
  void SetUp() override {
    context_ = absl::make_unique<generator_testing::MockGeneratorContext>();
  }

  DescriptorPool pool_;
  std::unique_ptr<generator_testing::MockGeneratorContext> context_;
};

TEST_F(GeneratorTest, GenericService) {
  DescriptorPool pool;
  FileDescriptorProto generic_service_file;
  generic_service_file.set_name("google/generic/v1/generic.proto");
  generic_service_file.add_service()->set_name("GenericService");
  generic_service_file.mutable_options()->set_cc_generic_services(true);
  const FileDescriptor* generic_service_file_descriptor =
      pool.BuildFile(generic_service_file);

  Generator generator;
  std::string actual_error;
  auto result = generator.Generate(generic_service_file_descriptor, {},
                                   context_.get(), &actual_error);
  EXPECT_FALSE(result);
  EXPECT_THAT(actual_error, HasSubstr("cc_generic_service = false"));
}

TEST_F(GeneratorTest, BadCommandLineArgs) {
  DescriptorPool pool;
  FileDescriptorProto service_file;
  service_file.set_name("google/foo/v1/service.proto");
  service_file.add_service()->set_name("Service");
  service_file.mutable_options()->set_cc_generic_services(false);
  const FileDescriptor* service_file_descriptor = pool.BuildFile(service_file);

  Generator generator;
  std::string actual_error;
  std::string expected_error =
      "--cpp_codegen_opt=product_path=<path> must be specified.";
  auto result = generator.Generate(service_file_descriptor, {}, context_.get(),
                                   &actual_error);
  EXPECT_FALSE(result);
  EXPECT_EQ(actual_error, expected_error);
}

TEST_F(GeneratorTest, GenerateServicesSuccess) {
  int constexpr kNumMockOutputStreams = 20;
  std::vector<std::unique_ptr<generator_testing::MockZeroCopyOutputStream>>
      mock_outputs(kNumMockOutputStreams);
  for (auto& output : mock_outputs) {
    output = absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  }

  const FileDescriptor* service_file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");

  for (auto& output : mock_outputs) {
    EXPECT_CALL(*output, Next).WillRepeatedly(Return(false));
  }

  int next_mock = 0;
  EXPECT_CALL(*context_, Open)
      .Times(kNumMockOutputStreams)
      .WillRepeatedly([&next_mock, &mock_outputs](::testing::Unused) {
        return mock_outputs[next_mock++].release();
      });

  std::string actual_error;
  Generator generator;
  auto result = generator.Generate(
      service_file_descriptor,
      {"product_path=google/cloud/foo"
       ",googleapis_commit_hash=59f97e6044a1275f83427ab7962a154c00d915b5"},
      context_.get(), &actual_error);
  EXPECT_TRUE(result);
  EXPECT_TRUE(actual_error.empty());
}

}  // namespace
}  // namespace generator
}  // namespace cloud
}  // namespace google
