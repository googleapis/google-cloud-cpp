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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/types/variant.h"
#include "generator/internal/auth_decorator_generator.h"
#include "generator/internal/client_generator.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/connection_generator.h"
#include "generator/internal/connection_impl_generator.h"
#include "generator/internal/idempotency_policy_generator.h"
#include "generator/internal/logging_decorator_generator.h"
#include "generator/internal/metadata_decorator_generator.h"
#include "generator/internal/mock_connection_generator.h"
#include "generator/internal/option_defaults_generator.h"
#include "generator/internal/options_generator.h"
#include "generator/internal/predicate_utils.h"
#include "generator/internal/retry_traits_generator.h"
#include "generator/internal/round_robin_decorator_generator.h"
#include "generator/internal/sample_generator.h"
#include "generator/internal/stub_factory_generator.h"
#include "generator/internal/stub_generator.h"
#include "generator/internal/stub_rest_generator.h"
#include <google/api/routing.pb.h>
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/cpp/names.h>
#include <regex>
#include <set>
#include <string>

using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::compiler::cpp::FieldName;

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
      return std::string(field->cpp_type_name());
    case FieldDescriptor::CPPTYPE_ENUM:
      return ProtoNameToCppName(field->enum_type()->full_name());

    case FieldDescriptor::CPPTYPE_STRING:
      return std::string("std::") + std::string(field->cpp_type_name());

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoNameToCppName(field->message_type()->full_name());
  }
  GCP_LOG(FATAL) << "FieldDescriptor " << field->cpp_type_name()
                 << " not handled";
  /*NOTREACHED*/
  return field->cpp_type_name();
}

std::string FormatDoxygenLink(
    google::protobuf::Descriptor const& message_type) {
  google::protobuf::SourceLocation loc;
  message_type.GetSourceLocation(&loc);
  std::string output_type_proto_file_name = message_type.file()->name();
  return absl::StrCat(
      "@googleapis_link{", ProtoNameToCppName(message_type.full_name()), ",",
      output_type_proto_file_name, "#L", loc.start_line + 1, "}");
}

std::string DefineReferenceLink(
    google::protobuf::Descriptor const& message_type) {
  google::protobuf::SourceLocation loc;
  message_type.GetSourceLocation(&loc);
  std::string output_type_proto_file_name = message_type.file()->name();
  return absl::StrCat("@googleapis_reference_link{",
                      output_type_proto_file_name, "#L", loc.start_line + 1,
                      "}");
}

absl::variant<std::string, google::protobuf::Descriptor const*>
FullyQualifyMessageType(google::protobuf::MethodDescriptor const& method,
                        std::string message_type) {
  google::protobuf::Descriptor const* output_type =
      method.file()->pool()->FindMessageTypeByName(message_type);
  if (output_type != nullptr) {
    return output_type;
  }
  output_type = method.file()->pool()->FindMessageTypeByName(
      absl::StrCat(method.file()->package(), ".", message_type));
  if (output_type != nullptr) {
    return output_type;
  }
  return message_type;
}

absl::variant<std::string, google::protobuf::Descriptor const*>
DeduceLongrunningOperationResponseType(
    google::protobuf::MethodDescriptor const& method,
    google::longrunning::OperationInfo const& operation_info) {
  std::string deduced_response_type =
      operation_info.response_type() == "google.protobuf.Empty"
          ? operation_info.metadata_type()
          : operation_info.response_type();
  return FullyQualifyMessageType(method, deduced_response_type);
}

struct FullyQualifiedMessageTypeVisitor {
  std::string operator()(std::string const& s) const { return s; }
  std::string operator()(google::protobuf::Descriptor const* d) const {
    return d->full_name();
  }
};

struct FormatDoxygenLinkVisitor {
  explicit FormatDoxygenLinkVisitor() = default;
  std::string operator()(std::string const& s) const {
    return ProtoNameToCppName(s);
  }
  std::string operator()(google::protobuf::Descriptor const* d) const {
    return FormatDoxygenLink(*d);
  }
};

struct DefineReferenceLinkVisitor {
  explicit DefineReferenceLinkVisitor() = default;
  std::string operator()(std::string const& s) const {
    return ProtoNameToCppName(s);
  }
  std::string operator()(google::protobuf::Descriptor const* d) const {
    return DefineReferenceLink(*d);
  }
};

void SetLongrunningOperationMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  if (method.output_type()->full_name() == "google.longrunning.Operation") {
    auto operation_info =
        method.options().GetExtension(google::longrunning::operation_info);
    method_vars["longrunning_metadata_type"] = ProtoNameToCppName(absl::visit(
        FullyQualifiedMessageTypeVisitor(),
        FullyQualifyMessageType(method, operation_info.metadata_type())));
    method_vars["longrunning_response_type"] = ProtoNameToCppName(absl::visit(
        FullyQualifiedMessageTypeVisitor(),
        FullyQualifyMessageType(method, operation_info.response_type())));
    auto deduced_response_type =
        DeduceLongrunningOperationResponseType(method, operation_info);
    method_vars["longrunning_deduced_response_message_type"] =
        absl::visit(FullyQualifiedMessageTypeVisitor(), deduced_response_type);
    method_vars["longrunning_deduced_response_type"] = ProtoNameToCppName(
        method_vars["longrunning_deduced_response_message_type"]);
    method_vars["method_longrunning_deduced_return_doxygen_link"] =
        absl::visit(FormatDoxygenLinkVisitor{}, deduced_response_type);
  }
}

void SetMethodSignatureMethodVars(
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  auto method_signature_extension =
      method.options().GetRepeatedExtension(google::api::method_signature);
  std::set<std::string> method_signature_uids;
  for (int i = 0; i < method_signature_extension.size(); ++i) {
    google::protobuf::Descriptor const* input_type = method.input_type();
    std::vector<std::string> parameters =
        absl::StrSplit(method_signature_extension[i], ',', absl::SkipEmpty());
    std::string method_signature;
    std::string method_request_setters;
    std::string method_signature_uid;
    bool field_deprecated = false;
    for (auto& parameter : parameters) {
      absl::StripAsciiWhitespace(&parameter);
      google::protobuf::FieldDescriptor const* parameter_descriptor =
          input_type->FindFieldByName(parameter);
      auto field_options = parameter_descriptor->options();
      if (field_options.deprecated()) {
        field_deprecated = true;
        break;
      }
      auto const parameter_name = FieldName(parameter_descriptor);
      std::string cpp_type;
      if (parameter_descriptor->is_map()) {
        cpp_type = absl::StrFormat(
            "std::map<%s, %s> const&",
            CppTypeToString(parameter_descriptor->message_type()->map_key()),
            CppTypeToString(parameter_descriptor->message_type()->map_value()));
        method_request_setters += absl::StrFormat(
            "  *request.mutable_%s() = {%s.begin(), %s.end()};\n",
            parameter_name, parameter_name, parameter_name);
      } else if (parameter_descriptor->is_repeated()) {
        cpp_type = absl::StrFormat("std::vector<%s> const&",
                                   CppTypeToString(parameter_descriptor));
        method_request_setters += absl::StrFormat(
            "  *request.mutable_%s() = {%s.begin(), %s.end()};\n",
            parameter_name, parameter_name, parameter_name);
      } else if (parameter_descriptor->type() ==
                 FieldDescriptor::TYPE_MESSAGE) {
        cpp_type =
            absl::StrFormat("%s const&", CppTypeToString(parameter_descriptor));
        method_request_setters += absl::StrFormat(
            "  *request.mutable_%s() = %s;\n", parameter_name, parameter_name);
      } else {
        switch (parameter_descriptor->cpp_type()) {
          case FieldDescriptor::CPPTYPE_STRING:
            cpp_type = absl::StrFormat("%s const&",
                                       CppTypeToString(parameter_descriptor));
            break;
          default:
            cpp_type = CppTypeToString(parameter_descriptor);
        }
        method_request_setters += absl::StrFormat(
            "  request.set_%s(%s);\n", parameter_name, parameter_name);
      }
      method_signature += absl::StrFormat("%s %s, ", cpp_type, parameter_name);
      method_signature_uid += absl::StrFormat("%s,", cpp_type);
    }
    // If method signatures conflict (because the parameters are of identical
    // types), we should generate the first signature in the conflict set and
    // drop the rest.
    //
    // See: https://google.aip.dev/client-libraries/4232#method-signatures_1
    auto p = method_signature_uids.insert(method_signature_uid);
    if (!p.second) continue;
    if (field_deprecated) continue;
    std::string key = "method_signature" + std::to_string(i);
    method_vars[key] = method_signature;
    std::string key2 = "method_request_setters" + std::to_string(i);
    method_vars[key2] = method_request_setters;
  }
}

std::string FormatFieldAccessorCall(
    google::protobuf::MethodDescriptor const& method,
    std::string const& field_name) {
  std::vector<std::string> chunks;
  auto const* input_type = method.input_type();
  for (auto const& sv : absl::StrSplit(field_name, '.')) {
    auto const chunk = std::string(sv);
    auto const* chunk_descriptor = input_type->FindFieldByName(chunk);
    chunks.push_back(FieldName(chunk_descriptor));
    input_type = chunk_descriptor->message_type();
  }
  return absl::StrJoin(chunks, "().");
}

void SetHttpResourceRoutingMethodVars(
    absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
        parsed_http_info,
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  struct HttpInfoVisitor {
    HttpInfoVisitor(google::protobuf::MethodDescriptor const& method,
                    VarsDictionary& method_vars)
        : method(method), method_vars(method_vars) {}

    // This visitor handles the case where the url field contains a token
    // surrounded by curly braces:
    //   patch: "/v1/{parent=projects/*/instances/*}/databases\"
    // In this case 'parent' is expected to be found as a field in the protobuf
    // request message whose value matches the pattern 'projects/*/instances/*'.
    // The request protobuf field can sometimes be nested a la:
    //   post: "/v1/{instance.name=projects/*/locations/*/instances/*}"
    // The emitted code needs to access the value via `request.parent()' and
    // 'request.instance().name()`, respectively.
    void operator()(HttpExtensionInfo const& info) {
      method_vars["method_request_url_path"] = info.url_path;
      method_vars["method_request_url_substitution"] = info.url_substitution;
      std::string param = info.request_field_name;
      method_vars["method_request_param_key"] = param;
      method_vars["method_request_param_value"] =
          FormatFieldAccessorCall(method, param) + "()";
      method_vars["method_request_body"] = info.body;
      method_vars["method_http_verb"] = info.http_verb;
      method_vars["method_rest_path"] =
          absl::StrCat("absl::StrCat(\"", info.path_prefix, "\",request.",
                       method_vars["method_request_param_value"], ",\"",
                       info.path_suffix, "\")");
    }

    // This visitor handles the case where no request field is specified in the
    // url:
    //   get: "/v1/foo/bar"
    void operator()(HttpSimpleInfo const& info) {
      method_vars["method_http_verb"] = info.http_verb;
      method_vars["method_request_param_value"] = method.full_name();
      method_vars["method_rest_path"] = absl::StrCat("\"", info.url_path, "\"");
    }

    // This visitor is an error diagnostic, in case we encounter an url that the
    // generator does not currently parse. Emitting the method name causes code
    // generation that does not compile and points to the proto location to be
    // investigated.
    void operator()(absl::monostate) {
      method_vars["method_http_verb"] = method.full_name();
      method_vars["method_request_param_value"] = method.full_name();
      method_vars["method_rest_path"] = method.full_name();
    }
    google::protobuf::MethodDescriptor const& method;
    VarsDictionary& method_vars;
  };

  absl::visit(HttpInfoVisitor(method, method_vars), parsed_http_info);
}

// RestClient::Get does not use request body, so per
// https://cloud.google.com/apis/design/standard_methods, for HTTP transcoding
// we need to turn the request fields into query parameters.
// TODO(#10176): Consider adding support for repeated simple fields.
void SetHttpGetQueryParameters(
    absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
        parsed_http_info,
    google::protobuf::MethodDescriptor const& method,
    VarsDictionary& method_vars) {
  struct HttpInfoVisitor {
    HttpInfoVisitor(google::protobuf::MethodDescriptor const& method,
                    VarsDictionary& method_vars)
        : method(method), method_vars(method_vars) {}
    void FormatQueryParameterCode(
        std::string const& http_verb,
        absl::optional<std::string> const& param_field_name) {
      if (http_verb == "Get") {
        std::vector<std::pair<std::string, protobuf::FieldDescriptor::CppType>>
            remaining_request_fields;
        auto const* request = method.input_type();
        for (int i = 0; i < request->field_count(); ++i) {
          auto const* field = request->field(i);
          // Only attempt to make non-repeated, simple fields query parameters.
          if (!field->is_repeated() &&
              field->cpp_type() != protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            if (!param_field_name || field->name() != *param_field_name) {
              remaining_request_fields.emplace_back(
                  std::make_pair(field->name(), field->cpp_type()));
            }
          }
        }
        auto format = [](auto* out, auto const& i) {
          if (i.second == protobuf::FieldDescriptor::CPPTYPE_STRING) {
            out->append(absl::StrFormat("std::make_pair(\"%s\", request.%s())",
                                        i.first, i.first));
            return;
          }
          out->append(absl::StrFormat(
              "std::make_pair(\"%s\", std::to_string(request.%s()))", i.first,
              i.first));
        };
        if (remaining_request_fields.empty()) {
          method_vars["method_http_query_parameters"] = ", {}";
        } else {
          method_vars["method_http_query_parameters"] = absl::StrCat(
              ",\n      {",
              absl::StrJoin(remaining_request_fields, ",\n       ", format),
              "}");
        }
      }
    }

    // This visitor handles the case where the url field contains a token
    // surrounded by curly braces:
    //   patch: "/v1/{parent=projects/*/instances/*}/databases\"
    // In this case 'parent' is expected to be found as a field in the protobuf
    // request message and is already included in the url. No need to duplicate
    // it as a query parameter.
    void operator()(HttpExtensionInfo const& info) {
      FormatQueryParameterCode(
          info.http_verb,
          FormatFieldAccessorCall(method, info.request_field_name));
    }

    // This visitor handles the case where no request field is specified in the
    // url:
    //   get: "/v1/foo/bar"
    // In this case, all non-repeated, simple fields should be query parameters.
    void operator()(HttpSimpleInfo const& info) {
      FormatQueryParameterCode(info.http_verb, {});
    }

    // This visitor is an error diagnostic, in case we encounter an url that the
    // generator does not currently parse. Emitting the method name causes code
    // generation that does not compile and points to the proto location to be
    // investigated.
    void operator()(absl::monostate) {
      method_vars["method_http_query_parameters"] = method.full_name();
    }

    google::protobuf::MethodDescriptor const& method;
    VarsDictionary& method_vars;
  };

  method_vars["method_http_query_parameters"] = "";
  absl::visit(HttpInfoVisitor(method, method_vars), parsed_http_info);
}

std::string DefaultIdempotencyFromHttpOperation(
    google::protobuf::MethodDescriptor const& method) {
  if (method.options().HasExtension(google::api::http)) {
    google::api::HttpRule http_rule =
        method.options().GetExtension(google::api::http);
    switch (http_rule.pattern_case()) {
      case google::api::HttpRule::kGet:
      case google::api::HttpRule::kPut:
        return "kIdempotent";
      case google::api::HttpRule::kPost:
      case google::api::HttpRule::kDelete:
      case google::api::HttpRule::kPatch:
        break;
      default:
        GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                       << ": google::api::HttpRule not handled";
    }
  }
  return "kNonIdempotent";
}

std::string ChompByValue(std::string const& s) {
  return (!s.empty() && s.back() == '\n') ? s.substr(0, s.size() - 1) : s;
}

std::string EscapePrinterDelimiter(std::string const& text) {
  return absl::StrReplaceAll(text, {{"$", "$$"}});
}

std::string FormatClassCommentsFromServiceComments(
    google::protobuf::ServiceDescriptor const& service) {
  google::protobuf::SourceLocation service_source_location;
  std::string formatted_comments;
  if (!service.GetSourceLocation(&service_source_location) ||
      service_source_location.leading_comments.empty()) {
    GCP_LOG(INFO) << __FILE__ << ":" << __LINE__ << ": " << service.full_name()
                  << " no leading_comments to format";
    formatted_comments = absl::StrCat(" ", service.name(), "Client");
  } else {
    formatted_comments = absl::StrReplaceAll(
        ChompByValue(service_source_location.leading_comments),
        {{"\n\n", "\n///\n/// "}, {"\n", "\n/// "}});
  }
  std::string doxygen_formatted_comments =
      absl::StrCat("///\n///", formatted_comments,
                   R"""(
///
/// @par Equality
///
/// Instances of this class created via copy-construction or copy-assignment
/// always compare equal. Instances created with equal
/// `std::shared_ptr<*Connection>` objects compare equal. Objects that compare
/// equal share the same underlying resources.
///
/// @par Performance
///
/// Creating a new instance of this class is a relatively expensive operation,
/// new objects establish new connections to the service. In contrast,
/// copy-construction, move-construction, and the corresponding assignment
/// operations are relatively efficient as the copies share all underlying
/// resources.
///
/// @par Thread Safety
///
/// Concurrent access to different instances of this class, even if they compare
/// equal, is guaranteed to work. Two or more threads operating on the same
/// instance of this class is not guaranteed to work. Since copy-construction
/// and move-construction is a relatively efficient operation, consider using
/// such a copy when using this class from multiple threads.
///)""");
  return absl::StrReplaceAll(doxygen_formatted_comments, {{"///  ", "/// "}});
}

auto constexpr kDialogflowCXSessionIdProto = R"""(
 Format: `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/sessions/<Session ID>` or `projects/<Project ID>/locations/<Location
 ID>/agents/<Agent ID>/environments/<Environment ID>/sessions/<Session ID>`.
)""";

auto constexpr kDialogflowCXSessionIdCpp = R"""(
 Format:

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/sessions/<Session ID>
 @endcode

 or

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/environments/<Environment ID>/sessions/<Session ID>
 @endcode
)""";

auto constexpr kDialogflowCXEntityTypeIdProto = R"""(
 Format: `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/sessions/<Session ID>/entityTypes/<Entity Type ID>` or
 `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/environments/<Environment ID>/sessions/<Session ID>/entityTypes/<Entity
 Type ID>`. If `Environment ID` is not specified, we assume default 'draft'
 environment.
)""";

auto constexpr kDialogflowCXEntityTypeIdCpp = R"""(
 Format:

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/sessions/<Session ID>/entityTypes/<Entity Type ID>
 @endcode

 or

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/environments/<Environment ID>/sessions/<Session ID>/entityTypes/<Entity Type ID>
 @endcode

 If `Environment ID` is not specified, we assume the default 'draft'
 environment.
)""";

auto constexpr kDialogflowESSessionIdProto =
    R"""( `projects/<Project ID>/agent/sessions/<Session ID>` or `projects/<Project
 ID>/agent/environments/<Environment ID>/users/<User ID>/sessions/<Session
 ID>`.)""";

auto constexpr kDialogflowESSessionIdCpp = R"""(
 @code
 projects/<Project ID>/agent/sessions/<Session ID>
 @endcode

 or

 @code
 projects/<Project ID>/agent/environments/<Environment ID>/users/<User ID>/sessions/<Session ID>
 @endcode
)""";

auto constexpr kDialogflowESContextIdProto =
    R"""( `projects/<Project ID>/agent/sessions/<Session ID>/contexts/<Context ID>`
 or `projects/<Project ID>/agent/environments/<Environment ID>/users/<User
 ID>/sessions/<Session ID>/contexts/<Context ID>`.)""";

auto constexpr kDialogflowESContextIdCpp = R"""(
 @code
 projects/<Project ID>/agent/sessions/<Session ID>/contexts/<Context ID>
 @endcode

 or

 @code
 projects/<Project ID>/agent/environments/<Environment ID>/users/<User ID>/sessions/<Session ID>/contexts/<Context ID>`
 @endcode
)""";

auto constexpr kDialogflowESSessionEntityTypeDisplayNameProto =
    R"""( `projects/<Project ID>/agent/sessions/<Session ID>/entityTypes/<Entity Type
 Display Name>` or `projects/<Project ID>/agent/environments/<Environment
 ID>/users/<User ID>/sessions/<Session ID>/entityTypes/<Entity Type Display
 Name>`.)""";

auto constexpr kDialogflowESSessionEntityTypeDisplayNameCpp = R"""(
 @code
 projects/<Project ID>/agent/sessions/<Session ID>/entityTypes/<Entity Type Display Name>
 @endcode

 or

 @code
 projects/<Project ID>/agent/environments/<Environment ID>/users/<User ID>/sessions/<Session ID>/entityTypes/<Entity Type Display Name>
 @endcode
)""";

std::string FormatApiMethodSignatureParameters(
    google::protobuf::MethodDescriptor const& method,
    std::string const& signature) {
  std::string parameter_comments;
  google::protobuf::Descriptor const* input_type = method.input_type();
  std::vector<std::string> parameters =
      absl::StrSplit(signature, ',', absl::SkipEmpty());
  for (auto& parameter : parameters) {
    absl::StripAsciiWhitespace(&parameter);
    google::protobuf::FieldDescriptor const* parameter_descriptor =
        input_type->FindFieldByName(parameter);
    google::protobuf::SourceLocation loc;
    parameter_descriptor->GetSourceLocation(&loc);
    auto comment = absl::StrReplaceAll(
        loc.leading_comments,
        {{kDialogflowCXSessionIdProto, kDialogflowCXSessionIdCpp},
         {kDialogflowCXEntityTypeIdProto, kDialogflowCXEntityTypeIdCpp},
         {kDialogflowESSessionIdProto, kDialogflowESSessionIdCpp},
         {kDialogflowESContextIdProto, kDialogflowESContextIdCpp},
         {kDialogflowESSessionEntityTypeDisplayNameProto,
          kDialogflowESSessionEntityTypeDisplayNameCpp}});
    comment = absl::StrReplaceAll(
        EscapePrinterDelimiter(ChompByValue(comment)),
        {
            {"\n\n", "\n  /// "},
            {"\n", "\n  /// "},
            // Doxygen cannot process this tag. One proto uses it in a comment.
            {"<tbody>", "<!--<tbody>-->"},
            {"</tbody>", "<!--</tbody>-->"},
            // Unescaped elements in spanner_instance_admin.proto.
            {" <parent>/instanceConfigs/us-east1,",
             " `<parent>/instanceConfigs/us-east1`,"},
            {" <parent>/instanceConfigs/nam3.",
             " `<parent>/instanceConfigs/nam3`."},
        });
    absl::StrAppendFormat(&parameter_comments, "  /// @param %s %s\n",
                          FieldName(parameter_descriptor), std::move(comment));
  }
  return parameter_comments;
}

void SetRetryStatusCodeExpression(VarsDictionary& vars) {
  auto iter = vars.find("retryable_status_codes");
  if (iter == vars.end()) return;
  std::string retry_status_code_expression = "status.code() != StatusCode::kOk";
  std::set<std::string> codes = absl::StrSplit(iter->second, ',');

  auto append_status_code = [&](std::string const& status_code) {
    absl::StrAppend(&retry_status_code_expression,
                    " && status.code() != StatusCode::", status_code);
  };

  for (auto const& code : codes) {
    std::pair<std::string, std::string> service_code =
        absl::StrSplit(code, '.');
    if (service_code.second.empty()) {
      append_status_code(service_code.first);
    }
    if (service_code.first == vars["service_name"]) {
      append_status_code(service_code.second);
    }
  }
  vars["retry_status_code_expression"] = retry_status_code_expression;
}

std::string FormatAdditionalPbHeaderPaths(VarsDictionary& vars) {
  std::vector<std::string> additional_pb_header_paths;
  auto iter = vars.find("additional_proto_files");
  if (iter != vars.end()) {
    std::vector<std::string> additional_proto_files =
        absl::StrSplit(iter->second, absl::ByChar(','));
    for (auto& file : additional_proto_files) {
      absl::StripAsciiWhitespace(&file);
      additional_pb_header_paths.push_back(
          absl::StrCat(absl::StripSuffix(file, ".proto"), ".pb.h"));
    }
  }
  return absl::StrJoin(additional_pb_header_paths, ",");
}

}  // namespace

absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo>
ParseHttpExtension(google::protobuf::MethodDescriptor const& method) {
  if (!method.options().HasExtension(google::api::http)) return {};
  HttpExtensionInfo info;
  google::api::HttpRule http_rule =
      method.options().GetExtension(google::api::http);

  std::string url_pattern;
  switch (http_rule.pattern_case()) {
    case google::api::HttpRule::kGet:
      info.http_verb = "Get";
      url_pattern = http_rule.get();
      break;
    case google::api::HttpRule::kPut:
      info.http_verb = "Put";
      url_pattern = http_rule.put();
      break;
    case google::api::HttpRule::kPost:
      info.http_verb = "Post";
      url_pattern = http_rule.post();
      break;
    case google::api::HttpRule::kDelete:
      info.http_verb = "Delete";
      url_pattern = http_rule.delete_();
      break;
    case google::api::HttpRule::kPatch:
      info.http_verb = "Patch";
      url_pattern = http_rule.patch();
      break;
    default:
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                     << ": google::api::HttpRule not handled";
  }

  static std::regex const kUrlPatternRegex(R"((.*)\{(.*)=(.*)\}(.*))");
  std::smatch match;
  if (!std::regex_match(url_pattern, match, kUrlPatternRegex)) {
    return HttpSimpleInfo{info.http_verb, url_pattern, http_rule.body()};
  }
  info.url_path = match[0];
  info.request_field_name = match[2];
  info.url_substitution = match[3];
  info.body = http_rule.body();
  info.path_prefix = match[1];
  info.path_suffix = match[4];
  return info;
}

ExplicitRoutingInfo ParseExplicitRoutingHeader(
    google::protobuf::MethodDescriptor const& method) {
  ExplicitRoutingInfo info;
  if (!method.options().HasExtension(google::api::routing)) return info;
  google::api::RoutingRule rule =
      method.options().GetExtension(google::api::routing);

  auto const& rps = rule.routing_parameters();
  // We use reverse iterators so that "last wins" becomes "first wins".
  for (auto it = rps.rbegin(); it != rps.rend(); ++it) {
    std::vector<std::string> chunks;
    auto const* input_type = method.input_type();
    for (auto const& sv : absl::StrSplit(it->field(), '.')) {
      auto const chunk = std::string(sv);
      auto const* chunk_descriptor = input_type->FindFieldByName(chunk);
      chunks.push_back(FieldName(chunk_descriptor));
      input_type = chunk_descriptor->message_type();
    }
    auto field_name = absl::StrJoin(chunks, "().");
    auto const& path_template = it->path_template();
    // When a path_template is not supplied, we use the field name as the
    // routing parameter key. The pattern matches the whole value of the field.
    if (path_template.empty()) {
      info[it->field()].push_back({std::move(field_name), "(.*)"});
      continue;
    }
    // When a path_template is supplied, we convert the pattern from the proto
    // into a std::regex. We extract the routing parameter key and set up a
    // single capture group. For example:
    //
    // Input :
    //   - path_template = "projects/*/{foo=instances/*/}/**"
    // Output:
    //   - param         = "foo"
    //   - pattern       = "projects/[^/]+/(instances/[^/]+)/.*"
    static std::regex const kPatternRegex(R"((.*)\{(.*)=(.*)\}(.*))");
    std::smatch match;
    if (!std::regex_match(path_template, match, kPatternRegex)) {
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": "
                     << "RoutingParameters path template is malformed: "
                     << path_template;
    }
    auto pattern =
        absl::StrCat(match[1].str(), "(", match[3].str(), ")", match[4].str());
    pattern = absl::StrReplaceAll(pattern, {{"**", ".*"}, {"*", "[^/]+"}});
    info[match[2].str()].push_back({std::move(field_name), std::move(pattern)});
  }
  return info;
}

std::string FormatMethodComments(
    google::protobuf::MethodDescriptor const& method,
    std::string const& variable_parameter_comments) {
  google::protobuf::SourceLocation method_source_location;
  if (!method.GetSourceLocation(&method_source_location) ||
      method_source_location.leading_comments.empty()) {
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": " << method.full_name()
                   << " no leading_comments to format";
  }
  std::string doxygen_formatted_function_comments = absl::StrReplaceAll(
      EscapePrinterDelimiter(method_source_location.leading_comments),
      {{"\n", "\n  ///"}});

  auto const options_comment = std::string{
      R"""(  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
)"""};

  std::string return_comment_string;
  if (IsLongrunningOperation(method)) {
    return_comment_string =
        "  /// @return $method_longrunning_deduced_return_doxygen_link$\n";
  } else if (IsBidirStreaming(method)) {
    return_comment_string =
        "  /// @return A bidirectional streaming interface with request "
        "(write) type: " +
        FormatDoxygenLink(*method.input_type()) +
        " and response (read) type: $method_return_doxygen_link$\n";
  } else if (!IsResponseTypeEmpty(method) && !IsPaginated(method)) {
    return_comment_string = "  /// @return $method_return_doxygen_link$\n";
  } else if (IsPaginated(method)) {
    return_comment_string =
        "  /// @return $method_paginated_return_doxygen_link$\n";
  }

  std::string trailer = "  ///\n";
  trailer += "  /// [" + method.input_type()->full_name() + "]: ";
  trailer += DefineReferenceLink(*method.input_type()) + "\n";
  if (IsLongrunningOperation(method)) {
    auto info =
        method.options().GetExtension(google::longrunning::operation_info);
    auto type = DeduceLongrunningOperationResponseType(method, info);
    trailer += "  /// [" +
               absl::visit(FullyQualifiedMessageTypeVisitor(), type) + "]: ";
    trailer += absl::visit(DefineReferenceLinkVisitor{}, type) + "\n";
  } else if (IsPaginated(method)) {
    auto info = DeterminePagination(method);
    if (info->second != nullptr) {
      trailer += "  /// [" + info->second->full_name() + "]: ";
      trailer += DefineReferenceLink(*info->second) + "\n";
    }
  } else if (!IsResponseTypeEmpty(method)) {
    trailer += "  /// [" + method.output_type()->full_name() + "]: ";
    trailer += DefineReferenceLink(*method.output_type()) + "\n";
  }

  trailer += "  ///\n";
  return absl::StrCat("  ///\n  ///", doxygen_formatted_function_comments, "\n",
                      variable_parameter_comments, options_comment,
                      return_comment_string, trailer);
}

std::string FormatMethodCommentsMethodSignature(
    google::protobuf::MethodDescriptor const& method,
    std::string const& signature) {
  auto parameter_comments =
      FormatApiMethodSignatureParameters(method, signature);
  return FormatMethodComments(method, std::move(parameter_comments));
}

std::string FormatMethodCommentsProtobufRequest(
    google::protobuf::MethodDescriptor const& method) {
  google::protobuf::Descriptor const* input_type = method.input_type();
  auto parameter_comment = absl::StrFormat("  /// @param %s %s\n", "request",
                                           FormatDoxygenLink(*input_type));
  return FormatMethodComments(method, std::move(parameter_comment));
}

VarsDictionary CreateServiceVars(
    google::protobuf::ServiceDescriptor const& descriptor,
    std::vector<std::pair<std::string, std::string>> const& initial_values) {
  VarsDictionary vars(initial_values.begin(), initial_values.end());
  vars["product_options_page"] = absl::StrCat(
      absl::StrReplaceAll(vars["product_path"], {{"/", "-"}}), "options");
  vars["additional_pb_header_paths"] = FormatAdditionalPbHeaderPaths(vars);
  vars["class_comment_block"] =
      FormatClassCommentsFromServiceComments(descriptor);
  vars["client_class_name"] = absl::StrCat(descriptor.name(), "Client");
  vars["client_cc_path"] =
      absl::StrCat(vars["product_path"],
                   ServiceNameToFilePath(descriptor.name()), "_client.cc");
  vars["client_header_path"] =
      absl::StrCat(vars["product_path"],
                   ServiceNameToFilePath(descriptor.name()), "_client.h");

  vars["client_samples_cc_path"] = absl::StrCat(
      vars["product_path"], "samples/",
      ServiceNameToFilePath(descriptor.name()), "_client_samples.cc");

  vars["connection_class_name"] = absl::StrCat(descriptor.name(), "Connection");
  vars["connection_cc_path"] =
      absl::StrCat(vars["product_path"],
                   ServiceNameToFilePath(descriptor.name()), "_connection.cc");
  vars["connection_header_path"] =
      absl::StrCat(vars["product_path"],
                   ServiceNameToFilePath(descriptor.name()), "_connection.h");
  vars["connection_impl_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_connection_impl.cc");
  vars["connection_impl_header_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_connection_impl.h");
  vars["connection_options_name"] =
      absl::StrCat(descriptor.name(), "ConnectionOptions");
  vars["connection_options_traits_name"] =
      absl::StrCat(descriptor.name(), "ConnectionOptionsTraits");
  vars["grpc_stub_fqn"] = ProtoNameToCppName(descriptor.full_name());
  vars["idempotency_class_name"] =
      absl::StrCat(descriptor.name(), "ConnectionIdempotencyPolicy");
  vars["idempotency_policy_cc_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()),
      "_connection_idempotency_policy.cc");
  vars["idempotency_policy_header_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(descriptor.name()),
      "_connection_idempotency_policy.h");
  vars["limited_error_count_retry_policy_name"] =
      absl::StrCat(descriptor.name(), "LimitedErrorCountRetryPolicy");
  vars["limited_time_retry_policy_name"] =
      absl::StrCat(descriptor.name(), "LimitedTimeRetryPolicy");
  vars["auth_class_name"] = absl::StrCat(descriptor.name(), "Auth");
  vars["auth_cc_path"] = absl::StrCat(vars["product_path"], "internal/",
                                      ServiceNameToFilePath(descriptor.name()),
                                      "_auth_decorator.cc");
  vars["auth_header_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_auth_decorator.h");
  vars["logging_class_name"] = absl::StrCat(descriptor.name(), "Logging");
  vars["logging_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_logging_decorator.cc");
  vars["logging_header_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_logging_decorator.h");
  vars["metadata_class_name"] = absl::StrCat(descriptor.name(), "Metadata");
  vars["metadata_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_metadata_decorator.cc");
  vars["metadata_header_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_metadata_decorator.h");

  vars["round_robin_class_name"] =
      absl::StrCat(descriptor.name(), "RoundRobin");
  vars["round_robin_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_round_robin_decorator.cc");
  vars["round_robin_header_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_round_robin_decorator.h");

  vars["mock_connection_class_name"] =
      absl::StrCat("Mock", descriptor.name(), "Connection");
  vars["mock_connection_header_path"] =
      absl::StrCat(vars["product_path"], "mocks/mock_",
                   ServiceNameToFilePath(descriptor.name()), "_connection.h");
  vars["option_defaults_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_option_defaults.cc");
  vars["option_defaults_header_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_option_defaults.h");
  vars["options_header_path"] =
      absl::StrCat(vars["product_path"],
                   ServiceNameToFilePath(descriptor.name()), "_options.h");
  vars["product_namespace"] = BuildNamespaces(vars["product_path"])[2];
  vars["product_internal_namespace"] =
      BuildNamespaces(vars["product_path"], NamespaceType::kInternal)[2];
  vars["proto_file_name"] = descriptor.file()->name();
  vars["proto_grpc_header_path"] = absl::StrCat(
      absl::StripSuffix(descriptor.file()->name(), ".proto"), ".grpc.pb.h");
  vars["proto_header_path"] = absl::StrCat(
      absl::StripSuffix(descriptor.file()->name(), ".proto"), ".pb.h");
  vars["retry_policy_name"] = absl::StrCat(descriptor.name(), "RetryPolicy");
  vars["retry_traits_name"] = absl::StrCat(descriptor.name(), "RetryTraits");
  vars["retry_traits_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_retry_traits.h");
  vars["service_endpoint"] =
      descriptor.options().GetExtension(google::api::default_host);
  auto& service_endpoint_env_var = vars["service_endpoint_env_var"];
  if (service_endpoint_env_var.empty()) {
    service_endpoint_env_var = absl::StrCat(
        "GOOGLE_CLOUD_CPP_",
        absl::AsciiStrToUpper(CamelCaseToSnakeCase(descriptor.name())),
        "_ENDPOINT");
  }
  absl::string_view service_endpoint_env_var_prefix = service_endpoint_env_var;
  if (!absl::ConsumeSuffix(&service_endpoint_env_var_prefix, "_ENDPOINT")) {
    // Until we have a need for a service_endpoint_env_var that does not end
    // with "_ENDPOINT", this allows us to generate service_authority_env_var,
    // and so avoid needing to add anything to message ServiceConfiguration.
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                   << R"(: For now we require that service_endpoint_env_var ")"
                   << service_endpoint_env_var << R"(" ends with "_ENDPOINT")";
  }
  vars["service_authority_env_var"] =
      absl::StrCat(service_endpoint_env_var_prefix, "_AUTHORITY");
  vars["service_name"] = descriptor.name();
  vars["stub_class_name"] = absl::StrCat(descriptor.name(), "Stub");
  vars["stub_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub.cc");
  vars["stub_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub.h");
  vars["stub_rest_class_name"] = absl::StrCat(descriptor.name(), "RestStub");
  vars["stub_rest_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_rest_stub.cc");
  vars["stub_rest_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_rest_stub.h");
  vars["stub_factory_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/",
      ServiceNameToFilePath(descriptor.name()), "_stub_factory.cc");
  vars["stub_factory_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(descriptor.name()), "_stub_factory.h");
  SetRetryStatusCodeExpression(vars);
  return vars;
}

std::map<std::string, VarsDictionary> CreateMethodVars(
    google::protobuf::ServiceDescriptor const& service, VarsDictionary const&) {
  std::map<std::string, VarsDictionary> service_methods_vars;
  for (int i = 0; i < service.method_count(); i++) {
    auto const& method = *service.method(i);
    VarsDictionary method_vars;
    method_vars["method_return_doxygen_link"] =
        FormatDoxygenLink(*method.output_type());
    method_vars["default_idempotency"] =
        DefaultIdempotencyFromHttpOperation(method);
    method_vars["method_name"] = method.name();
    method_vars["method_name_snake"] = CamelCaseToSnakeCase(method.name());
    method_vars["request_type"] =
        ProtoNameToCppName(method.input_type()->full_name());
    method_vars["response_message_type"] = method.output_type()->full_name();
    method_vars["response_type"] =
        ProtoNameToCppName(method.output_type()->full_name());
    SetLongrunningOperationMethodVars(method, method_vars);
    if (IsPaginated(method)) {
      auto pagination_info = DeterminePagination(method);
      method_vars["range_output_field_name"] = pagination_info->first;
      // Add exception to AIP-4233 for response types that have exactly one
      // repeated field that is of primitive type string.
      method_vars["range_output_type"] =
          pagination_info->second == nullptr
              ? "std::string"
              : ProtoNameToCppName(pagination_info->second->full_name());
      if (pagination_info->second) {
        method_vars["method_paginated_return_doxygen_link"] =
            FormatDoxygenLink(*pagination_info->second);
      } else {
        method_vars["method_paginated_return_doxygen_link"] = "std::string";
      }
    }
    SetMethodSignatureMethodVars(method, method_vars);
    auto parsed_http_info = ParseHttpExtension(method);
    SetHttpResourceRoutingMethodVars(parsed_http_info, method, method_vars);
    SetHttpGetQueryParameters(parsed_http_info, method, method_vars);
    service_methods_vars[method.full_name()] = method_vars;
  }
  return service_methods_vars;
}

std::vector<std::unique_ptr<GeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    std::vector<std::pair<std::string, std::string>> const& vars) {
  std::vector<std::unique_ptr<GeneratorInterface>> code_generators;
  VarsDictionary service_vars = CreateServiceVars(*service, vars);
  auto method_vars = CreateMethodVars(*service, service_vars);
  auto const omit_client = service_vars.find("omit_client");
  if (omit_client == service_vars.end() || omit_client->second != "true") {
    code_generators.push_back(absl::make_unique<ClientGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(absl::make_unique<SampleGenerator>(
        service, service_vars, method_vars, context));
  }
  auto const omit_connection = service_vars.find("omit_connection");
  if (omit_connection == service_vars.end() ||
      omit_connection->second != "true") {
    code_generators.push_back(absl::make_unique<ConnectionGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(absl::make_unique<IdempotencyPolicyGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(absl::make_unique<MockConnectionGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(absl::make_unique<OptionDefaultsGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(absl::make_unique<OptionsGenerator>(
        service, service_vars, method_vars, context));
    code_generators.push_back(absl::make_unique<ConnectionImplGenerator>(
        service, service_vars, method_vars, context));
    if (service_vars.find("retry_status_code_expression") !=
        service_vars.end()) {
      code_generators.push_back(absl::make_unique<RetryTraitsGenerator>(
          service, service_vars, method_vars, context));
    }
  }
  auto const omit_stub_factory = service_vars.find("omit_stub_factory");
  if (omit_stub_factory == service_vars.end() ||
      omit_stub_factory->second != "true") {
    code_generators.push_back(absl::make_unique<StubFactoryGenerator>(
        service, service_vars, method_vars, context));
  }
  code_generators.push_back(absl::make_unique<AuthDecoratorGenerator>(
      service, service_vars, method_vars, context));
  code_generators.push_back(absl::make_unique<LoggingDecoratorGenerator>(
      service, service_vars, method_vars, context));
  code_generators.push_back(absl::make_unique<MetadataDecoratorGenerator>(
      service, service_vars, method_vars, context));
  code_generators.push_back(absl::make_unique<StubGenerator>(
      service, service_vars, method_vars, context));

  auto const generate_round_robin_generator =
      service_vars.find("generate_round_robin_decorator");
  if (generate_round_robin_generator != service_vars.end() &&
      generate_round_robin_generator->second == "true") {
    code_generators.push_back(absl::make_unique<RoundRobinDecoratorGenerator>(
        service, service_vars, method_vars, context));
  }

  auto const generate_rest_transport =
      service_vars.find("generate_rest_transport");
  if (generate_rest_transport != service_vars.end() &&
      generate_rest_transport->second == "true") {
    code_generators.push_back(absl::make_unique<StubRestGenerator>(
        service, service_vars, method_vars, context));
  }

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
