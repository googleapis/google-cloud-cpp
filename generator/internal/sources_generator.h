// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SOURCES_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SOURCES_GENERATOR_H

#include "generator/internal/service_code_generator.h"
#include "google/cloud/status.h"
#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/descriptor.h"
#include <map>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Generates the conglomerate source file for a service.
 *
 * This conglomerate source is essentially a concatenation of the other source
 * files emitted by the generator, for a service.
 *
 * Every translation unit (TU) that transitively includes a protobuf header
 * needs to recompile that header. To minimize build times, it is best to have
 * just one TU.
 *
 * See go/cloud-cxx:reducing-build-times for further discussion on the design,
 * plus some experimental results.
 */
class SourcesGenerator : public ServiceCodeGenerator {
 public:
  SourcesGenerator(
      google::protobuf::ServiceDescriptor const* service_descriptor,
      VarsDictionary service_vars,
      std::map<std::string, VarsDictionary> service_method_vars,
      google::protobuf::compiler::GeneratorContext* context,
      std::vector<std::string> sources,
      std::vector<MixinMethod> const& mixin_methods);

  ~SourcesGenerator() override = default;

  SourcesGenerator(SourcesGenerator const&) = delete;
  SourcesGenerator& operator=(SourcesGenerator const&) = delete;
  SourcesGenerator(SourcesGenerator&&) = default;
  SourcesGenerator& operator=(SourcesGenerator&&) = default;

 private:
  // TODO(#14198): We use the `*Header*` methods, even though this class writes
  // a `*.cc` file, because:
  // - There is no difference in implementation based on the file extension.
  // - The `ServiceCodeGenerator` constructors are not amenable to initializing
  //   `Printer cc_` only.
  Status GenerateHeader() override;
  Status GenerateCc() override { return Status(); }

  std::vector<std::string> sources_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SOURCES_GENERATOR_H
