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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include "generator/internal/client_generator.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/connection_generator.h"
#include "generator/internal/connection_options_generator.h"
#include "generator/internal/idempotency_policy_generator.h"
#include "generator/internal/logging_decorator_generator.h"
#include "generator/internal/metadata_decorator_generator.h"
#include "generator/internal/mock_connection_generator.h"
#include "generator/internal/predicate_utils.h"
#include "generator/internal/retry_policy_generator.h"
#include "generator/internal/stub_factory_generator.h"
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
      return std::string(field->cpp_type_name());

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
    std::string method_request_setters;
    for (unsigned int j = 0; j < parameters.size(); ++j) {
      google::protobuf::FieldDescriptor const* parameter_descriptor =
          input_type->FindFieldByName(parameters[j]);
      if (parameter_descriptor->is_repeated()) {
        method_signature += absl::StrFormat(
            "std::vector<%s>", CppTypeToString(parameter_descriptor));
        method_request_setters += absl::StrFormat(
            "  *request.mutable_%s() = {%s.begin(), %s.end()};\n",
            parameters[j], parameters[j], parameters[j]);
      } else if (parameter_descriptor->type() ==
                 FieldDescriptor::TYPE_MESSAGE) {
        method_signature += CppTypeToString(parameter_descriptor);
        method_request_setters += absl::StrFormat(
            "  *request.mutable_%s() = %s;\n", parameters[j], parameters[j]);
      } else {
        method_signature += CppTypeToString(parameter_descriptor);
        method_request_setters += absl::StrFormat("  request.set_%s(%s);\n",
                                                  parameters[j], parameters[j]);
      }

      switch (parameter_descriptor->cpp_type()) {
        case FieldDescriptor::CPPTYPE_STRING:
        case FieldDescriptor::CPPTYPE_MESSAGE:
          method_signature += absl::StrFormat(" const& %s", parameters[j]);
          break;
        default:
          method_signature += absl::StrFormat(" %s", parameters[j]);
      }

      if (j < parameters.size() - 1) {
        method_signature += ", ";
      }
    }
    std::string key = "method_signature" + std::to_string(i);
    method_vars[key] = method_signature;
    std::string key2 = "method_request_setters" + std::to_string(i);
    method_vars[key2] = method_request_setters;
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

std::string DefaultIdempotencyFromHttpOperation(
    google::protobuf::MethodDescriptor const& method) {
  if (!method.options().HasExtension(google::api::http)) {
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                   << ": google::api::http extension not found" << std::endl;
  }

  google::api::HttpRule http_rule =
      method.options().GetExtension(google::api::http);

  switch (http_rule.pattern_case()) {
    case google::api::HttpRule::kGet:
    case google::api::HttpRule::kPut:
      return "kIdempotent";
      break;
    case google::api::HttpRule::kPost:
    case google::api::HttpRule::kDelete:
    case google::api::HttpRule::kPatch:
      return "kNonIdempotent";
      break;
    default:
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                     << ": google::api::HttpRule not handled" << std::endl;
      std::exit(1);
  }
}

std::string ChompByValue(std::string const& s) {
  return (!s.empty() && s.back() == '\n') ? s.substr(0, s.size() - 1) : s;
}

std::string FormatClassCommentsFromServiceComments(
    google::protobuf::ServiceDescriptor const& service) {
  google::protobuf::SourceLocation service_source_location;
  if (service.GetSourceLocation(&service_source_location) &&
      !service_source_location.leading_comments.empty()) {
    std::string chomped_leading_comments =
        ChompByValue(service_source_location.leading_comments);
    std::string doxygen_formatted_comments = absl::StrCat(
        "/**\n *",
        absl::StrReplaceAll(chomped_leading_comments,
                            {{"\n\n", "\n *\n * "}, {"\n", "\n * "}}),
        "\n */");
    return absl::StrReplaceAll(doxygen_formatted_comments, {{"*  ", "* "}});
  }
  GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": " << service.full_name()
                 << " no leading_comments to format.\n";
  return {};
}

enum class MethodParameterStyle { kApiMethodSignature, kProtobufReqeust };

std::string FormatMethodCommentsFromRpcComments(
    google::protobuf::MethodDescriptor const& method,
    MethodParameterStyle parameter_style) {
  google::protobuf::SourceLocation method_source_location;
  if (method.GetSourceLocation(&method_source_location) &&
      !method_source_location.leading_comments.empty()) {
    std::vector<std::pair<std::string, std::string>> parameter_comments;
    if (parameter_style == MethodParameterStyle::kApiMethodSignature) {
      auto method_signature_extension =
          method.options().GetRepeatedExtension(google::api::method_signature);
      for (auto const& signature : method_signature_extension) {
        google::protobuf::Descriptor const* input_type = method.input_type();
        std::vector<std::string> parameters = absl::StrSplit(signature, ",");
        for (auto const& parameter : parameters) {
          google::protobuf::FieldDescriptor const* parameter_descriptor =
              input_type->FindFieldByName(parameter);
          google::protobuf::SourceLocation loc;
          parameter_descriptor->GetSourceLocation(&loc);
          std::string chomped_parameter = ChompByValue(loc.leading_comments);
          parameter_comments.emplace_back(
              parameter,
              absl::StrReplaceAll(chomped_parameter, {{"\n\n", "\n   *\n   * "},
                                                      {"\n", "\n   * "}}));
        }
      }
    } else {
      google::protobuf::Descriptor const* input_type = method.input_type();
      parameter_comments.emplace_back(
          "request",
          absl::StrCat("`", ProtoNameToCppName(input_type->full_name()), "`"));
    }

    std::string doxygen_formatted_function_comments = absl::StrReplaceAll(
        method_source_location.leading_comments, {{"\n", "\n   *"}});

    std::string parameter_comment_string;
    for (auto const& param : parameter_comments) {
      parameter_comment_string +=
          absl::StrFormat("   * @param %s %s\n", param.first, param.second);
    }
    return absl::StrCat("/**\n   *", doxygen_formatted_function_comments, "\n",
                        parameter_comment_string, "   */");
  }
  GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": " << method.full_name()
                 << " no leading_comments to format.\n";
  return {};
}
}  // namespace

VarsDictionary CreateServiceVars(
    google::protobuf::ServiceDescriptor const& descriptor,
    std::vector<std::pair<std::string, std::string>> const& initial_values) {
  VarsDictionary vars(initial_values.begin(), initial_values.end());
  vars["class_comment_block"] =
      FormatClassCommentsFromServiceComments(descriptor);
  vars["client_class_name"] = absl::StrCat(descriptor.name(), "Client");
  vars["client_cc_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()), "_client",
      GeneratedFileSuffix(), ".cc");
  vars["client_header_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()), "_client",
      GeneratedFileSuffix(), ".h");
  vars["connection_class_name"] = absl::StrCat(descriptor.name(), "Connection");
  vars["connection_cc_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()),
      "_connection", GeneratedFileSuffix(), ".cc");
  vars["connection_header_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()),
      "_connection", GeneratedFileSuffix(), ".h");
  vars["connection_options_header_path"] = absl::StrCat(
      vars["product_path"], "connection_options", GeneratedFileSuffix(), ".h");
  vars["connection_options_cc_path"] = absl::StrCat(
      vars["product_path"], "connection_options", GeneratedFileSuffix(), ".cc");
  vars["grpc_stub_fqn"] = ProtoNameToCppName(descriptor.full_name());
  vars["idempotency_class_name"] =
      absl::StrCat(descriptor.name(), "ConnectionIdempotencyPolicy");
  vars["idempotency_policy_cc_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()),
      "_connection_idempotency_policy", GeneratedFileSuffix(), ".cc");
  vars["idempotency_policy_header_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()),
      "_connection_idempotency_policy", GeneratedFileSuffix(), ".h");
  vars["logging_class_name"] = absl::StrCat(descriptor.name(), "Logging");
  vars["logging_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()),
                   "_logging_decorator", GeneratedFileSuffix(), ".cc");
  vars["logging_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()),
                   "_logging_decorator", GeneratedFileSuffix(), ".h");
  vars["metadata_class_name"] = absl::StrCat(descriptor.name(), "Metadata");
  vars["metadata_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()),
                   "_metadata_decorator", GeneratedFileSuffix(), ".cc");
  vars["metadata_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()),
                   "_metadata_decorator", GeneratedFileSuffix(), ".h");
  vars["mock_connection_class_name"] =
      absl::StrCat("Mock", descriptor.name(), "Connection");
  vars["mock_connection_header_path"] =
      absl::StrCat(vars["product_path"], "mocks/mock_",
                   ServiceNameToFilePath(descriptor.name()), "_connection",
                   GeneratedFileSuffix(), ".h");
  vars["product_namespace"] = BuildNamespaces(vars["product_path"])[2];
  vars["product_internal_namespace"] =
      BuildNamespaces(vars["product_path"], NamespaceType::kInternal)[2];
  vars["proto_file_name"] = descriptor.file()->name();
  vars["proto_grpc_header_path"] = absl::StrCat(
      absl::StripSuffix(descriptor.file()->name(), ".proto"), ".grpc.pb.h");
  vars["retry_policy_header_path"] = absl::StrCat(
      vars["product_path"], "retry_policy", GeneratedFileSuffix(), ".h");
  vars["retry_traits_header_path"] =
      absl::StrCat(vars["product_path"], "retry_traits", ".h");
  vars["service_endpoint"] =
      descriptor.options().GetExtension(google::api::default_host);
  vars["stub_class_name"] = absl::StrCat(descriptor.name(), "Stub");
  vars["stub_cc_path"] = absl::StrCat(vars["product_path"], "internal/",
                                      ServiceNameToFilePath(descriptor.name()),
                                      "_stub", GeneratedFileSuffix(), ".cc");
  vars["stub_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub",
                   GeneratedFileSuffix(), ".h");
  vars["stub_factory_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub_factory",
                   GeneratedFileSuffix(), ".cc");
  vars["stub_factory_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub_factory",
                   GeneratedFileSuffix(), ".h");
  return vars;
}

std::map<std::string, VarsDictionary> CreateMethodVars(
    google::protobuf::ServiceDescriptor const& service) {
  std::map<std::string, VarsDictionary> service_methods_vars;
  for (int i = 0; i < service.method_count(); i++) {
    auto const& method = *service.method(i);
    VarsDictionary method_vars;
    method_vars["method_signature_comment_block"] =
        FormatMethodCommentsFromRpcComments(
            method, MethodParameterStyle::kApiMethodSignature);
    method_vars["request_comment_block"] = FormatMethodCommentsFromRpcComments(
        method, MethodParameterStyle::kProtobufReqeust);
    method_vars["default_idempotency"] =
        DefaultIdempotencyFromHttpOperation(method);
    method_vars["method_name"] = method.name();
    method_vars["method_name_snake"] = CamelCaseToSnakeCase(method.name());
    method_vars["request_type"] =
        ProtoNameToCppName(method.input_type()->full_name());
    method_vars["response_type"] =
        ProtoNameToCppName(method.output_type()->full_name());
    SetLongrunningOperationMethodVars(method, method_vars);
    if (IsPaginated(method)) {
      auto pagination_info = DeterminePagination(method);
      method_vars["range_output_field_name"] = pagination_info->first;
      method_vars["range_output_type"] =
          ProtoNameToCppName(pagination_info->second);
    }
    SetMethodSignatureMethodVars(method, method_vars);
    SetResourceRoutingMethodVars(method, method_vars);
    service_methods_vars[method.full_name()] = method_vars;
  }
  return service_methods_vars;
}

std::vector<std::unique_ptr<GeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    std::vector<std::pair<std::string, std::string>> const& vars) {
  std::vector<std::unique_ptr<GeneratorInterface>> code_generators;
  code_generators.push_back(absl::make_unique<ClientGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<ConnectionGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<ConnectionOptionsGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<IdempotencyPolicyGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<LoggingDecoratorGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<MetadataDecoratorGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<MockConnectionGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<RetryPolicyGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<StubGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  code_generators.push_back(absl::make_unique<StubFactoryGenerator>(
      service, CreateServiceVars(*service, vars), CreateMethodVars(*service),
      context));
  return code_generators;
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
