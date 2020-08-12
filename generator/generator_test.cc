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
#include "absl/memory/memory.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator {
namespace {

using google::protobuf::DescriptorPool;
using google::protobuf::FileDescriptor;
using google::protobuf::FileDescriptorProto;
using ::testing::_;
using ::testing::HasSubstr;
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

class GeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = absl::make_unique<MockGeneratorContext>();
    header_output_ = absl::make_unique<MockZeroCopyOutputStream>();
    cc_output_ = absl::make_unique<MockZeroCopyOutputStream>();
  }
  std::unique_ptr<MockGeneratorContext> context_;
  std::unique_ptr<MockZeroCopyOutputStream> header_output_;
  std::unique_ptr<MockZeroCopyOutputStream> cc_output_;
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
  DescriptorPool pool;
  FileDescriptorProto service_file;
  service_file.set_name("google/foo/v1/service.proto");
  service_file.add_service()->set_name("SuccessService");
  service_file.mutable_options()->set_cc_generic_services(false);
  const FileDescriptor* service_file_descriptor = pool.BuildFile(service_file);

  EXPECT_CALL(*header_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*cc_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*context_, Open(_))
      .WillOnce(Return(header_output_.release()))
      .WillOnce(Return(cc_output_.release()));
  std::string actual_error;
  Generator generator;
  auto result = generator.Generate(service_file_descriptor,
                                   {"product_path=google/cloud/foo"},
                                   context_.get(), &actual_error);
  EXPECT_TRUE(result);
  EXPECT_TRUE(actual_error.empty());
}

TEST_F(GeneratorTest, GenerateServicesFailure) {
  DescriptorPool pool;
  FileDescriptorProto service_file;
  service_file.set_name("google/foo/v1/service.proto");
  service_file.add_service()->set_name("FailureService");
  service_file.mutable_options()->set_cc_generic_services(false);
  const FileDescriptor* service_file_descriptor = pool.BuildFile(service_file);

  EXPECT_CALL(*header_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*cc_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*context_, Open(_))
      .WillOnce(Return(header_output_.release()))
      .WillOnce(Return(cc_output_.release()));

  std::string actual_error;
  std::string expected_error = "Failed for testing.";
  Generator generator;
  auto result = generator.Generate(service_file_descriptor,
                                   {"product_path=google/cloud/foo"},
                                   context_.get(), &actual_error);
  EXPECT_FALSE(result);
  EXPECT_EQ(actual_error, expected_error);
}

}  // namespace
}  // namespace generator
}  // namespace cloud
}  // namespace google
