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
#include "google/cloud/internal/port_platform.h"
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

TEST(PrinterTest, PrintWithMap) {
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*output, Next(_, _));
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");
  std::map<std::string, std::string> vars;
  vars["name"] = "Inigo Montoya";
  printer.Print(42, "some_file", vars, "Hello! My name is $name$.\n");
}

TEST(PrinterTest, PrintWithVariableArgs) {
  auto generator_context = absl::make_unique<MockGeneratorContext>();
  auto output = absl::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*output, Next(_, _));
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");
  printer.Print(42, "some_file", "Hello! My name is $name$.\n", "name",
                "Inigo Montoya");
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
