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

#include "generator/internal/printer.h"
#include "generator/testing/printer_mocks.h"
#include "google/cloud/internal/port_platform.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::generator_testing::MockGeneratorContext;
using ::google::cloud::generator_testing::MockZeroCopyOutputStream;
using ::testing::Return;

TEST(PrinterTest, PrintWithMap) {
  auto generator_context = std::make_unique<MockGeneratorContext>();
  auto output = std::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*output, Next);
  EXPECT_CALL(*generator_context, Open("foo"))
      .WillOnce(Return(output.release()));
  Printer printer(generator_context.get(), "foo");
  VarsDictionary vars{{"name", "Inigo Montoya"}};
  printer.Print(42, "some_file", vars, "Hello! My name is $name$.\n");
}

TEST(PrinterTest, PrintWithVariableArgs) {
  auto generator_context = std::make_unique<MockGeneratorContext>();
  auto output = std::make_unique<MockZeroCopyOutputStream>();
  EXPECT_CALL(*output, Next);
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
