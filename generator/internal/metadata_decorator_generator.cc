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

#include "generator/internal/metadata_decorator_generator.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/printer.h"
#include <google/api/client.pb.h>
#include <google/protobuf/descriptor.h>
#include <absl/memory/memory.h>
#include <absl/strings/str_cat.h>

namespace google {
namespace cloud {
namespace generator {
namespace internal {

using google::protobuf::ServiceDescriptor;
using google::protobuf::compiler::GeneratorContext;

MetadataDecoratorGenerator::MetadataDecoratorGenerator(
    google::protobuf::ServiceDescriptor const* service_descriptor,
    std::map<std::string, std::string> service_vars,
    google::protobuf::compiler::GeneratorContext* context)
    : service_descriptor_(service_descriptor),
      vars_(std::move(service_vars)),
      header_(absl::make_unique<Printer>(
          context,
          absl::StrCat(ServiceNameToFilePath(service_descriptor_->name()),
                       "_metadata", GeneratedFileSuffix(), ".h"))),
      cc_(absl::make_unique<Printer>(
          context,
          absl::StrCat(ServiceNameToFilePath(service_descriptor_->name()),
                       "_metadata", GeneratedFileSuffix(), ".cc"))) {
  SetVars();
}

Status MetadataDecoratorGenerator::Generate() const { return {}; }

void MetadataDecoratorGenerator::SetVars() {
  vars_["metadata_header_include_guard_const"] =
      absl::StrCat(service_descriptor_->name(), "_METADATA", "_H_");
}

}  // namespace internal
}  // namespace generator
}  // namespace cloud
}  // namespace google
