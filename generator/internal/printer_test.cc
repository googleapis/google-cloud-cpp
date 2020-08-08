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

#include "generator/internal/printer.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using testing::_;
using testing::Return;

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

class PrinterTest : public testing::Test {
 protected:
  void SetUp() override {
    generator_context_ = absl::make_unique<MockGeneratorContext>();
    output_ = new MockZeroCopyOutputStream();
  }
  std::unique_ptr<MockGeneratorContext> generator_context_;
  MockZeroCopyOutputStream* output_;
};

TEST_F(PrinterTest, PrintWithMap) {
  EXPECT_CALL(*generator_context_, Open("foo")).WillOnce(Return(output_));
  Printer printer(generator_context_.get(), "foo");
  std::map<std::string, std::string> vars;
  vars["name"] = "Inigo Montoya";
  EXPECT_CALL(*output_, Next(_, _));
  printer.Print(vars, "Hello! My name is $name$.\n");
}

TEST_F(PrinterTest, PrintWithVariableArgs) {
  EXPECT_CALL(*generator_context_, Open("foo")).WillOnce(Return(output_));
  Printer printer(generator_context_.get(), "foo");
  EXPECT_CALL(*output_, Next(_, _));
  printer.Print("Hello! My name is $name$.\n", "name", "Inigo Montoya");
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
