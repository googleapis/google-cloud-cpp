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

#include "generator/internal/discovery_type_vertex.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

class DiscoveryResource {
 public:
  DiscoveryResource();
  DiscoveryResource(std::string name, std::string default_host,
                    std::string base_path, nlohmann::json json);

  nlohmann::json const& json() const { return json_; }

  void AddRequestType(std::string name, DiscoveryTypeVertex const* type);

  // Examines the provided path and converts any parameter names in curly braces
  // to snake case, e.g. "projects/{projectId}/zone/{zone}" yields
  // "projects/{project_id}/zone/{zone}".
  // It is the caller's responsibility to ensure curly braces exist in pairs.
  static std::string FormatUrlPath(std::string const& path);

  // Examines the method JSON to determine the google.api.http,
  // google.api.method_signature, and google.cloud.operation_service options.
  StatusOr<std::string> FormatRpcOptions(
      nlohmann::json const& method_json,
      DiscoveryTypeVertex const& request_type) const;

  // Summarize all the scopes found in the resource methods for inclusion as
  // a service level google.api.oauth_scopes option.
  StatusOr<std::string> FormatOAuthScopes() const;

  // Package names for service proto definitions are created in the format:
  // "google.cloud.cpp.${product_name}.${resource_name}.${version}"
  std::string FormatPackageName(std::string const& product_name,
                                std::string const& version) const;

  // File paths for service protos are formatted thusly:
  // "${output_path}/google/cloud/${product_name}/${resource_name}/${version}/${resource_name}.proto"
  std::string FormatFilePath(std::string const& product_name,
                             std::string const& version,
                             std::string const& output_path) const;

  // Method names are formatted as is, except for primitive method names which
  // are concatenated with the resource name.
  std::string FormatMethodName(std::string method_name) const;

  StatusOr<std::string> JsonToProtobufService() const;

 private:
  std::string name_;
  std::string default_host_;
  std::string base_path_;
  nlohmann::json json_;
  std::map<std::string, DiscoveryTypeVertex const*> request_types_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_RESOURCE_H
