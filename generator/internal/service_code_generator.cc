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

#include "generator/internal/service_code_generator.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/printer.h"
#include <google/api/client.pb.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace cloud {
namespace generator_internal {

ServiceCodeGenerator::ServiceCodeGenerator(
    std::string const& header_path_key, std::string const& cc_path_key,
    google::protobuf::ServiceDescriptor const* service_descriptor,
    VarsDictionary service_vars,
    std::map<std::string, VarsDictionary> service_method_vars,
    google::protobuf::compiler::GeneratorContext* context)
    : service_descriptor_(service_descriptor),
      service_vars_(std::move(service_vars)),
      service_method_vars_(std::move(service_method_vars)),
      header_(context, service_vars_[header_path_key]),
      cc_(context, service_vars_[cc_path_key]) {
  SetVars(service_vars_[header_path_key]);
}

ServiceCodeGenerator::ServiceCodeGenerator(
    std::string const& header_path_key,
    google::protobuf::ServiceDescriptor const* service_descriptor,
    VarsDictionary service_vars,
    std::map<std::string, VarsDictionary> service_method_vars,
    google::protobuf::compiler::GeneratorContext* context)
    : service_descriptor_(service_descriptor),
      service_vars_(std::move(service_vars)),
      service_method_vars_(std::move(service_method_vars)),
      header_(context, service_vars_[header_path_key]) {
  SetVars(service_vars_[header_path_key]);
}

VarsDictionary ServiceCodeGenerator::MergeServiceAndMethodVars(
    google::protobuf::MethodDescriptor const& method) const {
  auto vars = service_vars_;
  vars.insert(service_method_vars_.at(method.full_name()).begin(),
              service_method_vars_.at(method.full_name()).end());
  return vars;
}

void ServiceCodeGenerator::GenerateLocalIncludes(
    Printer& p, std::vector<std::string> local_includes, FileType file_type) {
  if (file_type == FileType::kCcFile) {
    std::sort(local_includes.begin() + 1, local_includes.end());
  } else {
    std::sort(local_includes.begin(), local_includes.end());
  }
  for (auto const& include : local_includes) {
    p.Print(LocalInclude(include));
  }
}

void ServiceCodeGenerator::GenerateSystemIncludes(
    Printer& p, std::vector<std::string> system_includes) {
  std::sort(system_includes.begin(), system_includes.end());
  for (auto const& include : system_includes) {
    p.Print(SystemInclude(include));
  }
}

Status ServiceCodeGenerator::OpenNamespaces(Printer& p, NamespaceType ns_type) {
  auto result = service_vars_.find("product_path");
  if (result == service_vars_.end()) {
    return Status(StatusCode::kInternal, "product_path not found in vars");
  }
  namespaces_ = BuildNamespaces(service_vars_["product_path"], ns_type);
  for (auto const& nspace : namespaces_) {
    if (nspace == "GOOGLE_CLOUD_CPP_NS") {
      p.Print("inline namespace $namespace$ {\n", "namespace", nspace);
    } else {
      p.Print("namespace $namespace$ {\n", "namespace", nspace);
    }
  }
  p.Print("\n");
  return {};
}

void ServiceCodeGenerator::CloseNamespaces(Printer& p) {
  for (auto iter = namespaces_.rbegin(); iter != namespaces_.rend(); ++iter) {
    p.Print("}  // namespace $namespace$\n", "namespace", *iter);
  }
  p.Print("\n");
}

Status ServiceCodeGenerator::Generate() {
  auto result = GenerateHeader();
  if (!result.ok()) return result;
  return GenerateCc();
}

void ServiceCodeGenerator::SetVars(absl::string_view header_path) {
  service_vars_["header_include_guard"] = absl::StrCat(
      "GOOGLE_CLOUD_CPP_", absl::AsciiStrToUpper(absl::StrReplaceAll(
                               header_path, {{"/", "_"}, {".", "_"}})));
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
