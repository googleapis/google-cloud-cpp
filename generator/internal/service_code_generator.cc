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
#include "google/cloud/internal/absl_flat_hash_map_quiet.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
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
namespace {

absl::optional<std::string> IncludePathForWellKnownProtobufType(
    google::protobuf::FieldDescriptor const& parameter) {
  // This hash is not intended to be comprehensive. Problematic types and their
  // includes should be added as needed.
  static auto const* const kTypeIncludeMap =
      new absl::flat_hash_map<std::string, std::string>(
          {{"google.protobuf.Duration", "google/protobuf/duration.pb.h"}});
  if (parameter.type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
    auto iter = kTypeIncludeMap->find(parameter.message_type()->full_name());
    if (iter != kTypeIncludeMap->end()) {
      return iter->second;
    }
  }
  return {};
}

}  // namespace

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
  assert(service_descriptor != nullptr);
  assert(context != nullptr);
  SetVars(service_vars_[header_path_key]);
  std::vector<std::string> omitted_rpcs;
  auto iter = service_vars_.find("omitted_rpcs");
  if (iter != service_vars_.end()) {
    omitted_rpcs = absl::StrSplit(iter->second, ',');
  }
  for (int i = 0; i < service_descriptor_->method_count(); ++i) {
    if (!internal::Contains(omitted_rpcs,
                            service_descriptor_->method(i)->name())) {
      methods_.emplace_back(*service_descriptor_->method(i));
    }
  }
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
  assert(service_descriptor != nullptr);
  assert(context != nullptr);
  SetVars(service_vars_[header_path_key]);
  std::vector<std::string> omitted_rpcs;
  auto iter = service_vars_.find("omitted_rpcs");
  if (iter != service_vars_.end()) {
    omitted_rpcs = absl::StrSplit(iter->second, ',');
  }
  for (int i = 0; i < service_descriptor_->method_count(); ++i) {
    if (!internal::Contains(omitted_rpcs,
                            service_descriptor_->method(i)->name())) {
      methods_.emplace_back(*service_descriptor_->method(i));
    }
  }
}

bool ServiceCodeGenerator::HasLongrunningMethod() const {
  return std::any_of(methods_.begin(), methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsLongrunningOperation(m);
                     });
}

bool ServiceCodeGenerator::HasPaginatedMethod() const {
  return std::any_of(methods_.begin(), methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsPaginated(m);
                     });
}

bool ServiceCodeGenerator::HasMessageWithMapField() const {
  for (auto method : methods_) {
    auto const* const request = method.get().input_type();
    auto const* const response = method.get().output_type();
    for (int j = 0; j < request->field_count(); ++j) {
      if (request->field(j)->is_map()) {
        return true;
      }
    }
    for (int k = 0; k < response->field_count(); ++k) {
      if (response->field(k)->is_map()) {
        return true;
      }
    }
  }
  return false;
}

bool ServiceCodeGenerator::HasStreamingReadMethod() const {
  return std::any_of(methods_.begin(), methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsStreamingRead(m);
                     });
}

std::vector<std::string>
ServiceCodeGenerator::MethodSignatureWellKnownProtobufTypeIncludes() const {
  std::vector<std::string> include_paths;
  for (auto method : methods_) {
    auto method_signature_extension =
        method.get().options().GetRepeatedExtension(
            google::api::method_signature);
    google::protobuf::Descriptor const* input_type = method.get().input_type();
    for (auto const& extension : method_signature_extension) {
      std::vector<std::string> parameters = absl::StrSplit(extension, ',');
      for (auto const& parameter : parameters) {
        auto path = IncludePathForWellKnownProtobufType(
            *input_type->FindFieldByName(parameter));
        if (path) include_paths.push_back(*path);
      }
    }
  }
  return include_paths;
}

VarsDictionary const& ServiceCodeGenerator::vars() const {
  return service_vars_;
}

std::string ServiceCodeGenerator::vars(std::string const& key) const {
  auto iter = service_vars_.find(key);
  if (iter == service_vars_.end()) {
    GCP_LOG(FATAL) << key << " not found in service_vars_\n";
  }
  return iter->second;
}

std::vector<
    std::reference_wrapper<google::protobuf::MethodDescriptor const>> const&
ServiceCodeGenerator::methods() const {
  return methods_;
}

VarsDictionary ServiceCodeGenerator::MergeServiceAndMethodVars(
    google::protobuf::MethodDescriptor const& method) const {
  auto vars = service_vars_;
  vars.insert(service_method_vars_.at(method.full_name()).begin(),
              service_method_vars_.at(method.full_name()).end());
  return vars;
}

void ServiceCodeGenerator::HeaderLocalIncludes(
    std::vector<std::string> const& local_includes) {
  GenerateLocalIncludes(header_, local_includes);
}

void ServiceCodeGenerator::CcLocalIncludes(
    std::vector<std::string> const& local_includes) {
  GenerateLocalIncludes(cc_, local_includes, FileType::kCcFile);
}

void ServiceCodeGenerator::HeaderSystemIncludes(
    std::vector<std::string> const& system_includes) {
  GenerateSystemIncludes(header_, system_includes);
}

void ServiceCodeGenerator::CcSystemIncludes(
    std::vector<std::string> const& system_includes) {
  GenerateSystemIncludes(cc_, system_includes);
}

Status ServiceCodeGenerator::HeaderOpenNamespaces(NamespaceType ns_type) {
  return OpenNamespaces(header_, ns_type);
}

void ServiceCodeGenerator::HeaderCloseNamespaces() {
  CloseNamespaces(header_);
  HeaderPrint("\n");
}

Status ServiceCodeGenerator::CcOpenNamespaces(NamespaceType ns_type) {
  return OpenNamespaces(cc_, ns_type);
}

void ServiceCodeGenerator::CcCloseNamespaces() { CloseNamespaces(cc_); }

void ServiceCodeGenerator::HeaderPrint(std::string const& text) {
  header_.Print(service_vars_, text);
}

void ServiceCodeGenerator::HeaderPrint(
    std::vector<PredicatedFragment<void>> const& text) {
  for (auto const& fragment : text) {
    header_.Print(service_vars_, fragment());
  }
}

Status ServiceCodeGenerator::HeaderPrintMethod(
    google::protobuf::MethodDescriptor const& method,
    std::vector<MethodPattern> const& patterns, char const* file, int line) {
  return PrintMethod(method, header_, MergeServiceAndMethodVars(method),
                     patterns, file, line);
}

void ServiceCodeGenerator::CcPrint(std::string const& text) {
  cc_.Print(service_vars_, text);
}

void ServiceCodeGenerator::CcPrint(
    std::vector<PredicatedFragment<void>> const& text) {
  for (auto const& fragment : text) {
    cc_.Print(service_vars_, fragment());
  }
}

Status ServiceCodeGenerator::CcPrintMethod(
    google::protobuf::MethodDescriptor const& method,
    std::vector<MethodPattern> const& patterns, char const* file, int line) {
  return PrintMethod(method, cc_, MergeServiceAndMethodVars(method), patterns,
                     file, line);
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
    if (nspace == "GOOGLE_CLOUD_CPP_GENERATED_NS") {
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
