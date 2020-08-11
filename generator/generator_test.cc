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
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator {
namespace {

using google::protobuf::DescriptorPool;
using google::protobuf::FileDescriptor;
using google::protobuf::FileDescriptorProto;
using ::testing::HasSubstr;

TEST(GeneratorTest, GenericService) {
  DescriptorPool pool;
  FileDescriptorProto generic_service_file;
  generic_service_file.set_name("google/generic/v1/generic.proto");
  generic_service_file.add_service()->set_name("GenericService");
  generic_service_file.mutable_options()->set_cc_generic_services(true);
  const FileDescriptor* generic_service_file_descriptor =
      pool.BuildFile(generic_service_file);

  Generator generator;
  std::string actual_error;
  std::string expected_error =
      "cpp codegen proto compiler plugin does not work with generic "
      "services. To generate cpp codegen APIs, please set \""
      "cc_generic_service = false\".";
  auto result = generator.Generate(generic_service_file_descriptor, {}, nullptr,
                                   &actual_error);
  EXPECT_FALSE(result);
  EXPECT_THAT(actual_error, HasSubstr("cc_generic_service = false"));
}

TEST(GeneratorTest, BadCommandLineArgs) {
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
  auto result =
      generator.Generate(service_file_descriptor, {}, nullptr, &actual_error);
  EXPECT_FALSE(result);
  EXPECT_EQ(actual_error, expected_error);
}

TEST(GeneratorTest, GenerateServicesSuccess) {
  DescriptorPool pool;
  FileDescriptorProto service_file;
  service_file.set_name("google/foo/v1/service.proto");
  service_file.add_service()->set_name("SuccessService");
  service_file.mutable_options()->set_cc_generic_services(false);
  const FileDescriptor* service_file_descriptor = pool.BuildFile(service_file);

  std::string actual_error;
  Generator generator;
  auto result = generator.Generate(service_file_descriptor,
                                   {"product_path=google/cloud/foo"}, nullptr,
                                   &actual_error);
  EXPECT_TRUE(result);
  EXPECT_TRUE(actual_error.empty());
}

TEST(GeneratorTest, GenerateServicesFailure) {
  DescriptorPool pool;
  FileDescriptorProto service_file;
  service_file.set_name("google/foo/v1/service.proto");
  service_file.add_service()->set_name("FailureService");
  service_file.mutable_options()->set_cc_generic_services(false);
  const FileDescriptor* service_file_descriptor = pool.BuildFile(service_file);

  std::string actual_error;
  std::string expected_error = "Failed for testing.";
  Generator generator;
  auto result = generator.Generate(service_file_descriptor,
                                   {"product_path=google/cloud/foo"}, nullptr,
                                   &actual_error);
  EXPECT_FALSE(result);
  EXPECT_EQ(actual_error, expected_error);
}

}  // namespace
}  // namespace generator
}  // namespace cloud
}  // namespace google
