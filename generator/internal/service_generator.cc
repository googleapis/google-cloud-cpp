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

#include "generator/internal/service_generator.h"
#include "generator/internal/client_generator.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/connection_generator.h"
#include "generator/internal/logging_decorator_generator.h"
#include "generator/internal/metadata_decorator_generator.h"
#include "generator/internal/printer.h"
#include "generator/internal/stub_generator.h"
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

ServiceGenerator::ServiceGenerator(ServiceDescriptor const* service_descriptor,
                                   GeneratorContext* context)
    : descriptor_(service_descriptor), context_(context) {
  SetVars();
  class_generators_.push_back(
      absl::make_unique<StubGenerator>(service_descriptor, vars_, context));
  class_generators_.push_back(absl::make_unique<LoggingDecoratorGenerator>(
      service_descriptor, vars_, context));
  class_generators_.push_back(absl::make_unique<MetadataDecoratorGenerator>(
      service_descriptor, vars_, context));
  class_generators_.push_back(absl::make_unique<ConnectionGenerator>(
      service_descriptor, vars_, context));
  // TODO(sdhart): adding a ClientGenerator needs to be conditional
  class_generators_.push_back(
      absl::make_unique<ClientGenerator>(service_descriptor, vars_, context));
}

Status ServiceGenerator::Generate() const {
  for (auto const& c : class_generators_) {
    auto result = c->Generate();
    if (!result.ok()) {
      return result;
    }
  }
  return {};
}

void ServiceGenerator::SetVars() {
  vars_["class_name"] = descriptor_->name();
  vars_["stub_class_name"] = absl::StrCat(descriptor_->name(), "Stub");
  vars_["metadata_class_name"] = absl::StrCat(descriptor_->name(), "Metadata");
  vars_["logging_class_name"] = absl::StrCat(descriptor_->name(), "Logging");
  vars_["proto_file_name"] = descriptor_->file()->name();
  vars_["class_comment_block"] = "// TODO: pull in comments";
  vars_["grpc_stub_fqn"] =
      internal::ProtoNameToCppName(descriptor_->full_name());
  vars_["service_endpoint"] =
      descriptor_->options().GetExtension(google::api::default_host);
}

}  // namespace internal
}  // namespace generator
}  // namespace cloud
}  // namespace google
