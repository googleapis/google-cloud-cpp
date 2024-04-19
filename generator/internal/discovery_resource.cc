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

#include "generator/internal/discovery_resource.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include <iostream>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

// Defining long running operations in Discovery Documents relies upon
// conventions. This implements the convention used by compute. It may be
// that we need to introduce additional in the future if we come across other
// LRO defining conventions.
// https://cloud.google.com/compute/docs/regions-zones/global-regional-zonal-resources
absl::optional<std::string> DetermineLongRunningOperationService(
    nlohmann::json const& method_json, std::vector<std::string> const& params,
    std::set<std::string> const& operation_services,
    std::string const& resource_name) {
  // Only services NOT considered operation_services should be generated
  // using the asynchronous LRO framework, even if they have a response of type
  // Operation.
  if (method_json.contains("response") &&
      method_json["response"].value("$ref", "") == "Operation" &&
      !internal::Contains(operation_services,
                          CapitalizeFirstLetter(resource_name))) {
    if (internal::Contains(params, "zone")) {
      return "ZoneOperations";
    }
    if (internal::Contains(params, "region")) {
      return "RegionOperations";
    }
    if (internal::Contains(params, "project")) {
      return "GlobalOperations";
    }
    return "GlobalOrganizationOperations";
  }
  return absl::nullopt;
}

}  // namespace

DiscoveryResource::DiscoveryResource() : json_("") {}

DiscoveryResource::DiscoveryResource(std::string name, std::string package_name,
                                     nlohmann::json json)
    : name_(std::move(name)),
      package_name_(std::move(package_name)),
      has_empty_request_or_response_(false),
      json_(std::move(json)) {}

void DiscoveryResource::AddRequestType(std::string name,
                                       DiscoveryTypeVertex* type) {
  request_types_.insert(std::make_pair(std::move(name), type));
}

void DiscoveryResource::AddResponseType(std::string name,
                                        DiscoveryTypeVertex* type) {
  response_types_.insert(std::make_pair(std::move(name), type));
}

std::vector<DiscoveryTypeVertex*> DiscoveryResource::GetRequestTypesList()
    const {
  std::vector<DiscoveryTypeVertex*> v;
  std::transform(request_types_.begin(), request_types_.end(),
                 std::back_inserter(v),
                 [](std::pair<std::string, DiscoveryTypeVertex*> const& p) {
                   return p.second;
                 });
  return v;
}

StatusOr<std::string> DiscoveryResource::GetServiceApiVersion() const {
  if (!service_api_version_) {
    return internal::InternalError(
        "SetServiceApiVersion must be called before JsonToProtobufService is "
        "called",
        GCP_ERROR_INFO().WithMetadata("json", json_.dump()));
  }

  return *service_api_version_;
}

Status DiscoveryResource::SetServiceApiVersion() {
  if (!json_.contains("methods")) {
    service_api_version_ = internal::InvalidArgumentError(
        "resource contains no methods",
        GCP_ERROR_INFO().WithMetadata("json", json_.dump()));
    return service_api_version_->status();
  }

  for (auto const& m : *json_.find("methods")) {
    std::string method_api_version = m.value("apiVersion", "");
    if (!service_api_version_) service_api_version_ = method_api_version;
    if (**service_api_version_ != method_api_version) {
      service_api_version_ = internal::InvalidArgumentError(
          "resource contains methods with different apiVersion values",
          GCP_ERROR_INFO().WithMetadata("json", json_.dump()));
      return service_api_version_->status();
    }
  }
  return {};
}

std::string DiscoveryResource::FormatUrlPath(std::string const& path) {
  std::string output;
  std::size_t current = 0;
  for (auto open = path.find('{'); open != std::string::npos;
       open = path.find('{', current)) {
    absl::StrAppend(&output, path.substr(current, open - current + 1));
    current = open + 1;
    auto close = path.find('}', current);
    absl::StrAppend(
        &output, CamelCaseToSnakeCase(path.substr(current, close - current)));
    current = close;
  }
  absl::StrAppend(&output, path.substr(current));
  return output;
}

StatusOr<std::string> DiscoveryResource::FormatRpcOptions(
    nlohmann::json const& method_json, std::string const& base_path,
    std::set<std::string> const& operation_services,
    DiscoveryTypeVertex const* request_type) const {
  std::vector<std::string> rpc_options;
  std::string verb = absl::AsciiStrToLower(method_json.value("httpMethod", ""));
  std::string path = method_json.value("path", "");

  if (verb.empty() || path.empty()) {
    return internal::InvalidArgumentError(
        "Method does not define httpMethod and/or path.",
        GCP_ERROR_INFO().WithMetadata("json", method_json.dump()));
  }

  std::string http_option = absl::StrFormat(
      "    option (google.api.http) = {\n"
      "      %s: \"%s\"\n",
      verb,
      absl::StrCat(absl::StripSuffix(base_path, "/"), "/",
                   FormatUrlPath(path)));
  std::string request_resource_field_name;
  if (request_type && (verb == "post" || verb == "patch" || verb == "put")) {
    std::string http_body =
        request_type->json().value("request_resource_field_name", "*");
    if (http_body != "*") request_resource_field_name = http_body;
    absl::StrAppend(&http_option,
                    absl::StrFormat("      body: \"%s\"\n", http_body));
  }
  rpc_options.push_back(absl::StrCat(http_option, "    };"));

  std::vector<std::string> parameter_order =
      method_json.value("parameterOrder", std::vector<std::string>{});
  if (!parameter_order.empty()) {
    // Workaround for necessary, but not marked REQUIRED, mask field for update
    // methods. AIP-134 indicates that the update mask should be provided.
    if (verb == "patch" && !internal::Contains(parameter_order, "updateMask")) {
      nlohmann::json parameters =
          method_json.value("parameters", nlohmann::json());
      if (parameters.find("updateMask") != parameters.end()) {
        parameter_order.emplace_back("updateMask");
      }
    }
    if (!request_resource_field_name.empty()) {
      parameter_order.push_back(request_resource_field_name);
    }
    rpc_options.push_back(
        absl::StrFormat("    option (google.api.method_signature) = \"%s\";",
                        absl::StrJoin(parameter_order, ",",
                                      [](std::string* s, std::string const& p) {
                                        *s += CamelCaseToSnakeCase(p);
                                      })));
  }

  auto longrunning_operation_service = DetermineLongRunningOperationService(
      method_json, parameter_order, operation_services, name_);
  if (longrunning_operation_service) {
    rpc_options.push_back(
        absl::StrFormat("    option (google.cloud.operation_service) = \"%s\";",
                        *longrunning_operation_service));
  }

  return absl::StrJoin(rpc_options, "\n");
}

StatusOr<std::string> DiscoveryResource::FormatOAuthScopes() const {
  std::set<std::string> oauth_scopes;
  auto const& methods = json_.find("methods");
  for (auto const& iter : *methods) {
    auto const& scopes = iter.find("scopes");
    if (scopes == iter.end()) break;
    for (auto const& s : *scopes) {
      oauth_scopes.insert(std::string(s));
    }
  }
  if (oauth_scopes.empty()) {
    return internal::InvalidArgumentError(
        absl::StrFormat("No OAuth scopes found for service: %s.", name_));
  }
  return absl::StrCat("    \"", absl::StrJoin(oauth_scopes, ",\"\n    \""),
                      "\";\n");
}

std::string DiscoveryResource::FormatFilePath(
    std::string const& product_name, std::string const& version,
    std::string const& output_path) const {
  if (output_path.empty()) {
    return absl::StrJoin(
        {std::string("google/cloud"), product_name, CamelCaseToSnakeCase(name_),
         version, absl::StrCat(CamelCaseToSnakeCase(name_), ".proto")},
        "/");
  }
  return absl::StrJoin({output_path, std::string("google/cloud"), product_name,
                        CamelCaseToSnakeCase(name_), version,
                        absl::StrCat(CamelCaseToSnakeCase(name_), ".proto")},
                       "/");
}

std::string DiscoveryResource::GetMethodResponseTypeName() const {
  std::string get_ref;
  auto methods_iter = json_.find("methods");
  if (methods_iter != json_.end()) {
    auto get_method_iter = methods_iter->find("get");
    if (get_method_iter != methods_iter->end()) {
      auto response_iter = get_method_iter->find("response");
      if (response_iter != get_method_iter->end()) {
        get_ref = response_iter->value("$ref", "");
      }
    }
  }

  return get_ref;
}

std::string DiscoveryResource::FormatMethodName(std::string method_name) const {
  constexpr char const* kPluralPrimitives[] = {"AggregatedList", "List"};
  constexpr char const* kSingularPrimitives[] = {"Delete", "Get", "Insert",
                                                 "Patch", "Update"};
  method_name = CapitalizeFirstLetter(method_name);
  if (internal::Contains(kPluralPrimitives, method_name)) {
    return absl::StrCat(method_name, CapitalizeFirstLetter(name_));
  }
  if (internal::Contains(kSingularPrimitives, method_name)) {
    std::string get_method_response_name = GetMethodResponseTypeName();
    return absl::StrCat(
        method_name,
        CapitalizeFirstLetter(get_method_response_name.empty()
                                  ? name_
                                  : std::move(get_method_response_name)));
  }
  return method_name;
}

StatusOr<std::string> DiscoveryResource::JsonToProtobufService(
    DiscoveryDocumentProperties const& document_properties) const {
  std::vector<std::string> service_text;
  auto constexpr kServiceComments =
      R"""(Service for the %s resource. https://cloud.google.com/$product_name$/docs/reference/rest/$version$/%s
)""";
  service_text.push_back(
      absl::StrFormat("service %s {", CapitalizeFirstLetter(name_)));
  service_text.push_back(
      absl::StrFormat("  option (google.api.default_host) = \"%s\";",
                      document_properties.default_hostname));
  auto service_api_version = GetServiceApiVersion();
  if (!service_api_version) return service_api_version.status();
  if (!service_api_version->empty()) {
    service_text.push_back(absl::StrFormat(
        "  option (google.api.api_version) = \"%s\";", *service_api_version));
  }
  auto scopes = FormatOAuthScopes();
  if (!scopes) return std::move(scopes).status();
  service_text.push_back(
      absl::StrFormat("  option (google.api.oauth_scopes) =\n%s\n", *scopes));

  auto const& methods = json_.find("methods");
  std::vector<std::string> rpcs_text;
  for (auto iter = methods->begin(); iter != methods->end(); ++iter) {
    std::vector<std::string> rpc_text;
    std::string method_name = FormatMethodName(iter.key());
    auto const& method_json = iter.value();

    std::string request_type_name = "google.protobuf.Empty";
    DiscoveryTypeVertex const* request_type = nullptr;
    if (method_json.contains("parameters")) {
      request_type_name = absl::StrCat(method_name, "Request");
      auto const& request = request_types_.find(request_type_name);
      if (request == request_types_.end()) {
        return internal::InvalidArgumentError(absl::StrFormat(
            "Cannot find request_type_name=%s in type_map", request_type_name));
      }
      request_type = request->second;
    }

    std::string response_type_name = "google.protobuf.Empty";
    auto const& response = method_json.find("response");
    if (response != method_json.end()) {
      std::string ref = response->value("$ref", "");
      if (!ref.empty()) {
        auto const& response = response_types_.find(ref);
        if (response == response_types_.end()) {
          return internal::InvalidArgumentError(absl::StrFormat(
              "Cannot find response_type_name=%s in type_map", ref));
        }
        if (response->second->package_name() != package_name_) {
          response_type_name = absl::StrCat(response->second->package_name(),
                                            ".", response->second->name());
        } else {
          response_type_name = response->second->name();
        }
      }
    }

    std::string method_description = method_json.value("description", "");
    if (!method_description.empty()) {
      rpc_text.push_back(FormatCommentBlock(method_description, 1));
    }
    auto constexpr kMethodLinkComments =
        R"""(https://cloud.google.com/$product_name$/docs/reference/rest/$version$/%s/%s)""";
    rpc_text.push_back(FormatCommentBlock(
        absl::StrFormat(kMethodLinkComments, name_, iter.key()), 1));
    rpc_text.push_back(absl::StrFormat("  rpc %s(%s) returns (%s) {",
                                       method_name, request_type_name,
                                       response_type_name));
    auto rpc_options =
        FormatRpcOptions(method_json, document_properties.base_path,
                         document_properties.operation_services, request_type);
    if (!rpc_options) return std::move(rpc_options).status();
    rpc_text.push_back(*std::move(rpc_options));
    rpc_text.emplace_back("  }");
    rpcs_text.push_back(absl::StrJoin(rpc_text, "\n"));
  }

  return absl::StrCat(
      FormatCommentBlock(absl::StrFormat(kServiceComments, name_, name_), 0),
      absl::StrJoin(service_text, "\n"), absl::StrJoin(rpcs_text, "\n\n"),
      "\n}\n");
}

std::string DiscoveryResource::DebugString() const {
  return absl::StrCat("name: ", name_, "; package_name: ", package_name_,
                      "; json_: ", json_.dump());
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
