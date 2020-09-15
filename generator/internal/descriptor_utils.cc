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

#include "generator/internal/descriptor_utils.h"
#include "google/cloud/log.h"
// TODO(#4501) - fix by doing #include <absl/...>
#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif  // _MSC_VER
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#if _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER
// TODO(#4501) - end
#include "generator/internal/codegen_utils.h"
#include "generator/internal/predicate_utils.h"
#include "generator/internal/stub_generator.h"
#include <google/api/client.pb.h>
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/compiler/code_generator.h>
#include <regex>
#include <string>

using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::ServiceDescriptor;

namespace google {
namespace cloud {
namespace generator_internal {
namespace {
std::string CppTypeToString(FieldDescriptor const* field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
    case FieldDescriptor::CPPTYPE_INT64:
    case FieldDescriptor::CPPTYPE_UINT32:
    case FieldDescriptor::CPPTYPE_UINT64:
      return std::string("std::") + std::string(field->cpp_type_name()) + "_t";

    case FieldDescriptor::CPPTYPE_DOUBLE:
    case FieldDescriptor::CPPTYPE_FLOAT:
    case FieldDescriptor::CPPTYPE_BOOL:
    case FieldDescriptor::CPPTYPE_ENUM:
    case FieldDescriptor::CPPTYPE_STRING:
      return std::string("std::") + std::string(field->cpp_type_name());

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoNameToCppName(field->message_type()->full_name());
  }
  GCP_LOG(FATAL) << "FieldDescriptor " << std::string(field->cpp_type_name())
                 << " not handled.";
  std::exit(1);
}

void SetLongrunningOperationMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  if (method.output_type()->full_name() == "google.longrunning.Operation") {
    auto operation_info =
        method.options().GetExtension(google::longrunning::operation_info);
    method_vars["longrunning_metadata_type"] =
        ProtoNameToCppName(operation_info.metadata_type());
    method_vars["longrunning_response_type"] =
        ProtoNameToCppName(operation_info.response_type());
    if (method_vars["longrunning_response_type"] == "::Backup") {
      method_vars["longrunning_response_type"] =
          "::google::spanner::admin::database::v1::Backup";
    }

    method_vars["longrunning_deduced_response_type"] =
        operation_info.response_type() == "google.protobuf.Empty"
            ? ProtoNameToCppName(operation_info.metadata_type())
            : ProtoNameToCppName(operation_info.response_type());
    if (method_vars["longrunning_deduced_response_type"] == "::Backup") {
      method_vars["longrunning_deduced_response_type"] =
          "::google::spanner::admin::database::v1::Backup";
    }
  }
}

void SetMethodSignatureMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  auto method_signature_extension =
      method.options().GetRepeatedExtension(google::api::method_signature);
  for (int i = 0; i < method_signature_extension.size(); ++i) {
    google::protobuf::Descriptor const* input_type = method.input_type();
    std::vector<std::string> parameters =
        absl::StrSplit(method_signature_extension[i], ",");
    std::string method_signature;
    for (unsigned int j = 0; j < parameters.size() - 1; ++j) {
      google::protobuf::FieldDescriptor const* parameter =
          input_type->FindFieldByName(parameters[j]);
      method_signature += CppTypeToString(parameter);
      method_signature += " const& ";
      method_signature += parameters[j];
      method_signature += ", ";
    }
    google::protobuf::FieldDescriptor const* parameter =
        input_type->FindFieldByName(parameters[parameters.size() - 1]);
    method_signature += CppTypeToString(parameter);
    method_signature += " const& ";
    method_signature += parameters[parameters.size() - 1];
    std::string key = "method_signature" + std::to_string(i);
    method_vars[key] = method_signature;
  }
}

void SetResourceRoutingMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  if (!method.options().HasExtension(google::api::http)) return;
  google::api::HttpRule http_rule =
      method.options().GetExtension(google::api::http);

  std::string url_pattern;
  switch (http_rule.pattern_case()) {
    case google::api::HttpRule::kGet:
      url_pattern = http_rule.get();
      break;
    case google::api::HttpRule::kPut:
      url_pattern = http_rule.put();
      break;
    case google::api::HttpRule::kPost:
      url_pattern = http_rule.post();
      break;
    case google::api::HttpRule::kDelete:
      url_pattern = http_rule.delete_();
      break;
    case google::api::HttpRule::kPatch:
      url_pattern = http_rule.patch();
      break;
    default:
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                     << ": google::api::HttpRule not handled" << std::endl;
      std::exit(1);
  }

  std::regex url_pattern_regex(R"(.*\{(.*)=(.*)\}.*)");
  std::smatch match;
  std::regex_match(url_pattern, match, url_pattern_regex);
  method_vars["method_request_url_path"] = match[0];
  method_vars["method_request_url_substitution"] = match[2];
  std::string param = match[1];
  method_vars["method_request_param_key"] = param;
  std::vector<std::string> chunks = absl::StrSplit(param, std::string("."));
  if (chunks.size() > 1) {
    std::string value;
    unsigned int i = 0;
    for (; i < chunks.size() - 1; ++i) {
      value += chunks[i] + "().";
    }
    value += chunks[i] + "()";
    method_vars["method_request_param_value"] = value;
  } else {
    method_vars["method_request_param_value"] = param + "()";
  }
  method_vars["method_request_body"] = http_rule.body();
}
}  // namespace

VarsDictionary CreateServiceVars(
    google::protobuf::ServiceDescriptor const& descriptor,
    std::vector<std::pair<std::string, std::string>> const& initial_values) {
  VarsDictionary vars(initial_values.begin(), initial_values.end());
  vars["class_comment_block"] = "// TODO: pull in comments";
  vars["client_class_name"] = absl::StrCat(descriptor.name(), "Client");
  vars["grpc_stub_fqn"] = ProtoNameToCppName(descriptor.full_name());
  vars["logging_class_name"] = absl::StrCat(descriptor.name(), "Logging");
  vars["metadata_class_name"] = absl::StrCat(descriptor.name(), "Metadata");
  vars["proto_file_name"] = descriptor.file()->name();
  vars["service_endpoint"] =
      descriptor.options().GetExtension(google::api::default_host);
  vars["stub_cc_path"] = absl::StrCat(vars["product_path"], "internal/",
                                      ServiceNameToFilePath(descriptor.name()),
                                      "_stub", GeneratedFileSuffix(), ".cc");
  vars["stub_class_name"] = absl::StrCat(descriptor.name(), "Stub");
  vars["stub_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub",
                   GeneratedFileSuffix(), ".h");
  return vars;
}

std::map<std::string, VarsDictionary> CreateMethodVars(
    google::protobuf::ServiceDescriptor const& service) {
  std::map<std::string, VarsDictionary> service_methods_vars;
  for (int i = 0; i < service.method_count(); i++) {
    auto method = service.method(i);
    VarsDictionary method_vars;
    method_vars["method_name"] = method->name();
    method_vars["method_name_snake"] = CamelCaseToSnakeCase(method->name());
    method_vars["request_type"] =
        ProtoNameToCppName(method->input_type()->full_name());
    method_vars["response_type"] =
        ProtoNameToCppName(method->output_type()->full_name());
    SetLongrunningOperationMethodVars(*method, method_vars);
    if (IsPaginated(*method)) {
      auto pagination_info = DeterminePagination(*method);
      method_vars["range_output_field_name"] = pagination_info->first;
      method_vars["range_output_type"] =
          ProtoNameToCppName(pagination_info->second);
    }
    SetMethodSignatureMethodVars(*method, method_vars);
    SetResourceRoutingMethodVars(*method, method_vars);
    service_methods_vars[method->full_name()] = method_vars;
  }
  return service_methods_vars;
}

std::vector<std::unique_ptr<ClassGeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    std::vector<std::pair<std::string, std::string>> const& vars) {
  std::vector<std::unique_ptr<ClassGeneratorInterface>> class_generators;
  class_generators.push_back(absl::make_unique<StubGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  return class_generators;
}

Status PrintMethod(google::protobuf::MethodDescriptor const& method,
                   Printer& printer, VarsDictionary const& vars,
                   std::vector<MethodPattern> const& patterns, char const* file,
                   int line) {
  std::vector<MethodPattern> matching_patterns;
  for (auto const& p : patterns) {
    if (p(method)) {
      matching_patterns.push_back(p);
    }
  }

  if (matching_patterns.empty())
    return Status(StatusCode::kNotFound,
                  absl::StrCat(file, ":", line, ": no matching patterns for: ",
                               method.full_name()));
  if (matching_patterns.size() > 1)
    return Status(
        StatusCode::kInternal,
        absl::StrCat(file, ":", line, ": more than one pattern found for: ",
                     method.full_name()));
  for (auto const& f : matching_patterns[0].fragments()) {
    printer.Print(line, file, vars, f(method));
  }
  return {};
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
