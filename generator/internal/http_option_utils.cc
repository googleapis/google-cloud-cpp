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
#include "generator/internal/http_annotation_parser.h"
#include "generator/internal/longrunning.h"
#include "generator/internal/printer.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/url_encode.h"
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

void RestPathVisitorHelper(std::string const& api_version,
                           PathTemplate::Segment const& s,
                           std::vector<HttpExtensionInfo::RestPathPiece>& path);

struct RestPathVisitor {
  explicit RestPathVisitor(std::string api_version,
                           std::vector<HttpExtensionInfo::RestPathPiece>& path)
      : api_version(std::move(api_version)), path(path) {}
  void operator()(PathTemplate::Match const&) {}
  void operator()(PathTemplate::MatchRecursive const&) {}
  void operator()(std::string const& s) {
    path.emplace_back(
        [piece = s, api = api_version](
            google::protobuf::MethodDescriptor const&, bool is_async) {
          if (piece != api) return absl::StrFormat("\"%s\"", piece);
          if (is_async) {
            return absl::StrFormat(
                "rest_internal::DetermineApiVersion(\"%s\", *options)", api);
          }
          return absl::StrFormat(
              "rest_internal::DetermineApiVersion(\"%s\", options)", api);
        });
  }
  void operator()(PathTemplate::Variable const& v) {
    path.emplace_back(
        [piece = v.field_path](google::protobuf::MethodDescriptor const& method,
                               bool) {
          return absl::StrFormat("request.%s()",
                                 FormatFieldAccessorCall(method, piece));
        });
  }
  void operator()(PathTemplate::Segment const& s) {
    RestPathVisitorHelper(api_version, s, path);
  }

  std::string api_version;
  std::vector<HttpExtensionInfo::RestPathPiece>& path;
};

void RestPathVisitorHelper(
    std::string const& api_version, PathTemplate::Segment const& s,
    std::vector<HttpExtensionInfo::RestPathPiece>& path) {
  absl::visit(RestPathVisitor{api_version, path}, s.value);
}

std::string FormatQueryParameterCode(
    google::protobuf::MethodDescriptor const& method,
    std::vector<std::string> param_field_names) {
  std::vector<std::pair<std::string, QueryParameterInfo>>
      remaining_request_fields;
  auto const* request = method.input_type();
  for (int i = 0; i < request->field_count(); ++i) {
    auto const* field = request->field(i);
    auto param_info = DetermineQueryParameterInfo(*field);
    if (param_info && !internal::Contains(param_field_names, field->name())) {
      remaining_request_fields.emplace_back(field->name(), *param_info);
    }
  }

  auto format = [](auto* out, auto const& i) {
    std::string field_access;
    if (i.second.cpp_type == protobuf::FieldDescriptor::CPPTYPE_STRING) {
      field_access = i.second.request_field_accessor;
    } else if (i.second.cpp_type == protobuf::FieldDescriptor::CPPTYPE_BOOL) {
      field_access = absl::StrCat("(", i.second.request_field_accessor,
                                  R"""( ? "1" : "0"))""");
    } else {
      field_access =
          absl::StrCat("std::to_string(", i.second.request_field_accessor, ")");
    }

    if (i.second.check_presence) {
      out->append(absl::StrFormat(
          R"""(std::make_pair("%s", (request.has_%s() ? %s : "")))""", i.first,
          i.first, field_access));
    } else {
      out->append(absl::StrFormat(R"""(std::make_pair("%s", %s))""", i.first,
                                  field_access));
    }
  };

  if (remaining_request_fields.empty()) {
    return "";
  }
  return absl::StrCat(
      ",\n      rest_internal::TrimEmptyQueryParameters({",
      absl::StrJoin(remaining_request_fields, ",\n        ", format), "})");
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
      method_vars["method_request_params"] = absl::StrJoin(
          info.field_substitutions, ", \"&\",",
          [&](std::string* out, std::pair<std::string, std::string> const& p) {
            out->append(
                absl::StrFormat("\"%s=\", internal::UrlEncode(request.%s())",
                                internal::UrlEncode(p.first),
                                FormatFieldAccessorCall(method, p.first)));
          });
      method_vars["method_request_body"] = info.body;
      method_vars["method_http_verb"] = info.http_verb;
      // method_rest_path is only used for REST transport.
      auto trailer = [&info] {
        if (info.rest_path_verb.empty()) return std::string(")");
        return absl::StrFormat(R"""(, ":%s"))""", info.rest_path_verb);
      };
      auto path_expression = [&info, method = &method](bool is_async) {
        return absl::StrCat(
            ", ", absl::StrJoin(info.rest_path, R"""(, "/", )""",
                                [method, is_async](
                                    std::string* out,
                                    HttpExtensionInfo::RestPathPiece const& p) {
                                  out->append(p(*method, is_async));
                                }));
      };
      method_vars["method_rest_path"] = absl::StrCat(
          R"""(absl::StrCat("/")""", path_expression(false), trailer());
      method_vars["method_rest_path_async"] = absl::StrCat(
          R"""(absl::StrCat("/")""", path_expression(true), trailer());
    }

    // This visitor handles the case where no request field is specified in the
    // url:
    //   get: "/v1/foo/bar"
    void operator()(HttpSimpleInfo const& info) {
      method_vars["method_http_verb"] = info.http_verb;
      if (absl::StrContains(info.url_path, info.api_version)) {
        std::string needle = absl::StrFormat("/%s/", info.api_version);
        auto url_path = absl::string_view(info.url_path);
        auto start = url_path.find(needle);
        auto pre_needle = url_path.substr(0, start);
        auto pos_needle = url_path.substr(start + needle.length());

        method_vars["method_rest_path"] = absl::StrFormat(
            R"""(absl::StrCat("%s/", rest_internal::DetermineApiVersion("%s", options), "/%s"))""",
            pre_needle, info.api_version, pos_needle);
        method_vars["method_rest_path_async"] = absl::StrFormat(
            R"""(absl::StrCat("%s/", rest_internal::DetermineApiVersion("%s", *options), "/%s"))""",
            pre_needle, info.api_version, pos_needle);
      } else {
        method_vars["method_rest_path"] =
            absl::StrCat("\"", info.url_path, "\"");
        method_vars["method_rest_path_async"] =
            absl::StrCat("\"", info.url_path, "\"");
      }
    }

    // This visitor is an error diagnostic, in case we encounter an url that the
    // generator does not currently parse. Emitting the method name causes code
    // generation that does not compile and points to the proto location to be
    // investigated.
    void operator()(absl::monostate) {
      method_vars["method_http_verb"] = method.full_name();
      method_vars["method_request_param_value"] = method.full_name();
      method_vars["method_rest_path"] = method.full_name();
      method_vars["method_rest_path_async"] = method.full_name();
    }
    google::protobuf::MethodDescriptor const& method;
    VarsDictionary& method_vars;
  };

  absl::visit(HttpInfoVisitor(method, method_vars), parsed_http_info);
}

absl::optional<QueryParameterInfo> DetermineQueryParameterInfo(
    google::protobuf::FieldDescriptor const& field) {
  static auto* const kSupportedWellKnownValueTypes = [] {
    auto foo = std::make_unique<
        std::unordered_map<std::string, protobuf::FieldDescriptor::CppType>>();
    foo->emplace("google.protobuf.BoolValue",
                 protobuf::FieldDescriptor::CPPTYPE_BOOL);
    foo->emplace("google.protobuf.DoubleValue",
                 protobuf::FieldDescriptor::CPPTYPE_DOUBLE);
    foo->emplace("google.protobuf.FloatValue",
                 protobuf::FieldDescriptor::CPPTYPE_FLOAT);
    foo->emplace("google.protobuf.Int32Value",
                 protobuf::FieldDescriptor::CPPTYPE_INT32);
    foo->emplace("google.protobuf.Int64Value",
                 protobuf::FieldDescriptor::CPPTYPE_INT64);
    foo->emplace("google.protobuf.StringValue",
                 protobuf::FieldDescriptor::CPPTYPE_STRING);
    foo->emplace("google.protobuf.UInt32Value",
                 protobuf::FieldDescriptor::CPPTYPE_UINT32);
    foo->emplace("google.protobuf.UInt64Value",
                 protobuf::FieldDescriptor::CPPTYPE_UINT64);
    return foo.release();
  }();

  absl::optional<QueryParameterInfo> param_info;
  // Only attempt to make non-repeated, simple fields query parameters.
  if (!field.is_repeated() && !field.options().deprecated()) {
    if (field.cpp_type() != protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      param_info = QueryParameterInfo{
          field.cpp_type(), absl::StrCat("request.", field.name(), "()"),
          false};
    } else {
      // But also consider protobuf well known types that wrap simple types.
      auto iter = kSupportedWellKnownValueTypes->find(
          field.message_type()->full_name());
      if (iter != kSupportedWellKnownValueTypes->end()) {
        param_info = QueryParameterInfo{
            iter->second, absl::StrCat("request.", field.name(), "().value()"),
            true};
      }
    }
  }
  return param_info;
}

// Request fields not appearing in the path may not wind up as part of the json
// request body, so per https://cloud.google.com/apis/design/standard_methods,
// for HTTP transcoding we need to turn the request fields into query
// parameters.
// TODO(#10176): Consider adding support for repeated simple fields.
void SetHttpQueryParameters(HttpExtensionInfo const& info,
                            google::protobuf::MethodDescriptor const& method,
                            VarsDictionary& method_vars) {
  // The url field contains a token, or tokens, surrounded by curly braces:
  //   patch: "/v1/{parent=projects/*/instances/*}/databases"
  //   patch: "/v1/projects/{project}/instances/{instance}/databases"
  // In the first case 'parent' is expected to be found as a field in the
  // protobuf request message and is already included in the url. In the
  // second case, both 'project' and 'instance' are expected as fields in
  // the request and are already present in the url. No need to duplicate
  // these fields as query parameters.
  std::vector<std::string> param_field_names;
  param_field_names.reserve(info.field_substitutions.size());
  for (auto const& p : info.field_substitutions) {
    param_field_names.push_back(FormatFieldAccessorCall(method, p.first));
  }
  method_vars["method_http_query_parameters"] =
      FormatQueryParameterCode(method, std::move(param_field_names));
}

HttpExtensionInfo ParseHttpExtension(
    google::protobuf::MethodDescriptor const& method) {
  if (!method.options().HasExtension(google::api::http)) return {};
  auto api_version = FormatApiVersionFromPackageName(method);

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

  auto parsed_http_rule = ParsePathTemplate(url_pattern);
  if (!parsed_http_rule) {
    GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                   << " failure in ParsePathTemplate: "
                   << parsed_http_rule.status();
  }

  info.body = http_rule.body();
  info.url_path = url_pattern;

  struct SegmentAsStringVisitor {
    std::string operator()(PathTemplate::Match const&) { return "*"; }
    std::string operator()(PathTemplate::MatchRecursive const&) { return "**"; }
    std::string operator()(std::string const& s) { return s; }
    std::string operator()(PathTemplate::Variable const&) {
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__
                     << " unsupported attempt to format PathTemplate::Variable";
      return {};
    }
  };
  auto segment_formatter = [](std::string* out,
                              std::shared_ptr<PathTemplate::Segment> const& s) {
    out->append(absl::visit(SegmentAsStringVisitor{}, s->value));
  };

  auto rest_path_visitor = RestPathVisitor(api_version, info.rest_path);
  for (auto const& s : parsed_http_rule->segments) {
    if (absl::holds_alternative<PathTemplate::Variable>(s->value)) {
      auto v = absl::get<PathTemplate::Variable>(s->value);
      if (v.segments.empty()) {
        info.field_substitutions.emplace_back(v.field_path, v.field_path);
      } else {
        info.field_substitutions.emplace_back(
            v.field_path, absl::StrJoin(v.segments, "/", segment_formatter));
      }
    }

    absl::visit(rest_path_visitor, s->value);
  }

  info.rest_path_verb = parsed_http_rule->verb;
  return info;
}

bool HasHttpRoutingHeader(MethodDescriptor const& method) {
  auto result = ParseHttpExtension(method);
  return !result.field_substitutions.empty();
}

bool HasHttpAnnotation(MethodDescriptor const& method) {
  return method.options().HasExtension(google::api::http);
}

std::string FormatRequestResource(
    google::protobuf::Descriptor const& request,
    absl::variant<absl::monostate, HttpSimpleInfo, HttpExtensionInfo> const&
        parsed_http_info) {
  struct HttpInfoVisitor {
    std::string operator()(HttpExtensionInfo const& info) { return info.body; }
    std::string operator()(HttpSimpleInfo const& info) { return info.body; }
    std::string operator()(absl::monostate) { return ""; }
  };

  std::string body_field_name =
      absl::visit(HttpInfoVisitor{}, parsed_http_info);
  if (body_field_name.empty()) return "request";

  std::string field_name;
  for (int i = 0; i != request.field_count(); ++i) {
    auto const* field = request.field(i);
    if (field->name() == body_field_name) {
      field_name = FieldName(field);
    }
  }

  // TODO(#12204): The resulting field_name from this check may never differ
  // from the value in info.body due to how we generate the discovery protos.
  // In fact, we may be able to stop emitting the __json_request_body annotation
  // and remove this check.
  if (field_name.empty()) {
    for (int i = 0; i != request.field_count(); ++i) {
      auto const* field = request.field(i);
      if (field->has_json_name() &&
          field->json_name() == "__json_request_body") {
        field_name = FieldName(field);
      }
    }
  }

  if (field_name.empty()) return "request";
  return absl::StrCat("request.", field_name, "()");
}

// For protos we generate from Discovery Documents the api version is always the
// last part of the package name. Protos found in googleapis have package names
// that mirror their directory path, which ends in the api version as well.
std::string FormatApiVersionFromPackageName(
    google::protobuf::MethodDescriptor const& method) {
  std::vector<std::string> parts =
      absl::StrSplit(method.file()->package(), '.');
  if (absl::StartsWith(parts.back(), "v")) return parts.back();
  GCP_LOG(FATAL) << "Unrecognized API version in file: "
                 << method.file()->name()
                 << ", package: " << method.file()->package();
  return {};  // Suppress clang-tidy warnings
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
