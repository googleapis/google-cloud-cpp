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
#include <future>
#include <map>
#include <thread>
#include <utility>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

namespace rest = google::cloud::rest_internal;

auto constexpr kCommonPackageNameFormat = "google.cloud.cpp.%s.%s";
auto constexpr kResourcePackageNameFormat = "google.cloud.cpp.%s.%s.%s";

google::cloud::StatusOr<std::string> GetPage(std::string const& url) {
  std::pair<std::string, std::string> url_pieces =
      absl::StrSplit(url, absl::ByString("com/"));
  auto client = rest::MakeDefaultRestClient(url_pieces.first + "com", {});
  rest::RestRequest request;
  request.SetPath(url_pieces.second);
  rest_internal::RestContext context;
  auto response = client->Get(context, request);
  if (!response) {
    return std::move(response).status();
  }

  auto payload = std::move(**response).ExtractPayload();
  return rest::ReadAll(std::move(payload));
}

}  // namespace

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

StatusOr<std::map<std::string, DiscoveryTypeVertex>> ExtractTypesFromSchema(
    DiscoveryDocumentProperties const& document_properties,
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
    types.emplace(id, DiscoveryTypeVertex{
                          id,
                          absl::StrFormat(kCommonPackageNameFormat,
                                          document_properties.product_name,
                                          document_properties.version),
                          s});
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
    DiscoveryDocumentProperties const& document_properties,
    nlohmann::json const& discovery_doc) {
  if (!discovery_doc.contains("resources")) return {};
  std::map<std::string, DiscoveryResource> resources;
  auto const& resources_json = discovery_doc.find("resources");
  for (auto r = resources_json->begin(); r != resources_json->end(); ++r) {
    std::string resource_name = r.key();
    resources.emplace(
        resource_name,
        DiscoveryResource{
            resource_name,
            absl::StrFormat(kResourcePackageNameFormat,
                            document_properties.product_name, resource_name,
                            document_properties.version),
            r.value()});
  }
  return resources;
}

// The DiscoveryResource& parameter will be used later to help determine what
// protobuf files need to be imported to provide the response message.
StatusOr<DiscoveryTypeVertex const*> DetermineAndVerifyResponseType(
    nlohmann::json const& method_json, DiscoveryResource&,
    std::map<std::string, DiscoveryTypeVertex>& types) {
  std::string response_type_name;
  DiscoveryTypeVertex const* response_type = nullptr;
  auto const& response_iter = method_json.find("response");
  if (response_iter != method_json.end()) {
    auto const& ref_iter = response_iter->find("$ref");
    if (ref_iter == response_iter->end()) {
      return internal::InvalidArgumentError("Missing $ref field in response");
    }
    response_type_name = *ref_iter;
    auto const& iter = types.find(response_type_name);
    if (iter == types.end()) {
      return internal::InvalidArgumentError(absl::StrFormat(
          "Response name=%s not found in types", response_type_name));
    }
    response_type = &iter->second;
  }
  return response_type;
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

  return DiscoveryTypeVertex(id, resource.package_name(), synthesized_request);
}

Status ProcessMethodRequestsAndResponses(
    std::map<std::string, DiscoveryResource>& resources,
    std::map<std::string, DiscoveryTypeVertex>& types) {
  for (auto& resource : resources) {
    std::string resource_name = CapitalizeFirstLetter(resource.first);
    auto const& methods = resource.second.json().find("methods");
    for (auto m = methods->begin(); m != methods->end(); ++m) {
      auto response_type =
          DetermineAndVerifyResponseType(*m, resource.second, types);
      if (!response_type) {
        return Status(response_type.status().code(),
                      response_type.status().message(),
                      GCP_ERROR_INFO()
                          .WithMetadata("resource", resource.first)
                          .WithMetadata("method", m.key())
                          .Build(response_type.status().code()));
      }
      std::string response_type_name;
      if (*response_type != nullptr) {
        response_type_name = (*response_type)->name();
        DiscoveryTypeVertex const* node = *response_type;
        resource.second.AddResponseType(response_type_name, node);
      } else {
        resource.second.AddEmptyResponseType();
      }

      if (m->contains("parameters")) {
        auto request_type = SynthesizeRequestType(*m, resource.second,
                                                  response_type_name, m.key());
        if (!request_type) return std::move(request_type).status();
        // It is necessary to add the resource name to the map key to
        // disambiguate methods that appear in more than one resource.
        std::string id = request_type->name();
        auto insert_result = types.insert(std::make_pair(
            absl::StrCat(resource_name, ".", id), *std::move(request_type)));
        if (!insert_result.second) {
          return internal::InternalError(
              absl::StrCat("Unable to insert type ", resource_name, ".", id));
        }
        resource.second.AddRequestType(id, &insert_result.first->second);
      } else {
        resource.second.AddEmptyRequestType();
      }
    }
  }

  return {};
}

std::vector<DiscoveryFile> CreateFilesFromResources(
    std::map<std::string, DiscoveryResource> const& resources,
    DiscoveryDocumentProperties const& document_properties,
    std::string const& output_path) {
  std::vector<DiscoveryFile> files;
  files.reserve(resources.size());
  for (auto const& r : resources) {
    DiscoveryFile f{
        &r.second,
        r.second.FormatFilePath(document_properties.product_name,
                                document_properties.version, output_path),
        r.second.package_name(), r.second.GetRequestTypesList()};
    f.AddImportPath("google/api/annotations.proto")
        .AddImportPath("google/api/client.proto")
        .AddImportPath("google/api/field_behavior.proto")
        .AddImportPath(
            "google/cloud/$product_name$/$version$/"
            "internal/common.proto");

    if (r.second.RequiresEmptyImport()) {
      f.AddImportPath("google/protobuf/empty.proto");
    }
    if (r.second.RequiresLROImport()) {
      f.AddImportPath("google/cloud/extended_operations.proto");
    }

    files.push_back(std::move(f));
  }
  return files;
}

std::vector<DiscoveryFile> AssignResourcesAndTypesToFiles(
    std::map<std::string, DiscoveryResource> const& resources,
    std::map<std::string, DiscoveryTypeVertex> const& types,
    DiscoveryDocumentProperties const& document_properties,
    std::string const& output_path) {
  auto files =
      CreateFilesFromResources(resources, document_properties, output_path);

  // TODO(#11190): For the first phase of implementation, we create one proto
  // per resource and its request types. For the remainder of the types, we
  // dump them all into one internal/common.proto file. This should be reworked
  // to split the common types into multiple files and inject the corresponding
  // import statements where needed.
  std::vector<DiscoveryTypeVertex const*> common_types;
  for (auto const& t : types) {
    if (!t.second.IsSynthesizedRequestType()) {
      common_types.push_back(&t.second);
    }
  }

  files.emplace_back(
      nullptr,
      absl::StrCat(output_path,
                   absl::StrFormat("/google/cloud/%s/%s/internal/common.proto",
                                   document_properties.product_name,
                                   document_properties.version)),
      absl::StrFormat(kCommonPackageNameFormat,
                      document_properties.product_name,
                      document_properties.version),
      std::move(common_types));
  return files;
}

StatusOr<std::string> DefaultHostFromRootUrl(nlohmann::json const& json) {
  std::string root_url = json.value("rootUrl", "");
  if (root_url.empty()) return root_url;
  absl::string_view host = root_url;
  if (!absl::ConsumePrefix(&host, "https://")) {
    return internal::InvalidArgumentError(
        absl::StrCat("rootUrl field in unexpected format: ", host));
  }
  absl::ConsumeSuffix(&host, "/");
  return std::string(host);
}

Status GenerateProtosFromDiscoveryDoc(nlohmann::json const& discovery_doc,
                                      std::string const&, std::string const&,
                                      std::string const& output_path) {
  auto default_hostname =
      google::cloud::generator_internal::DefaultHostFromRootUrl(discovery_doc);
  if (!default_hostname) return std::move(default_hostname).status();

  DiscoveryDocumentProperties document_properties{
      discovery_doc.value("basePath", ""), *default_hostname,
      discovery_doc.value("name", ""), discovery_doc.value("version", "")};

  if (document_properties.base_path.empty() ||
      document_properties.default_hostname.empty() ||
      document_properties.product_name.empty() ||
      document_properties.version.empty()) {
    return internal::InvalidArgumentError(
        "Missing one or more document properties",
        GCP_ERROR_INFO()
            .WithMetadata("basePath", document_properties.base_path)
            .WithMetadata("rootUrl", document_properties.default_hostname)
            .WithMetadata("name", document_properties.product_name)
            .WithMetadata("version", document_properties.version));
  }

  auto types = ExtractTypesFromSchema(document_properties, discovery_doc);
  if (!types) return std::move(types).status();

  auto resources = ExtractResources(document_properties, discovery_doc);
  if (resources.empty()) {
    return internal::InvalidArgumentError(
        "No resources found in Discovery Document.");
  }

  auto method_status = ProcessMethodRequestsAndResponses(resources, *types);
  if (!method_status.ok()) return method_status;

  std::vector<DiscoveryFile> files = AssignResourcesAndTypesToFiles(
      resources, *types, document_properties, output_path);

  std::vector<std::future<google::cloud::Status>> tasks;
  tasks.reserve(files.size());
  for (auto f : files) {
    tasks.push_back(std::async(
        std::launch::async,
        [](DiscoveryFile const& f,
           DiscoveryDocumentProperties const& document_properties,
           std::map<std::string, DiscoveryTypeVertex> const& types) {
          return f.WriteFile(document_properties, types);
        },
        std::move(f), document_properties, *types));
  }

  bool file_write_error = false;
  for (auto& t : tasks) {
    auto result = t.get();
    if (!result.ok()) {
      GCP_LOG(ERROR) << result;
      file_write_error = true;
    }
  }

  if (file_write_error) {
    return internal::InternalError(
        "Error encountered writing file. Check log for additional details.");
  }

  return {};
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
