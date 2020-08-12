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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_STUB_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_STUB_GENERATOR_H

#include "google/cloud/status.h"
#include "generator/internal/class_generator_interface.h"
#include "generator/internal/printer.h"
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Generates the header file and cc file for the Stub class for a particular
 * service.
 */
class StubGenerator : public ClassGeneratorInterface {
 public:
  StubGenerator(google::protobuf::ServiceDescriptor const* service_descriptor,
                std::map<std::string, std::string> service_vars,
                google::protobuf::compiler::GeneratorContext* context);

  ~StubGenerator() override = default;

  StubGenerator(StubGenerator const&) = delete;
  StubGenerator& operator=(StubGenerator const&) = delete;
  StubGenerator(StubGenerator&&) = default;
  StubGenerator& operator=(StubGenerator&&) = default;

  Status Generate() override;

 private:
  void SetVars();
  Status GenerateHeader();

  google::protobuf::ServiceDescriptor const* service_descriptor_;
  std::map<std::string, std::string> vars_;
  Printer header_;
  Printer cc_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_STUB_GENERATOR_H
