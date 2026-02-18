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

#include "generator/generator.h"
#include "generator/internal/printer.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include "generator/testing/printer_mocks.h"
#include "google/protobuf/descriptor.pb.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
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

char const* const kSuccessServiceProto =
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
  generator_testing::ErrorCollector collector_;
  generator_testing::FakeSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  void SetUp() override {
    context_ = std::make_unique<generator_testing::MockGeneratorContext>();
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
  FileDescriptor const* generic_service_file_descriptor =
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
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);

  Generator generator;
  std::string actual_error;
  std::string expected_error =
      "--cpp_codegen_opt=product_path=<path> must be specified.";
  auto result = generator.Generate(service_file_descriptor, {}, context_.get(),
                                   &actual_error);
  EXPECT_FALSE(result);
  EXPECT_EQ(actual_error, expected_error);
}

}  // namespace
}  // namespace generator
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
