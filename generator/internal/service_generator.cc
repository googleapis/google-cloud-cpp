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
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/stub_generator.h"
#include <google/api/client.pb.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace cloud {
namespace generator_internal {

ServiceGenerator::ServiceGenerator(
    google::protobuf::ServiceDescriptor const* service_descriptor,
    google::protobuf::compiler::GeneratorContext* context,
    std::map<std::string, std::string> command_line_vars)
    : descriptor_(service_descriptor), vars_(std::move(command_line_vars)) {
  SetVars();
  class_generators_.push_back(
      absl::make_unique<StubGenerator>(service_descriptor, vars_, context));
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
  vars_["stub_class_name"] = absl::StrCat(descriptor_->name(), "Stub");
  vars_["stub_header_path"] =
      absl::StrCat(absl::StrCat(vars_["product_path"], "internal/",
                                ServiceNameToFilePath(descriptor_->name()),
                                "_stub", GeneratedFileSuffix(), ".h"));
  vars_["stub_cc_path"] =
      absl::StrCat(absl::StrCat(vars_["product_path"], "internal/",
                                ServiceNameToFilePath(descriptor_->name()),
                                "_stub", GeneratedFileSuffix(), ".cc"));
  vars_["client_class_name"] = absl::StrCat(descriptor_->name(), "Client");
  vars_["metadata_class_name"] = absl::StrCat(descriptor_->name(), "Metadata");
  vars_["logging_class_name"] = absl::StrCat(descriptor_->name(), "Logging");
  vars_["proto_file_name"] = descriptor_->file()->name();
  vars_["class_comment_block"] = "// TODO: pull in comments";
  vars_["grpc_stub_fqn"] = ProtoNameToCppName(descriptor_->full_name());
  vars_["service_endpoint"] =
      descriptor_->options().GetExtension(google::api::default_host);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
