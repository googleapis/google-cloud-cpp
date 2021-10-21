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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_STUB_FACTORY_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_STUB_FACTORY_GENERATOR_H

#include "google/cloud/status.h"
#include "generator/internal/predicate_utils.h"
#include "generator/internal/printer.h"
#include "generator/internal/service_code_generator.h"
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Generates the header file and cc file for the Stub factory function.
 */
class StubFactoryGenerator : public ServiceCodeGenerator {
 public:
  StubFactoryGenerator(
      google::protobuf::ServiceDescriptor const* service_descriptor,
      VarsDictionary service_vars,
      std::map<std::string, VarsDictionary> service_method_vars,
      google::protobuf::compiler::GeneratorContext* context);

  ~StubFactoryGenerator() override = default;

  StubFactoryGenerator(StubFactoryGenerator const&) = delete;
  StubFactoryGenerator& operator=(StubFactoryGenerator const&) = delete;
  StubFactoryGenerator(StubFactoryGenerator&&) = default;
  StubFactoryGenerator& operator=(StubFactoryGenerator&&) = default;

 private:
  Status GenerateHeader() override;
  Status GenerateCc() override;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_STUB_FACTORY_GENERATOR_H
