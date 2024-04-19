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
#include "generator/generator_config.pb.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/doxygen.h"
#include "generator/internal/format_class_comments.h"
#include "generator/internal/format_method_comments.h"
#include "generator/internal/http_option_utils.h"
#include "generator/internal/longrunning.h"
#include "generator/internal/pagination.h"
#include "generator/internal/request_id.h"
#include "generator/internal/resolve_method_return.h"
#include "generator/internal/routing.h"
#include "generator/internal/scaffold_generator.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/types/variant.h"
#include <google/api/routing.pb.h>
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/cpp/names.h>
#include <regex>
#include <set>
#include <string>
#include <vector>

using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::compiler::cpp::FieldName;

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

void SetMethodSignatureMethodVars(
    google::protobuf::ServiceDescriptor const& service,
    google::protobuf::MethodDescriptor const& method,
    std::set<std::string> const& emitted_rpcs,
    std::set<std::string> const& omitted_rpcs, VarsDictionary& method_vars) {
  auto method_name = method.name();
  auto qualified_method_name = absl::StrCat(service.name(), ".", method_name);
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
      if (field_options.deprecated()) field_deprecated = true;
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
      method_signature_uid += absl::StrFormat("%s, ", cpp_type);
    }
    // If method signatures conflict (because the parameters are of identical
    // types), we should generate an overload for the first signature in the
    // conflict set, and drop the rest. This "first match wins" strategy means
    // it is imperative that signatures are always seen in the same order.
    //
    // See: https://google.aip.dev/client-libraries/4232#method-signatures_1
    auto p = method_signature_uids.insert(method_signature_uid);
    if (!p.second) continue;
    auto signature = absl::StrCat(
        method_name, "(",
        method_signature_uid.substr(0, method_signature_uid.length() - 2), ")");
    auto qualified_signature = absl::StrCat(service.name(), ".", signature);
    auto any_match = [&](std::string const& v) {
      return v == method_name || v == qualified_method_name || v == signature ||
             v == qualified_signature;
    };
    if (internal::ContainsIf(omitted_rpcs, any_match)) continue;
    if (field_deprecated) {
      // RPCs with deprecated fields must be listed in either omitted_rpcs
      // or emitted_rpcs. The former is used for newly-generated services,
      // where we never want to support the deprecated field, and the
      // latter for newly-deprecated fields, where we want to maintain
      // backwards compatibility.
      if (!internal::ContainsIf(emitted_rpcs, any_match)) {
        GCP_LOG(FATAL) << "Deprecated RPC " << qualified_signature
                       << " must be listed in either omitted_rpcs"
                       << " or emitted_rpcs";
      }
      method_vars["uses_deprecated_field"] = "true";
      // TODO(#8486): Add a @deprecated Doxygen comment and the
      // GOOGLE_CLOUD_CPP_DEPRECATED annotation to the generated RPC.
    }
    std::string key = "method_signature" + std::to_string(i);
    method_vars[key] = method_signature;
    std::string key2 = "method_request_setters" + std::to_string(i);
    method_vars[key2] = method_request_setters;
  }
}

bool IsKnownIdempotentMethod(google::protobuf::MethodDescriptor const& m) {
  return (m.name() == "GetIamPolicy" &&
          m.output_type()->full_name() == "google.iam.v1.Policy" &&
          m.input_type()->full_name() == "google.iam.v1.GetIamPolicyRequest") ||
         (m.name() == "TestIamPermissions" &&
          m.output_type()->full_name() ==
              "google.iam.v1.TestIamPermissionsResponse" &&
          m.input_type()->full_name() ==
              "google.iam.v1.TestIamPermissionsRequest");
}

std::string DefaultIdempotencyFromHttpOperation(
    google::protobuf::MethodDescriptor const& method) {
  if (IsKnownIdempotentMethod(method)) return "kIdempotent";
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

// Apply substitutions to the comments snarfed from the proto file for
// method_signature parameters. This is mostly for the benefit of Doxygen,
// but is also to fix mismatched quotes, etc.
struct ParameterCommentSubstitution {
  absl::string_view before;
  absl::string_view after;
  std::size_t uses = 0;
};

auto constexpr kDialogflowCXEnvironmentIdProto1 = R"""(
 list all environments for. Format: `projects/<Project
 ID>/locations/<Location ID>/agents/<Agent ID>/environments/<Environment
 ID>`.)""";

auto constexpr kDialogflowCXEnvironmentIdCpp1 = R"""(
 list all environments for. Format:

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/environments/<Environment ID>
 @endcode)""";

auto constexpr kDialogflowCXEnvironmentIdProto2 = R"""(
 Format: `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/environments/<Environment ID>`.)""";

auto constexpr kDialogflowCXEnvironmentIdCpp2 = R"""(
 Format:

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/environments/<Environment ID>
 @endcode)""";

auto constexpr kDialogflowCXSessionIdProto = R"""(
 Format: `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/sessions/<Session ID>` or `projects/<Project ID>/locations/<Location
 ID>/agents/<Agent ID>/environments/<Environment ID>/sessions/<Session ID>`.)""";

auto constexpr kDialogflowCXSessionIdCpp = R"""(
 Format:

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/sessions/<Session ID>
 @endcode

 or

 @code
 projects/<Project ID>/locations/<Location ID>/agents/<Agent ID>/environments/<Environment ID>/sessions/<Session ID>
 @endcode)""";

auto constexpr kDialogflowCXEntityTypeIdProto = R"""(
 Format: `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/sessions/<Session ID>/entityTypes/<Entity Type ID>` or
 `projects/<Project ID>/locations/<Location ID>/agents/<Agent
 ID>/environments/<Environment ID>/sessions/<Session ID>/entityTypes/<Entity
 Type ID>`. If `Environment ID` is not specified, we assume default 'draft'
 environment.)""";

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
 environment.)""";

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
 @endcode)""";

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
 @endcode)""";

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
 @endcode)""";

auto constexpr kLoggingConfigClientProto1 =
    R"""( The resource name of the link:

   "projects/[PROJECT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]"
   "organizations/[ORGANIZATION_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]"
   "billingAccounts/[BILLING_ACCOUNT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]"
   "folders/[FOLDER_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID])""";

auto constexpr kLoggingConfigClientCpp1 =
    R"""( The resource name of the link:

   "projects/[PROJECT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]"
   "organizations/[ORGANIZATION_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]"
   "billingAccounts/[BILLING_ACCOUNT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]"
   "folders/[FOLDER_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/[LINK_ID]")""";

auto constexpr kLoggingConfigClientProto2 =
    R"""( The parent resource whose links are to be listed:

   "projects/[PROJECT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/links/"
   "organizations/[ORGANIZATION_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/"
   "billingAccounts/[BILLING_ACCOUNT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/"
   "folders/[FOLDER_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]/)""";

auto constexpr kLoggingConfigClientCpp2 =
    R"""( The parent resource whose links are to be listed:

   "projects/[PROJECT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]"
   "organizations/[ORGANIZATION_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]"
   "billingAccounts/[BILLING_ACCOUNT_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]"
   "folders/[FOLDER_ID]/locations/[LOCATION_ID]/buckets/[BUCKET_ID]")""";

ParameterCommentSubstitution substitutions[] = {
    // Unescaped elements in bigtable/admin/v2.
    {" projects/<project>/instances/<instance>/"
     "tables/<table>/authorizedViews/<authorized_view>",
     " `projects/<project>/instances/<instance>/"
     "tables/<table>/authorizedViews/<authorized_view>`"},

    // From dialogflow/cx/v3.
    {kDialogflowCXEnvironmentIdProto1, kDialogflowCXEnvironmentIdCpp1},
    {kDialogflowCXEnvironmentIdProto2, kDialogflowCXEnvironmentIdCpp2},
    {kDialogflowCXSessionIdProto, kDialogflowCXSessionIdCpp},
    {kDialogflowCXEntityTypeIdProto, kDialogflowCXEntityTypeIdCpp},
    {"<Agent\n ID>", "<Agent ID>"},
    {"<Location\n ID>", "<Location ID>"},
    {"<Transition\n Route Group ID>", "<Transition Route Group ID>"},
    {"<Agent ID>.", "<Agent ID>`."},  // missing close quote

    // From dialogflow/v2.
    {kDialogflowESSessionIdProto, kDialogflowESSessionIdCpp},
    {kDialogflowESContextIdProto, kDialogflowESContextIdCpp},
    {kDialogflowESSessionEntityTypeDisplayNameProto,
     kDialogflowESSessionEntityTypeDisplayNameCpp},

    // From logging/v2.
    {kLoggingConfigClientProto1, kLoggingConfigClientCpp1},
    {kLoggingConfigClientProto2, kLoggingConfigClientCpp2},

    // From artifactregistry/v1, where a missing closing quote confuses
    // the Doxygen parser.
    {R"""("projects/p1/locations/us-central1/repositories/repo1)""",
     R"""("projects/p1/locations/us-central1/repositories/repo1")"""},

    // Unescaped elements in spanner/admin/instance/v1.
    {" <parent>/instanceConfigs/us-east1,",
     " `<parent>/instanceConfigs/us-east1`,"},
    {" <parent>/instanceConfigs/nam3.", " `<parent>/instanceConfigs/nam3`."},

    // Extra quotes in asset/v1.
    {R"""( "folders/12345")", or a )""", R"""( "folders/12345"), or a )"""},

    // Doxygen gets confused by single quotes in code spans:
    //    https://www.doxygen.nl/manual/markdown.html#mddox_code_spans
    // The workaround is to double quote these:
    {R"""(`{instance} = '-'`)""", R"""(``{instance} = '-'``)"""},
    {R"""(`{cluster} = '-'`)""", R"""(``{cluster} = '-'``)"""},
    {R"""(`projects/<Project ID or '-'>`)""",
     R"""(``projects/<Project ID or '-'>``)"""},

    // Further trim some initial paragraphs for long descriptions.
    {R"""( The included patch
 environment would specify the scikit-learn version as follows:)""",
     ""},
    {R"""( The elements of the repeated paths field can only include these
 fields from [AwsCluster][google.cloud.gkemulticloud.v1.AwsCluster]:)""",
     ""},
    {R"""( The elements of the repeated paths field can only include these
 fields from [AwsNodePool][google.cloud.gkemulticloud.v1.AwsNodePool]:)""",
     ""},
    {R"""( For more information, see the
 [LogEntry][google.logging.v2.LogEntry] type.)""",
     ""},
    {R"""(and the `PATCH` request body would specify the new value, as follows:)""",
     R"""(and the `PATCH` request body would specify the new value.)"""},
    {"fields. Some eligible fields are:", "fields."},
    {" The allowable fields to\n update are:", ""},

    // These appear in google/api/servicemanagement/v1/servicemanager.proto
    // Doxygen gets confused by single quotes in code spans:
    //    https://www.doxygen.nl/manual/markdown.html#mddox_code_spans
    // The workaround is to double quote these:
    {"`filter='status=SUCCESS'`", "``filter='status=SUCCESS'``"},
    {"`filter='strategy=TrafficPercentStrategy'`",
     "``filter='strategy=TrafficPercentStrategy'``"},

    {"conversionWorkspace/123/mappingRules/"
     "rule123@c7cfa2a8c7cfa2a8c7cfa2a8c7cfa2a8",
     "`conversionWorkspace/123/mappingRules/"
     "rule123@c7cfa2a8c7cfa2a8c7cfa2a8c7cfa2a8`"},

    // From google/cloud/gkemulticloud/v1/azure_service.proto
    {" projects/<project-id>/locations/<region>/azureClusters/<cluster-id>",
     " `projects/<project-id>/locations/<region>/azureClusters/<cluster-id>`"},

    // From google/cloud/aiplatform/v1/feature_registry_service.proto
    {"`projects/{project}/locations/{location}'`",
     "`projects/{project}/locations/{location}`"},

    // Some comments include multiple newlines in a row. We need to preserve
    // these because they are paragraph separators. When used in `@param`
    // commands we need to represent them as `@n` or they do would terminate the
    // `@param` comment. No comments use more than three newlines in a row at
    // the moment.
    {"\n\n\n", "\n @n\n"},
    {"\n\n", "\n @n\n"},

    // Finally, the next line after a newline needs to start as a comment.
    {"\n", "\n  /// "},
};

// Very long parameters need different formatting.
auto constexpr kShortParamFormat = "  /// @param %s %s\n";
auto constexpr kLongParamFormat = R"""(  /// @param %s %s
  ///  @n
  ///  For more information, see [%s][%s].
)""";

std::string FormattedCommentsForParameter(
    google::protobuf::MethodDescriptor const& method,
    std::string const& parameter) {
  google::protobuf::Descriptor const* input_type = method.input_type();
  google::protobuf::FieldDescriptor const* parameter_descriptor =
      input_type->FindFieldByName(parameter);
  google::protobuf::SourceLocation loc;
  parameter_descriptor->GetSourceLocation(&loc);
  auto comment = EscapePrinterDelimiter(ChompByValue(loc.leading_comments));
  // This is an arbitrary threshold. The intent is to simplify the generator
  // code for corner cases. In the few cases where the documentation of a field
  // is extremely detailed it manages to confuse Doxygen. We could try to
  // reformat the comments until Doxygen works. Considering that there are very
  // few of these cases, and that the long descriptions are confusing when
  // reading a single function documentation, we just link the full method
  // documentation and skip the details.
  auto constexpr kTooManyLines = 20;
  if (std::count(comment.begin(), comment.end(), '\n') > kTooManyLines) {
    std::vector<absl::string_view> paragraphs = absl::StrSplit(comment, "\n\n");
    auto brief = std::string{paragraphs.front()};
    for (auto& sub : substitutions) {
      sub.uses += absl::StrReplaceAll({{sub.before, sub.after}}, &brief);
    }
    return absl::StrFormat(kLongParamFormat, FieldName(parameter_descriptor),
                           brief, method.input_type()->name(),
                           method.input_type()->full_name());
  }

  for (auto& sub : substitutions) {
    sub.uses += absl::StrReplaceAll({{sub.before, sub.after}}, &comment);
  }
  return absl::StrFormat(kShortParamFormat, FieldName(parameter_descriptor),
                         std::move(comment));
}

std::string FormatApiMethodSignatureParameters(
    google::protobuf::MethodDescriptor const& method,
    std::string const& signature) {
  std::string parameter_comments;
  std::vector<std::string> parameters =
      absl::StrSplit(signature, ',', absl::SkipEmpty());
  for (auto& parameter : parameters) {
    absl::StripAsciiWhitespace(&parameter);
    absl::StrAppend(&parameter_comments,
                    FormattedCommentsForParameter(method, parameter));
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

std::string TransientErrorsComment(VarsDictionary const& vars) {
  auto loc = vars.find("retryable_status_codes");
  if (loc == vars.end()) return {};
  std::set<std::string> codes = absl::StrSplit(loc->second, ',');

  loc = vars.find("service_name");
  auto const service_name = loc == vars.end() ? std::string{} : loc->second;

  std::string comment = R"""(
 * In this class the following status codes are treated as transient errors:)""";

  auto format = [](absl::string_view code) {
    auto constexpr kCodeFormat = R"""(
 * - [`%s`](@ref google::cloud::StatusCode))""";
    return absl::StrFormat(kCodeFormat, code);
  };
  for (auto const& code : codes) {
    std::pair<std::string, std::string> service_code =
        absl::StrSplit(code, '.');
    if (service_code.second.empty()) {
      comment += format(service_code.first);
      continue;
    }
    if (service_code.first == service_name) {
      comment += format(service_code.second);
      continue;
    }
  }
  return comment;
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

// If a service name mapping exists, return the new name.
// Parses a command line argument in the form:
// {"service_name_mappings": "service_a=new_service_a,service=new_service"}.
std::string GetEffectiveServiceName(VarsDictionary const& vars,
                                    absl::string_view name) {
  auto service_name_mappings = vars.find("service_name_mappings");
  if (service_name_mappings != vars.end()) {
    for (absl::string_view arg :
         absl::StrSplit(service_name_mappings->second, ',')) {
      std::pair<absl::string_view, absl::string_view> p =
          absl::StrSplit(arg, absl::MaxSplits('=', 1));
      if (p.first == name) return std::string(p.second.data(), p.second.size());
    }
  }
  return std::string(name.data(), name.size());
}

// If a service name mapping exists, return the replacement comment.
// If it does not exist, return a null optional.
// Parses a command line argument in the form:
// {"service_name_to_comments": "service_a=comment_a,service_b=comment_b"}.
absl::optional<std::string> GetReplacementComment(VarsDictionary const& vars,
                                                  absl::string_view name) {
  auto service_name_to_comments = vars.find("service_name_to_comments");
  if (service_name_to_comments == vars.end()) {
    return absl::nullopt;
  }
  for (absl::string_view arg :
       absl::StrSplit(service_name_to_comments->second, ',')) {
    std::pair<absl::string_view, absl::string_view> p =
        absl::StrSplit(arg, absl::MaxSplits('=', 1));
    if (p.first == name) return std::string(p.second.data(), p.second.size());
  }
  return absl::nullopt;
}

}  // namespace

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

std::string FormatMethodCommentsMethodSignature(
    google::protobuf::MethodDescriptor const& method,
    std::string const& signature, bool is_discovery_document_proto) {
  auto parameter_comments =
      FormatApiMethodSignatureParameters(method, signature);
  return FormatMethodComments(method, std::move(parameter_comments),
                              is_discovery_document_proto);
}

std::string FormatMethodCommentsProtobufRequest(
    google::protobuf::MethodDescriptor const& method,
    bool is_discovery_document_proto) {
  auto constexpr kRequestParam =
      R"""(  /// @param request Unary RPCs, such as the one wrapped by this
  ///     function, receive a single `request` proto message which includes all
  ///     the inputs for the RPC. In this case, the proto message is a
  ///     [%s].
  ///     Proto messages are converted to C++ classes by Protobuf, using the
  ///     [Protobuf mapping rules].
)""";
  return FormatMethodComments(
      method, absl::StrFormat(kRequestParam, method.input_type()->full_name()),
      is_discovery_document_proto);
}

bool CheckParameterCommentSubstitutions() {
  bool all_substitutions_used = true;
  for (auto& sub : substitutions) {
    if (sub.uses == 0) {
      GCP_LOG(ERROR) << "Parameter comment substitution went unused ("
                     << sub.before << ")";
      all_substitutions_used = false;
    }
  }
  return all_substitutions_used;
}

VarsDictionary CreateServiceVars(
    google::protobuf::ServiceDescriptor const& descriptor,
    std::vector<std::pair<std::string, std::string>> const& initial_values) {
  VarsDictionary vars(initial_values.begin(), initial_values.end());
  auto const& service_name = GetEffectiveServiceName(vars, descriptor.name());
  vars["product_options_page"] = OptionsGroup(vars["product_path"]);
  vars["additional_pb_header_paths"] = FormatAdditionalPbHeaderPaths(vars);
  vars["api_version"] =
      descriptor.options().GetExtension(google::api::api_version);
  vars["class_comment_block"] = FormatClassCommentsFromServiceComments(
      descriptor, service_name, GetReplacementComment(vars, service_name));
  vars["client_class_name"] = absl::StrCat(service_name, "Client");
  vars["client_cc_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(service_name), "_client.cc");
  vars["client_header_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(service_name), "_client.h");
  vars["client_samples_cc_path"] =
      absl::StrCat(vars["product_path"], "samples/",
                   ServiceNameToFilePath(service_name), "_client_samples.cc");
  vars["connection_class_name"] = absl::StrCat(service_name, "Connection");
  vars["connection_cc_path"] =
      absl::StrCat(vars["product_path"], ServiceNameToFilePath(service_name),
                   "_connection.cc");
  vars["connection_header_path"] =
      absl::StrCat(vars["product_path"], ServiceNameToFilePath(service_name),
                   "_connection.h");
  vars["connection_rest_cc_path"] =
      absl::StrCat(vars["product_path"], ServiceNameToFilePath(service_name),
                   "_rest_connection.cc");
  vars["connection_rest_header_path"] =
      absl::StrCat(vars["product_path"], ServiceNameToFilePath(service_name),
                   "_rest_connection.h");
  vars["connection_impl_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_connection_impl.cc");
  vars["connection_impl_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_connection_impl.h");
  vars["connection_impl_rest_class_name"] =
      absl::StrCat(service_name, "RestConnectionImpl");
  vars["connection_impl_rest_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_connection_impl.cc");
  vars["connection_impl_rest_header_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_connection_impl.h");
  vars["connection_options_name"] =
      absl::StrCat(service_name, "ConnectionOptions");
  vars["connection_options_traits_name"] =
      absl::StrCat(service_name, "ConnectionOptionsTraits");
  vars["forwarding_client_header_path"] =
      absl::StrCat(vars["forwarding_product_path"],
                   ServiceNameToFilePath(service_name), "_client.h");
  vars["forwarding_connection_header_path"] =
      absl::StrCat(vars["forwarding_product_path"],
                   ServiceNameToFilePath(service_name), "_connection.h");
  vars["forwarding_idempotency_policy_header_path"] = absl::StrCat(
      vars["forwarding_product_path"], ServiceNameToFilePath(service_name),
      "_connection_idempotency_policy.h");
  vars["forwarding_mock_connection_header_path"] =
      absl::StrCat(vars["forwarding_product_path"], "mocks/mock_",
                   ServiceNameToFilePath(service_name), "_connection.h");
  vars["forwarding_options_header_path"] =
      absl::StrCat(vars["forwarding_product_path"],
                   ServiceNameToFilePath(service_name), "_options.h");
  vars["grpc_service"] = descriptor.full_name();
  vars["grpc_stub_fqn"] = ProtoNameToCppName(descriptor.full_name());
  vars["idempotency_class_name"] =
      absl::StrCat(service_name, "ConnectionIdempotencyPolicy");
  vars["idempotency_policy_cc_path"] =
      absl::StrCat(vars["product_path"], ServiceNameToFilePath(service_name),
                   "_connection_idempotency_policy.cc");
  vars["idempotency_policy_header_path"] =
      absl::StrCat(vars["product_path"], ServiceNameToFilePath(service_name),
                   "_connection_idempotency_policy.h");
  vars["limited_error_count_retry_policy_name"] =
      absl::StrCat(service_name, "LimitedErrorCountRetryPolicy");
  vars["limited_time_retry_policy_name"] =
      absl::StrCat(service_name, "LimitedTimeRetryPolicy");
  vars["auth_class_name"] = absl::StrCat(service_name, "Auth");
  vars["auth_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_auth_decorator.cc");
  vars["auth_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_auth_decorator.h");
  vars["logging_class_name"] = absl::StrCat(service_name, "Logging");
  vars["logging_cc_path"] = absl::StrCat(vars["product_path"], "internal/",
                                         ServiceNameToFilePath(service_name),
                                         "_logging_decorator.cc");
  vars["logging_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_logging_decorator.h");
  vars["logging_rest_class_name"] = absl::StrCat(service_name, "RestLogging");
  vars["logging_rest_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_logging_decorator.cc");
  vars["logging_rest_header_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_logging_decorator.h");
  vars["metadata_class_name"] = absl::StrCat(service_name, "Metadata");
  vars["metadata_cc_path"] = absl::StrCat(vars["product_path"], "internal/",
                                          ServiceNameToFilePath(service_name),
                                          "_metadata_decorator.cc");
  vars["metadata_header_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_metadata_decorator.h");
  vars["metadata_rest_class_name"] = absl::StrCat(service_name, "RestMetadata");
  vars["metadata_rest_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_metadata_decorator.cc");
  vars["metadata_rest_header_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_metadata_decorator.h");
  vars["mock_connection_class_name"] =
      absl::StrCat("Mock", service_name, "Connection");
  vars["mock_connection_header_path"] =
      absl::StrCat(vars["product_path"], "mocks/mock_",
                   ServiceNameToFilePath(service_name), "_connection.h");
  vars["option_defaults_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_option_defaults.cc");
  vars["option_defaults_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_option_defaults.h");
  vars["options_header_path"] = absl::StrCat(
      vars["product_path"], ServiceNameToFilePath(service_name), "_options.h");
  vars["product_namespace"] = Namespace(vars["product_path"]);
  vars["product_internal_namespace"] =
      Namespace(vars["product_path"], NamespaceType::kInternal);
  vars["proto_file_name"] = descriptor.file()->name();
  vars["proto_grpc_header_path"] = absl::StrCat(
      absl::StripSuffix(descriptor.file()->name(), ".proto"), ".grpc.pb.h");
  vars["proto_header_path"] = absl::StrCat(
      absl::StripSuffix(descriptor.file()->name(), ".proto"), ".pb.h");
  vars["retry_policy_name"] = absl::StrCat(service_name, "RetryPolicy");
  vars["retry_traits_name"] = absl::StrCat(service_name, "RetryTraits");
  vars["retry_traits_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_retry_traits.h");
  vars["round_robin_class_name"] = absl::StrCat(service_name, "RoundRobin");
  vars["round_robin_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_round_robin_decorator.cc");
  vars["round_robin_header_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_round_robin_decorator.h");
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
  vars["service_name"] = service_name;
  vars["stub_class_name"] = absl::StrCat(service_name, "Stub");
  vars["stub_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_stub.cc");
  vars["stub_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_stub.h");
  vars["stub_rest_class_name"] = absl::StrCat(service_name, "RestStub");
  vars["stub_rest_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_rest_stub.cc");
  vars["stub_rest_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_rest_stub.h");
  vars["stub_factory_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_stub_factory.cc");
  vars["stub_factory_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_stub_factory.h");
  vars["stub_factory_rest_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_rest_stub_factory.cc");
  vars["stub_factory_rest_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_rest_stub_factory.h");
  vars["tracing_connection_class_name"] =
      absl::StrCat(service_name, "TracingConnection");
  vars["tracing_connection_cc_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_tracing_connection.cc");
  vars["tracing_connection_header_path"] = absl::StrCat(
      vars["product_path"], "internal/", ServiceNameToFilePath(service_name),
      "_tracing_connection.h");
  vars["tracing_stub_class_name"] = absl::StrCat(service_name, "TracingStub");
  vars["tracing_stub_cc_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_tracing_stub.cc");
  vars["tracing_stub_header_path"] =
      absl::StrCat(vars["product_path"], "internal/",
                   ServiceNameToFilePath(service_name), "_tracing_stub.h");
  SetRetryStatusCodeExpression(vars);
  vars["transient_errors_comment"] = TransientErrorsComment(vars);
  SetLongrunningOperationServiceVars(descriptor, vars);
  return vars;
}

std::map<std::string, std::string> ParseIdempotencyOverrides(
    VarsDictionary const& vars) {
  using ::google::cloud::cpp::generator::ServiceConfiguration;
  auto iter = vars.find("idempotency_overrides");
  if (iter == vars.end()) return {};
  std::map<std::string, std::string> parsed_overrides;
  for (auto const& idempotency_override :
       absl::StrSplit(iter->second, absl::ByChar(','))) {
    std::pair<std::string, std::string> override_splits =
        absl::StrSplit(idempotency_override, absl::ByChar(':'));
    auto idempotency =
        ServiceConfiguration::IdempotencyOverride::NON_IDEMPOTENT;
    ServiceConfiguration::IdempotencyOverride::Idempotency_Parse(
        override_splits.second, &idempotency);
    parsed_overrides[override_splits.first] =
        (idempotency == ServiceConfiguration::IdempotencyOverride::IDEMPOTENT
             ? "kIdempotent"
             : "kNonIdempotent");
  }
  return parsed_overrides;
}

std::map<std::string, VarsDictionary> CreateMethodVars(
    google::protobuf::ServiceDescriptor const& service,
    YAML::Node const& service_config, VarsDictionary const& vars) {
  auto split_arg = [&vars](std::string const& arg) -> std::set<std::string> {
    auto l = vars.find(arg);
    if (l == vars.end()) return {};
    std::vector<std::string> s = absl::StrSplit(l->second, ',');
    for (auto& a : s) a = SafeReplaceAll(a, "@", ",");
    return {s.begin(), s.end()};
  };
  auto const emitted_rpcs = split_arg("emitted_rpcs");
  auto const omitted_rpcs = split_arg("omitted_rpcs");
  auto const idempotency_overrides = ParseIdempotencyOverrides(vars);
  std::map<std::string, VarsDictionary> service_methods_vars;
  for (int i = 0; i < service.method_count(); i++) {
    auto const& method = *service.method(i);
    VarsDictionary method_vars;
    method_vars["method_return_doxygen_link"] =
        FormatDoxygenLink(*method.output_type());
    method_vars["idempotency"] = DefaultIdempotencyFromHttpOperation(method);
    if (!idempotency_overrides.empty()) {
      auto iter = idempotency_overrides.find(
          absl::StrCat(service.name(), ".", method.name()));
      if (iter != idempotency_overrides.end()) {
        method_vars["idempotency"] = iter->second;
      }
    }
    method_vars["method_name"] = method.name();
    method_vars["method_name_snake"] = CamelCaseToSnakeCase(method.name());
    method_vars["request_type"] =
        ProtoNameToCppName(method.input_type()->full_name());
    method_vars["response_message_type"] = method.output_type()->full_name();
    method_vars["response_type"] =
        ProtoNameToCppName(method.output_type()->full_name());
    auto request_id_field_name = RequestIdFieldName(service_config, method);
    if (!request_id_field_name.empty()) {
      method_vars["request_id_field_name"] = std::move(request_id_field_name);
    }
    SetLongrunningOperationMethodVars(method, method_vars);
    AssignPaginationMethodVars(method, method_vars);
    SetMethodSignatureMethodVars(service, method, emitted_rpcs, omitted_rpcs,
                                 method_vars);
    auto parsed_http_info = ParseHttpExtension(method);
    method_vars["request_resource"] =
        FormatRequestResource(*method.input_type(), parsed_http_info);
    SetHttpDerivedMethodVars(parsed_http_info, method, method_vars);
    SetHttpQueryParameters(parsed_http_info, method, method_vars);
    service_methods_vars[method.full_name()] = method_vars;
  }
  return service_methods_vars;
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
