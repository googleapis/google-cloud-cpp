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

#include "generator/internal/service_generator.h"
#include "absl/memory/memory.h"
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <gmock/gmock.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using google::protobuf::DescriptorPool;
using google::protobuf::FileDescriptor;
using google::protobuf::FileDescriptorProto;
using google::protobuf::ServiceDescriptorProto;
using ::testing::_;
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

class ServiceGeneratorTest : public testing::Test {
 protected:
  void SetUp() override {
    cloud_dir_file_.set_name("google/cloud/translate/v3/translation.proto");
    context_ = absl::make_unique<MockGeneratorContext>();
    header_output_ = absl::make_unique<MockZeroCopyOutputStream>();
    cc_output_ = absl::make_unique<MockZeroCopyOutputStream>();
  }

  DescriptorPool pool_;
  FileDescriptorProto cloud_dir_file_;
  std::unique_ptr<MockGeneratorContext> context_;
  std::unique_ptr<MockZeroCopyOutputStream> header_output_;
  std::unique_ptr<MockZeroCopyOutputStream> cc_output_;
};

TEST_F(ServiceGeneratorTest, GenerateSuccess) {
  ServiceDescriptorProto* s = cloud_dir_file_.add_service();
  s->set_name("TranslationService");
  const FileDescriptor* file_descriptor = pool_.BuildFile(cloud_dir_file_);

  std::map<std::string, std::string> vars = {
      {"product_path", "google/cloud/translate/"}};
  EXPECT_CALL(*header_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*cc_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*context_, Open(_))
      .WillOnce(Return(header_output_.release()))
      .WillOnce(Return(cc_output_.release()));
  ServiceGenerator service_generator(file_descriptor->service(0),
                                     context_.get(), std::move(vars));
  EXPECT_TRUE(service_generator.Generate().ok());
}

TEST_F(ServiceGeneratorTest, GenerateFailure) {
  ServiceDescriptorProto* s = cloud_dir_file_.add_service();
  s->set_name("FailureService");
  const FileDescriptor* file_descriptor = pool_.BuildFile(cloud_dir_file_);

  std::map<std::string, std::string> vars = {
      {"product_path", "google/cloud/translate/"}};
  EXPECT_CALL(*header_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*cc_output_, Next(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*context_, Open(_))
      .WillOnce(Return(header_output_.release()))
      .WillOnce(Return(cc_output_.release()));
  ServiceGenerator service_generator(file_descriptor->service(0),
                                     context_.get(), std::move(vars));
  EXPECT_FALSE(service_generator.Generate().ok());
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
