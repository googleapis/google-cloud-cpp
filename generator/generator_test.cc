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
#include "generator/testing/printer_mocks.h"
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
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

class GeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = absl::make_unique<generator_testing::MockGeneratorContext>();
  }
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
  auto connection_options_header_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto connection_options_cc_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto stub_header_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto stub_cc_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto logging_header_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto logging_cc_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto metadata_header_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto metadata_cc_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto stub_factory_header_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();
  auto stub_factory_cc_output =
      absl::make_unique<generator_testing::MockZeroCopyOutputStream>();

  DescriptorPool pool;
  FileDescriptorProto service_file;
  service_file.set_name("google/foo/v1/service.proto");
  service_file.add_service()->set_name("SuccessService");
  service_file.mutable_options()->set_cc_generic_services(false);
  const FileDescriptor* service_file_descriptor = pool.BuildFile(service_file);

  EXPECT_CALL(*connection_options_header_output, Next(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*connection_options_cc_output, Next(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*stub_header_output, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*stub_cc_output, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*logging_header_output, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*logging_cc_output, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*metadata_header_output, Next(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*metadata_cc_output, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*stub_factory_header_output, Next(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*stub_factory_cc_output, Next(_, _))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*context_, Open(_))
      .WillOnce(Return(connection_options_header_output.release()))
      .WillOnce(Return(connection_options_cc_output.release()))
      .WillOnce(Return(stub_header_output.release()))
      .WillOnce(Return(stub_cc_output.release()))
      .WillOnce(Return(logging_header_output.release()))
      .WillOnce(Return(logging_cc_output.release()))
      .WillOnce(Return(metadata_header_output.release()))
      .WillOnce(Return(metadata_cc_output.release()))
      .WillOnce(Return(stub_factory_header_output.release()))
      .WillOnce(Return(stub_factory_cc_output.release()));

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
