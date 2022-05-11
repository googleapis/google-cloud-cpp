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
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/printer.h"
#include <google/api/client.pb.h>
#include <google/protobuf/descriptor.h>
#include <unordered_map>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

absl::optional<std::string> IncludePathForWellKnownProtobufType(
    google::protobuf::FieldDescriptor const& parameter) {
  // This hash is not intended to be comprehensive. Problematic types and their
  // includes should be added as needed.
  static auto const* const kTypeIncludeMap =
      new std::unordered_map<std::string, std::string>(
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
  SetMethods();
  auto e = service_vars_.find("backwards_compatibility_namespace_alias");
  define_backwards_compatibility_namespace_alias_ =
      (e != service_vars_.end() && e->second == "true");
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
  SetMethods();
}

bool ServiceCodeGenerator::HasLongrunningMethod() const {
  return std::any_of(methods_.begin(), methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsLongrunningOperation(m);
                     });
}

bool ServiceCodeGenerator::HasAsyncMethod() const {
  return !async_methods_.empty() || HasLongrunningMethod();
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

bool ServiceCodeGenerator::HasAsynchronousStreamingReadMethod() const {
  return std::any_of(async_methods_.begin(), async_methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsStreamingRead(m);
                     });
}

bool ServiceCodeGenerator::HasStreamingWriteMethod() const {
  return std::any_of(methods_.begin(), methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsStreamingWrite(m);
                     });
}

bool ServiceCodeGenerator::HasBidirStreamingMethod() const {
  return std::any_of(methods_.begin(), methods_.end(),
                     [](google::protobuf::MethodDescriptor const& m) {
                       return IsBidirStreaming(m);
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
      std::vector<std::string> parameters =
          absl::StrSplit(extension, ',', absl::SkipEmpty());
      for (auto& parameter : parameters) {
        absl::StripAsciiWhitespace(&parameter);
        auto path = IncludePathForWellKnownProtobufType(
            *input_type->FindFieldByName(parameter));
        if (path) include_paths.push_back(*path);
      }
    }
  }
  return include_paths;
}

bool ServiceCodeGenerator::IsDeprecatedMethodSignature(
    google::protobuf::MethodDescriptor const& method,
    int method_signature_number) const {
  auto method_vars = service_method_vars_.find(method.full_name());
  if (method_vars == service_method_vars_.end()) {
    GCP_LOG(FATAL) << method.full_name()
                   << " not found in service_method_vars_\n";
  }
  return method_vars->second.find("method_signature" +
                                  std::to_string(method_signature_number)) ==
         method_vars->second.end();
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
  HeaderPrint("\n");
  return OpenNamespaces(header_, ns_type);
}

void ServiceCodeGenerator::HeaderCloseNamespaces() {
  HeaderPrint("\n");
  CloseNamespaces(header_);
}

Status ServiceCodeGenerator::CcOpenNamespaces(NamespaceType ns_type) {
  CcPrint("\n");
  return OpenNamespaces(cc_, ns_type);
}

void ServiceCodeGenerator::CcCloseNamespaces() {
  CcPrint("\n");
  CloseNamespaces(cc_);
}

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

void ServiceCodeGenerator::HeaderPrintMethod(
    google::protobuf::MethodDescriptor const& method, char const* file,
    int line, std::string const& text) {
  header_.Print(line, file, MergeServiceAndMethodVars(method), text);
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

void ServiceCodeGenerator::CcPrintMethod(
    google::protobuf::MethodDescriptor const& method, char const* file,
    int line, std::string const& text) {
  cc_.Print(line, file, MergeServiceAndMethodVars(method), text);
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
      p.Print("GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN\n");
    } else {
      p.Print("namespace $namespace$ {\n", "namespace", nspace);
    }
  }
  return {};
}

void ServiceCodeGenerator::CloseNamespaces(Printer& p) {
  for (auto iter = namespaces_.rbegin(); iter != namespaces_.rend(); ++iter) {
    if (*iter == "GOOGLE_CLOUD_CPP_NS") {
      p.Print("GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END\n");
      // TODO(#7463) - remove backwards compatibility namespaces
      if (define_backwards_compatibility_namespace_alias_) {
        p.Print(
            "namespace gcpcxxV1 = GOOGLE_CLOUD_CPP_NS;"
            "  // NOLINT(misc-unused-alias-decls)\n");
      }
    } else {
      p.Print("}  // namespace $namespace$\n", "namespace", *iter);
    }
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

void ServiceCodeGenerator::SetMethods() {
  auto split_arg = [this](std::string const& arg) -> std::set<std::string> {
    auto l = service_vars_.find(arg);
    if (l == service_vars_.end()) return {};
    return absl::StrSplit(l->second, ',');
  };
  auto const omitted_rpcs = split_arg("omitted_rpcs");
  auto const gen_async_rpcs = split_arg("gen_async_rpcs");

  auto service_name = service_descriptor_->name();
  for (int i = 0; i < service_descriptor_->method_count(); ++i) {
    auto const* method = service_descriptor_->method(i);
    auto method_name = method->name();
    auto qualified_method_name = absl::StrCat(service_name, ".", method_name);
    if (!internal::Contains(omitted_rpcs, method_name) &&
        !internal::Contains(omitted_rpcs, qualified_method_name)) {
      methods_.emplace_back(*method);
    }
    if (internal::Contains(gen_async_rpcs, method_name) ||
        internal::Contains(gen_async_rpcs, qualified_method_name)) {
      async_methods_.emplace_back(*method);
    }
  }
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
