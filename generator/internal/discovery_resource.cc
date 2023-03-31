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

DiscoveryResource::DiscoveryResource() : json_("") {}

DiscoveryResource::DiscoveryResource(std::string name, std::string default_host,
                                     std::string base_path, nlohmann::json json)
    : name_(std::move(name)),
      default_host_(std::move(default_host)),
      base_path_(std::move(base_path)),
      json_(std::move(json)) {}

StatusOr<std::string> DiscoveryResource::FormatRpcOptions(
    nlohmann::json const& method_json,
    DiscoveryTypeVertex const& request_type) const {
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
      verb, absl::StrCat(absl::StripSuffix(base_path_, "/"), "/", path));
  std::string request_resource_field_name;
  if (verb == "post" || verb == "patch" || verb == "put") {
    std::string http_body =
        request_type.json().value("request_resource_field_name", "*");
    if (http_body != "*") request_resource_field_name = http_body;
    absl::StrAppend(&http_option,
                    absl::StrFormat("      body: \"%s\"\n", http_body));
  }
  rpc_options.push_back(absl::StrCat(http_option, "    };"));

  // Defining long running operations in Discovery Documents relies upon
  // conventions. This implements the convention used by compute. It may be
  // that we need to introduce additional in the future if we come across other
  // LRO defining conventions.
  // https://cloud.google.com/compute/docs/regions-zones/global-regional-zonal-resources
  std::string operation_scope;
  std::vector<std::string> params =
      method_json.value("parameterOrder", std::vector<std::string>{});
  if (!params.empty()) {
    if (internal::Contains(params, "zone")) {
      operation_scope = "ZoneOperations";
    } else if (internal::Contains(params, "region")) {
      operation_scope = "RegionOperations";
    }
    if (!request_resource_field_name.empty()) {
      params.push_back(request_resource_field_name);
    }
    rpc_options.push_back(
        absl::StrFormat("    option (google.api.method_signature) = \"%s\";",
                        absl::StrJoin(params, ",")));
  }

  if (method_json.contains("response") &&
      method_json["response"].value("$ref", "") == "Operation") {
    if (absl::StrContains(path, "/global/")) {
      operation_scope = "GlobalOperations";
    }
    if (operation_scope.empty()) {
      return internal::InvalidArgumentError(
          "Method response is Operation but no scope is defined.",
          GCP_ERROR_INFO().WithMetadata("json", method_json.dump()));
    }
    rpc_options.push_back(
        absl::StrFormat("    option (google.cloud.operation_service) = \"%s\";",
                        operation_scope));
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

std::string DiscoveryResource::FormatPackageName(
    std::string const& product_name, std::string const& version) const {
  return absl::StrJoin(
      std::make_tuple(std::string("google.cloud.cpp"), product_name,
                      CamelCaseToSnakeCase(name_), version),
      ".");
}

std::string DiscoveryResource::FormatFilePath(
    std::string const& product_name, std::string const& version,
    std::string const& output_path) const {
  return absl::StrJoin({output_path, std::string("google/cloud"), product_name,
                        CamelCaseToSnakeCase(name_), version,
                        absl::StrCat(CamelCaseToSnakeCase(name_), ".proto")},
                       "/");
}

// TODO(#11138): Add logic to keep resource name plural for aggregatedList and
// list, but change resource name to singular for remaining primitive methods.
std::string DiscoveryResource::FormatMethodName(std::string method_name) const {
  constexpr char const* kPrimitives[] = {
      "AggregatedList", "Delete", "Get", "Insert", "List", "Patch", "Update"};
  method_name = CapitalizeFirstLetter(method_name);
  if (internal::Contains(kPrimitives, method_name)) {
    return absl::StrCat(method_name, CapitalizeFirstLetter(name_));
  }
  return method_name;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
