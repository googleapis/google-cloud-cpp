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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SERVICE_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SERVICE_GENERATOR_H

#include "google/cloud/status.h"
#include "generator/internal/class_generator_interface.h"
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Constructs and contains a collection of ClassGeneratorInterface instances
 * each of which generates code necessary to support a single service.
 *
 * This collection of ClassGeneratorInterfaces typically contains generators
 * for Stub, decorators, Connection, and optionally Client classes. Each of
 * these ClassGeneratorInterfaces has its substituion variable map seeded with
 * key value pairs common across the service.
 */
class ServiceGenerator {
 public:
  ServiceGenerator(
      google::protobuf::ServiceDescriptor const* service_descriptor,
      google::protobuf::compiler::GeneratorContext* context,
      std::map<std::string, std::string> command_line_vars);

  Status Generate() const;

 private:
  void SetVars();

  google::protobuf::ServiceDescriptor const* descriptor_;
  std::map<std::string, std::string> vars_;
  std::vector<std::unique_ptr<ClassGeneratorInterface>> class_generators_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SERVICE_GENERATOR_H
