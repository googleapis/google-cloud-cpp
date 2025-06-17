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

#include "generator/internal/make_generators.h"
#include "generator/generator.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include "generator/testing/printer_mocks.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
#include "google/protobuf/descriptor.pb.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::Return;

char const* const kSuccessServiceProto =
    "syntax = \"proto3\";\n"
    "package google.foo.v1;\n"
    "// Leading comments about service SuccessService.\n"
    "service SuccessService {\n"
    "}\n";

class MakeGeneratorsTest : public ::testing::Test {
 public:
  MakeGeneratorsTest()
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

TEST_F(MakeGeneratorsTest, GenerateServicesSuccess) {
  int constexpr kNumMockOutputStreams = 28;
  std::vector<std::unique_ptr<generator_testing::MockZeroCopyOutputStream>>
      mock_outputs(kNumMockOutputStreams);
  for (auto& output : mock_outputs) {
    output = std::make_unique<generator_testing::MockZeroCopyOutputStream>();
  }

  FileDescriptor const* service_file_descriptor =
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
  auto result = generator.Generate(service_file_descriptor,
                                   {"product_path=google/cloud/foo"},
                                   context_.get(), &actual_error);
  EXPECT_TRUE(result);
  EXPECT_TRUE(actual_error.empty());
}

}  // namespace
}  // namespace generator
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
