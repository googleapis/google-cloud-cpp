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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_RESOURCE_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_RESOURCE_H

#include "generator/internal/discovery_document.h"
#include "generator/internal/discovery_type_vertex.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

class DiscoveryResource {
 public:
  DiscoveryResource();
  DiscoveryResource(std::string name, std::string package_name,
                    nlohmann::json json);

  std::string const& name() const { return name_; }
  std::string const& package_name() const { return package_name_; }
  nlohmann::json const& json() const { return json_; }
  std::map<std::string, DiscoveryTypeVertex*> const& request_types() const {
    return request_types_;
  }
  std::map<std::string, DiscoveryTypeVertex*> const& response_types() const {
    return response_types_;
  }

  bool RequiresEmptyImport() const { return has_empty_request_or_response_; }
  bool RequiresLROImport() const {
    return response_types_.find("Operation") != response_types_.end();
  }

  void AddRequestType(std::string name, DiscoveryTypeVertex* type);
  void AddEmptyRequestType() { has_empty_request_or_response_ = true; }

  void AddResponseType(std::string name, DiscoveryTypeVertex* type);
  void AddEmptyResponseType() { has_empty_request_or_response_ = true; }

  std::vector<DiscoveryTypeVertex*> GetRequestTypesList() const;

  // "apiVersion" is an optional field that can be specified per method in a
  // Discovery Document. These must all be equal to qualify a resource for proto
  // generation.
  StatusOr<std::string> GetServiceApiVersion() const;
  Status SetServiceApiVersion();

  // Examines the provided path and converts any parameter names in curly braces
  // to snake case, e.g. "projects/{projectId}/zone/{zone}" yields
  // "projects/{project_id}/zone/{zone}".
  // It is the caller's responsibility to ensure curly braces exist in pairs.
  static std::string FormatUrlPath(std::string const& path);

  // Examines the method JSON to determine the google.api.http,
  // google.api.method_signature, and google.cloud.operation_service options.
  StatusOr<std::string> FormatRpcOptions(
      nlohmann::json const& method_json, std::string const& base_path,
      std::set<std::string> const& operation_services,
      DiscoveryTypeVertex const* request_type) const;

  // Summarize all the scopes found in the resource methods for inclusion as
  // a service level google.api.oauth_scopes option.
  StatusOr<std::string> FormatOAuthScopes() const;

  // File paths for service protos are formatted thusly:
  // "${output_path}/google/cloud/${product_name}/${resource_name}/${version}/${resource_name}.proto"
  std::string FormatFilePath(std::string const& product_name,
                             std::string const& version,
                             std::string const& output_path) const;

  // Interrogates the resource json for the schema name of the response to its
  // get method. An empty string is returned if none can be found.
  std::string GetMethodResponseTypeName() const;

  // Method names are formatted as is, except for primitive method names which
  // are concatenated with the singular form of the resource name which is
  // determined via the schema name returned from the get method.
  std::string FormatMethodName(std::string method_name) const;

  StatusOr<std::string> JsonToProtobufService(
      DiscoveryDocumentProperties const& document_properties) const;

  std::string DebugString() const;

 private:
  std::string name_;
  std::string package_name_;
  bool has_empty_request_or_response_;
  nlohmann::json json_;
  std::map<std::string, DiscoveryTypeVertex*> request_types_;
  std::map<std::string, DiscoveryTypeVertex*> response_types_;
  absl::optional<StatusOr<std::string>> service_api_version_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_RESOURCE_H
