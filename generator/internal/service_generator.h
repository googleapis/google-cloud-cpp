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
namespace generator {
namespace internal {

class ServiceGenerator {
 public:
  ServiceGenerator(
      google::protobuf::ServiceDescriptor const* service_descriptor,
      google::protobuf::compiler::GeneratorContext* context);

  Status Generate() const;

 private:
  void SetVars();

  google::protobuf::ServiceDescriptor const* descriptor_;
  google::protobuf::compiler::GeneratorContext* context_;
  std::map<std::string, std::string> vars_;
  std::vector<std::unique_ptr<ClassGeneratorInterface>> class_generators_;
};

}  // namespace internal
}  // namespace generator
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SERVICE_GENERATOR_H
