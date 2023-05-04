// Copyright 2023 Google LLC
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

#include "generator/internal/http_option_utils.h"
#include "generator/internal/printer.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include <google/api/annotations.pb.h>
#include <google/protobuf/compiler/cpp/names.h>
#include <google/protobuf/descriptor.h>
#include <regex>
#include <vector>

using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::compiler::cpp::FieldName;

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

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

}  // namespace

void SetHttpDerivedMethodVars(
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
          absl::StrCat("absl::StrCat(\"", info.path_prefix, "\", request.",
                       method_vars["method_request_param_value"], ", \"",
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
              remaining_request_fields.emplace_back(field->name(),
                                                    field->cpp_type());
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

bool HasHttpRoutingHeader(MethodDescriptor const& method) {
  auto result = ParseHttpExtension(method);
  return absl::holds_alternative<HttpExtensionInfo>(result);
}

bool HasHttpAnnotation(MethodDescriptor const& method) {
  auto result = ParseHttpExtension(method);
  return absl::holds_alternative<HttpExtensionInfo>(result) ||
         absl::holds_alternative<HttpSimpleInfo>(result);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
