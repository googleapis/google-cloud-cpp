// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_TESTING_PRINTER_MOCKS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_TESTING_PRINTER_MOCKS_H

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_testing {

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

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_TESTING_PRINTER_MOCKS_H
