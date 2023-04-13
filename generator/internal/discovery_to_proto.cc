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

#include "generator/internal/discovery_to_proto.h"
#include "generator/internal/codegen_utils.h"
#include "generator/internal/discovery_resource.h"
#include "generator/internal/discovery_type_vertex.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <utility>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

namespace rest = google::cloud::rest_internal;

google::cloud::StatusOr<std::string> GetPage(std::string const& url) {
  std::pair<std::string, std::string> url_pieces =
      absl::StrSplit(url, absl::ByString("com/"));
  auto client = rest::MakeDefaultRestClient(url_pieces.first + "com", {});
  rest::RestRequest request;
  request.SetPath(url_pieces.second);
  auto response = client->Get(request);
  if (!response) {
    return std::move(response).status();
  }

  auto payload = std::move(**response).ExtractPayload();
  return rest::ReadAll(std::move(payload));
}

StatusOr<nlohmann::json> GetDiscoveryDoc(std::string const& url) {
  nlohmann::json parsed_discovery_doc;
  if (absl::StartsWith(url, "file://")) {
    auto file_path = std::string(absl::StripPrefix(url, "file://"));
    std::ifstream json_file(file_path);
    if (!json_file.is_open()) {
      return internal::InvalidArgumentError(
          absl::StrCat("Unable to open file ", file_path));
    }
    parsed_discovery_doc = nlohmann::json::parse(json_file, nullptr, false);
  } else {
    auto page = GetPage(url);
    if (!page) return std::move(page).status();
    parsed_discovery_doc = nlohmann::json::parse(*page, nullptr, false);
  }

  if (!parsed_discovery_doc.is_object()) {
    return internal::InvalidArgumentError("Error parsing Discovery Doc");
  }

  return parsed_discovery_doc;
}

}  // namespace

StatusOr<std::map<std::string, DiscoveryTypeVertex>> ExtractTypesFromSchema(
    nlohmann::json const& discovery_doc) {
  std::map<std::string, DiscoveryTypeVertex> types;
  if (!discovery_doc.contains("schemas")) {
    return internal::InvalidArgumentError(
        "Discovery Document does not contain schemas element.");
  }

  auto const& schemas = discovery_doc["schemas"];
  bool schemas_all_type_object = true;
  bool schemas_all_have_id = true;
  std::string id;
  for (auto const& s : schemas) {
    if (!s.contains("id") || s["id"].is_null()) {
      GCP_LOG(ERROR) << "current schema has no id. last schema with id="
                     << (id.empty() ? "(none)" : id);
      schemas_all_have_id = false;
      continue;
    }
    id = s["id"];
    std::string type = s.value("type", "untyped");
    if (type != "object") {
      GCP_LOG(ERROR) << id << " not type:object; is instead " << type;
      schemas_all_type_object = false;
      continue;
    }
    types.emplace(id, DiscoveryTypeVertex{id, s});
  }

  if (!schemas_all_have_id) {
    return internal::InvalidArgumentError(
        "Discovery Document contains schema without id field.");
  }

  if (!schemas_all_type_object) {
    return internal::InvalidArgumentError(
        "Discovery Document contains non object schema.");
  }

  return types;
}

std::map<std::string, DiscoveryResource> ExtractResources(
    nlohmann::json const& discovery_doc, std::string const& default_hostname,
    std::string const& base_path) {
  if (!discovery_doc.contains("resources")) return {};
  std::map<std::string, DiscoveryResource> resources;
  auto const& resources_json = discovery_doc.find("resources");
  for (auto r = resources_json->begin(); r != resources_json->end(); ++r) {
    resources.emplace(r.key(), DiscoveryResource{r.key(), default_hostname,
                                                 base_path, r.value()});
  }
  return resources;
}

// The DiscoveryResource& parameter will be used later to help determine what
// protobuf files need to be imported to provide the response message.
StatusOr<std::string> DetermineAndVerifyResponseTypeName(
    nlohmann::json const& method_json, DiscoveryResource&,
    std::map<std::string, DiscoveryTypeVertex>& types) {
  std::string response_type_name;
  auto const& response_iter = method_json.find("response");
  if (response_iter != method_json.end()) {
    auto const& ref_iter = response_iter->find("$ref");
    if (ref_iter == response_iter->end()) {
      return internal::InvalidArgumentError("Missing $ref field in response");
    }
    response_type_name = *ref_iter;
    auto const& response_type = types.find(response_type_name);
    if (response_type == types.end()) {
      return internal::InvalidArgumentError(absl::StrFormat(
          "Response name=%s not found in types", response_type_name));
    }
  }
  return response_type_name;
}

StatusOr<DiscoveryTypeVertex> SynthesizeRequestType(
    nlohmann::json const& method_json, DiscoveryResource const& resource,
    std::string const& response_type_name, std::string method_name) {
  if (!method_json.contains("parameters")) {
    return internal::InternalError(
        "method_json does not contain parameters field",
        GCP_ERROR_INFO()
            .WithMetadata("resource", resource.name())
            .WithMetadata("method", method_name)
            .WithMetadata("json", method_json.dump()));
  }
  std::vector<std::string> const operation_request_fields = {"project", "zone",
                                                             "region"};
  nlohmann::json synthesized_request;
  synthesized_request["type"] = "object";
  synthesized_request["synthesized_request"] = true;
  synthesized_request["resource"] = resource.name();
  synthesized_request["method"] = method_name;
  method_name = resource.FormatMethodName(method_name);
  std::string id = absl::StrCat(method_name, "Request");
  synthesized_request["id"] = id;
  synthesized_request["description"] =
      absl::StrFormat("Request message for %s.", method_name);

  // add method parameters as properties to new type
  auto const& params = method_json.find("parameters");
  for (auto p = params->begin(); p != params->end(); ++p) {
    std::string const& param_name = p.key();
    synthesized_request["properties"][param_name] = *p;
    if (response_type_name == "Operation" &&
        internal::Contains(operation_request_fields, param_name)) {
      synthesized_request["properties"][param_name]["operation_request_field"] =
          true;
    }
  }

  // if present add request object
  auto const& request_iter = method_json.find("request");
  if (request_iter != method_json.end()) {
    auto const& ref_iter = request_iter->find("$ref");
    if (ref_iter == request_iter->end()) {
      return internal::InvalidArgumentError(
          absl::StrFormat("resource %s has method %s with non $ref request",
                          resource.name(), method_name),
          GCP_ERROR_INFO().WithMetadata("json", method_json.dump()));
    }
    std::string request_resource_field_name =
        CamelCaseToSnakeCase(std::string(*ref_iter));
    if (!absl::EndsWith(request_resource_field_name, "_resource")) {
      absl::StrAppend(&request_resource_field_name, "_resource");
    }
    synthesized_request["request_resource_field_name"] =
        request_resource_field_name;
    synthesized_request["properties"][request_resource_field_name] =
        method_json["request"];
    synthesized_request["properties"][request_resource_field_name]
                       ["description"] = absl::StrFormat(
                           "The %s for this request.",
                           std::string((method_json["request"]["$ref"])));
  }

  return DiscoveryTypeVertex(id, synthesized_request);
}

Status GenerateProtosFromDiscoveryDoc(std::string const& url,
                                      std::string const&, std::string const&,
                                      std::string const&) {
  auto discovery_doc = GetDiscoveryDoc(url);
  if (!discovery_doc) return discovery_doc.status();

  // TODO(10980): Finish implementing this function.
  return internal::UnimplementedError("Not implemented.");
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
