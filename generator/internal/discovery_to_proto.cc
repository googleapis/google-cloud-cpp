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
#include "generator/internal/discovery_proto_export_file.h"
#include "generator/internal/discovery_resource.h"
#include "generator/internal/discovery_type_vertex.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
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

bool IsDiscoveryArrayType(nlohmann::json const& json) {
  return json.contains("type") && json["type"] == "array" &&
         json.contains("items");
}

bool IsDiscoveryMapType(nlohmann::json const& json) {
  return json.contains("type") && json["type"] == "object" &&
         json.contains("additionalProperties");
}

bool IsDiscoveryNestedType(nlohmann::json const& json) {
  return json.contains("type") && json["type"] == "object" &&
         json.contains("properties");
}

// NOLINTNEXTLINE(misc-no-recursion)
void ApplyResourceLabelsToTypesHelper(std::string const& resource_name,
                                      DiscoveryTypeVertex& type) {
  type.AddNeededByResource(resource_name);
  auto deps = type.needs_type();
  for (auto* dep : deps) {
    if (!internal::Contains(dep->needed_by_resource(), resource_name)) {
      ApplyResourceLabelsToTypesHelper(resource_name, *dep);
    }
  }
}

std::string FormatFileResourceKey(std::set<std::string> const& resources) {
  return absl::StrJoin(resources, ":");
}

void AddImportToFile(std::map<std::string, DiscoveryFile> const& common_files,
                     DiscoveryTypeVertex const& type, DiscoveryFile& file) {
  auto iter =
      common_files.find(FormatFileResourceKey(type.needed_by_resource()));
  if (iter != common_files.end()) {
    if (file.relative_proto_path() != iter->second.relative_proto_path()) {
      file.AddImportPath(iter->second.relative_proto_path());
    }
  }
}

StatusOr<std::string> GetImportForProtobufType(
    std::string const& protobuf_type) {
  static auto const* const kProtobufTypeImports =
      new std::unordered_map<std::string, std::string>{
          {"google.protobuf.Any", "google/protobuf/any.proto"}};

  auto iter = kProtobufTypeImports->find(protobuf_type);
  if (iter == kProtobufTypeImports->end()) {
    return internal::InvalidArgumentError(
        absl::StrCat("Unrecognized protobuf type: ", protobuf_type),
        GCP_ERROR_INFO());
  }
  return iter->second;
}

}  // namespace

StatusOr<std::map<std::string, DiscoveryTypeVertex>> ExtractTypesFromSchema(
    DiscoveryDocumentProperties const& document_properties,
    nlohmann::json const& discovery_doc,
    google::protobuf::DescriptorPool const* descriptor_pool) {
  std::map<std::string, DiscoveryTypeVertex> types;
  if (!discovery_doc.contains("schemas")) {
    return internal::InvalidArgumentError(
        "Discovery Document does not contain schemas element.");
  }

  auto const& schemas = discovery_doc["schemas"];
  std::vector<std::string> recognized_types = {"object", "any"};
  bool schemas_all_recognized_types = true;
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
    if (!internal::Contains(recognized_types, type)) {
      GCP_LOG(ERROR) << id << " type is not in `recognized_types`; is instead "
                     << type;
      schemas_all_recognized_types = false;
      continue;
    }
    types.emplace(id, DiscoveryTypeVertex{
                          id,
                          absl::StrFormat(kCommonPackageNameFormat,
                                          document_properties.product_name,
                                          document_properties.version),
                          s, descriptor_pool});
  }

  if (!schemas_all_have_id) {
    return internal::InvalidArgumentError(
        "Discovery Document contains schema without id field.");
  }

  if (!schemas_all_recognized_types) {
    return internal::InvalidArgumentError(
        "Discovery Document contains unrecognized schema type.");
  }

  return types;
}

StatusOr<std::map<std::string, DiscoveryResource>> ExtractResources(
    DiscoveryDocumentProperties const& document_properties,
    nlohmann::json const& discovery_doc) {
  if (!discovery_doc.contains("resources") ||
      discovery_doc.find("resources")->empty()) {
    return internal::InvalidArgumentError(
        "No resources found in Discovery Document.");
  }
  std::map<std::string, DiscoveryResource> resources;
  auto const& resources_json = discovery_doc.find("resources");
  for (auto r = resources_json->begin(); r != resources_json->end(); ++r) {
    std::string resource_name = r.key();
    auto iter = resources.emplace(
        resource_name,
        DiscoveryResource{resource_name,
                          absl::StrFormat(kResourcePackageNameFormat,
                                          CamelCaseToSnakeCase(
                                              document_properties.product_name),
                                          CamelCaseToSnakeCase(resource_name),
                                          document_properties.version),
                          r.value()});
    auto verify_service_api_version = iter.first->second.SetServiceApiVersion();
    if (!verify_service_api_version.ok()) {
      return verify_service_api_version;
    }
  }

  return resources;
}

// The DiscoveryResource& parameter will be used later to help determine what
// protobuf files need to be imported to provide the response message.
StatusOr<DiscoveryTypeVertex*> DetermineAndVerifyResponseType(
    nlohmann::json const& method_json, DiscoveryResource&,
    std::map<std::string, DiscoveryTypeVertex>& types) {
  std::string response_type_name;
  DiscoveryTypeVertex* response_type = nullptr;
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
    std::string const& response_type_name, std::string method_name,
    google::protobuf::DescriptorPool const* descriptor_pool) {
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
                       ["is_resource"] = true;
    synthesized_request["properties"][request_resource_field_name]
                       ["description"] = absl::StrFormat(
                           "The %s for this request.",
                           std::string((method_json["request"]["$ref"])));
  }

  return DiscoveryTypeVertex(id, resource.package_name(), synthesized_request,
                             descriptor_pool);
}

Status ProcessMethodRequestsAndResponses(
    std::map<std::string, DiscoveryResource>& resources,
    std::map<std::string, DiscoveryTypeVertex>& types,
    google::protobuf::DescriptorPool const* descriptor_pool) {
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
        resource.second.AddResponseType(response_type_name, *response_type);
      } else {
        resource.second.AddEmptyResponseType();
      }

      if (m->contains("parameters")) {
        auto request_type = SynthesizeRequestType(
            *m, resource.second, response_type_name, m.key(), descriptor_pool);
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

// NOLINTNEXTLINE(misc-no-recursion)
std::set<std::string> FindAllTypesToImport(nlohmann::json const& json) {
  std::set<std::string> types_to_import;
  nlohmann::json fields;
  if (json.contains("properties")) {
    fields = json["properties"];
  } else if (json.contains("additionalProperties") || json.contains("items")) {
    fields = json;
  }

  for (auto const& f : fields) {
    if (f.contains("type")) {
      if (f["type"] == "any") {
        types_to_import.insert("google.protobuf.Any");
      }
    }

    if (f.contains("$ref")) {
      types_to_import.insert(f["$ref"]);
    }

    if (IsDiscoveryArrayType(f) || IsDiscoveryMapType(f) ||
        IsDiscoveryNestedType(f)) {
      auto new_ref_values = FindAllTypesToImport(f);
      types_to_import.insert(new_ref_values.begin(), new_ref_values.end());
    }
  }

  return types_to_import;
}

Status EstablishTypeDependencies(
    std::map<std::string, DiscoveryTypeVertex>& types) {
  for (auto& type : types) {
    auto const& json = type.second.json();
    auto ref_values = FindAllTypesToImport(json);
    for (auto const& ref : ref_values) {
      if (absl::StartsWith(ref, "google.protobuf.")) {
        type.second.AddNeedsProtobufType(ref);
      } else {
        auto ref_iter = types.find(ref);
        if (ref_iter == types.end()) {
          return internal::InvalidArgumentError(
              absl::StrCat("Unknown depended upon type: ", ref),
              GCP_ERROR_INFO()
                  .WithMetadata("dependent type", type.first)
                  .WithMetadata("depended upon type", ref));
        }
        type.second.AddNeedsType(&ref_iter->second);
        ref_iter->second.AddNeededByType(&type.second);
      }
    }
  }

  return {};
}

void ApplyResourceLabelsToTypes(
    std::map<std::string, DiscoveryResource>& resources) {
  for (auto& resource : resources) {
    for (auto const& request_type : resource.second.request_types()) {
      ApplyResourceLabelsToTypesHelper(resource.first, *request_type.second);
    }
    for (auto const& response_type : resource.second.response_types()) {
      ApplyResourceLabelsToTypesHelper(resource.first, *response_type.second);
    }
  }
}

std::vector<DiscoveryFile> CreateFilesFromResources(
    std::map<std::string, DiscoveryResource> const& resources,
    DiscoveryDocumentProperties const& document_properties,
    std::string const& output_path,
    std::map<std::string, DiscoveryFile> const& common_files) {
  std::vector<DiscoveryFile> files;
  files.reserve(resources.size());
  for (auto const& r : resources) {
    auto f =
        DiscoveryFile{
            &r.second,
            r.second.FormatFilePath(document_properties.product_name,
                                    document_properties.version, output_path),
            r.second.FormatFilePath(document_properties.product_name,
                                    document_properties.version, {}),
            r.second.package_name(), r.second.GetRequestTypesList()}
            .AddImportPath("google/api/annotations.proto")
            .AddImportPath("google/api/client.proto")
            .AddImportPath("google/api/field_behavior.proto");
    if (r.second.RequiresEmptyImport()) {
      f.AddImportPath("google/protobuf/empty.proto");
    }
    if (r.second.RequiresLROImport()) {
      f.AddImportPath("google/cloud/extended_operations.proto");
    }

    for (auto const& request : r.second.request_types()) {
      for (auto const& needs : request.second->needs_type()) {
        AddImportToFile(common_files, *needs, f);
      }
    }
    for (auto const& response : r.second.response_types()) {
      AddImportToFile(common_files, *response.second, f);
    }
    files.push_back(std::move(f));
  }
  return files;
}

StatusOr<std::pair<std::vector<DiscoveryFile>,
                   std::vector<DiscoveryProtoExportFile>>>
AssignResourcesAndTypesToFiles(
    std::map<std::string, DiscoveryResource> const& resources,
    std::map<std::string, DiscoveryTypeVertex>& types,
    DiscoveryDocumentProperties const& document_properties,
    std::string const& output_path, std::string const& export_output_path) {
  std::map<std::string, DiscoveryFile> common_files_by_resource;
  int i = 0;
  for (auto& kv : types) {
    if (!kv.second.IsSynthesizedRequestType()) {
      auto* type = &kv.second;
      std::string resource_key =
          FormatFileResourceKey(type->needed_by_resource());
      auto iter = common_files_by_resource.find(resource_key);
      if (iter != common_files_by_resource.end()) {
        iter->second.AddType(type);
      } else {
        auto relative_proto_path = absl::StrFormat(
            "google/cloud/%s/%s/internal/common_%03d.proto",
            document_properties.product_name, document_properties.version, i++);
        common_files_by_resource.emplace(
            resource_key,
            DiscoveryFile(nullptr,
                          absl::StrCat(output_path, "/", relative_proto_path),
                          relative_proto_path,
                          absl::StrFormat(kCommonPackageNameFormat,
                                          document_properties.product_name,
                                          document_properties.version),
                          {})
                .AddType(type));
      }
    }
  }

  for (auto& kv : common_files_by_resource) {
    auto& file = kv.second;
    for (auto* type : file.types()) {
      for (auto* needed_type : type->needs_type()) {
        std::string resource_key =
            FormatFileResourceKey(needed_type->needed_by_resource());
        auto iter = common_files_by_resource.find(resource_key);
        if (iter == common_files_by_resource.end()) {
          return internal::InvalidArgumentError(
              absl::StrCat("Unable to find resource_key: ", resource_key),
              GCP_ERROR_INFO()
                  .WithMetadata("resource_key", resource_key)
                  .WithMetadata("needed_type", needed_type->name())
                  .WithMetadata("type", type->name())
                  .WithMetadata("proto_file", file.relative_proto_path()));
        }
        auto needed_file_relative_proto_path =
            iter->second.relative_proto_path();
        if (file.relative_proto_path() != needed_file_relative_proto_path) {
          file.AddImportPath(std::move(needed_file_relative_proto_path));
        }
      }
      for (auto const& protobuf_type : type->needs_protobuf_type()) {
        auto protobuf_type_import = GetImportForProtobufType(protobuf_type);
        if (!protobuf_type_import) {
          return std::move(protobuf_type_import).status();
        }
        file.AddImportPath(*protobuf_type_import);
      }
    }
  }

  auto files = CreateFilesFromResources(resources, document_properties,
                                        output_path, common_files_by_resource);

  std::transform(common_files_by_resource.begin(),
                 common_files_by_resource.end(), std::back_inserter(files),
                 [](auto p) { return p.second; });

  std::vector<DiscoveryProtoExportFile> export_files;
  for (auto const& r : resources) {
    auto proto_export_path = absl::StrFormat(
        "google/cloud/%s/%s/%s/%s_proto_export.h",
        document_properties.product_name, CamelCaseToSnakeCase(r.first),
        document_properties.version, CamelCaseToSnakeCase(r.first));
    std::set<std::string> includes;
    for (auto const& f : common_files_by_resource) {
      if (absl::StrContains(f.first, r.first)) {
        includes.insert(f.second.relative_proto_path());
      }
    }
    export_files.emplace_back(
        absl::StrCat(export_output_path, "/", proto_export_path),
        proto_export_path, std::move(includes));
  }

  return std::make_pair(std::move(files), std::move(export_files));
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
    // TODO(sdhart): figure out if we can get more error info from nhlomann as
    // to why the parsing failed and put it into ErrorInfo.
    return internal::InvalidArgumentError("Error parsing Discovery Doc");
  }

  return parsed_discovery_doc;
}

Status GenerateProtosFromDiscoveryDoc(
    nlohmann::json const& discovery_doc, std::string const& discovery_doc_url,
    std::string const& protobuf_proto_path,
    std::string const& googleapis_proto_path, std::string const& output_path,
    std::string const& export_output_path,
    bool enable_parallel_write_for_discovery_protos,
    std::set<std::string> operation_services) {
  auto default_hostname = DefaultHostFromRootUrl(discovery_doc);
  if (!default_hostname) return std::move(default_hostname).status();

  DiscoveryDocumentProperties document_properties{
      discovery_doc.value("basePath", ""), *default_hostname,
      discovery_doc.value("name", ""),     discovery_doc.value("version", ""),
      discovery_doc.value("revision", ""), discovery_doc_url,
      std::move(operation_services),       CurrentCopyrightYear()};

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

  google::protobuf::compiler::DiskSourceTree protobuf_proto_files;
  protobuf_proto_files.MapPath("", protobuf_proto_path);
  google::protobuf::compiler::DiskSourceTree googleapis_proto_files;
  googleapis_proto_files.MapPath("", googleapis_proto_path);
  google::protobuf::compiler::DiskSourceTree compute_proto_files;
  compute_proto_files.MapPath("", output_path);
  google::protobuf::compiler::SourceTreeDescriptorDatabase protobuf_proto_db(
      &protobuf_proto_files);
  google::protobuf::compiler::SourceTreeDescriptorDatabase googleapis_proto_db(
      &googleapis_proto_files);
  google::protobuf::compiler::SourceTreeDescriptorDatabase compute_proto_db(
      &compute_proto_files);
  google::protobuf::MergedDescriptorDatabase merged_db(
      {&protobuf_proto_db, &googleapis_proto_db, &compute_proto_db});
  google::protobuf::DescriptorPool descriptor_pool(&merged_db);

  auto types = ExtractTypesFromSchema(document_properties, discovery_doc,
                                      &descriptor_pool);
  if (!types) return std::move(types).status();

  auto resources = ExtractResources(document_properties, discovery_doc);
  if (!resources.ok()) return std::move(resources).status();

  auto method_status =
      ProcessMethodRequestsAndResponses(*resources, *types, &descriptor_pool);
  if (!method_status.ok()) return method_status;

  EstablishTypeDependencies(*types);
  ApplyResourceLabelsToTypes(*resources);
  auto files = AssignResourcesAndTypesToFiles(
      *resources, *types, document_properties, output_path, export_output_path);
  if (!files) return std::move(files).status();

  // google::protobuf::DescriptorPool lazily initializes itself. Searching for
  // types by name will fail if the descriptor has not yet been created. By
  // finding all the files we intend to write, the DescriptorPool builds its
  // collection of descriptors for that file and any it imports. We must
  // perform this mutation of the DescriptorPool before we begin the threaded
  // write process. Additionally, populating the DescriptorPool allows us to
  // snapshot the existing proto files before we overwrite them in place.
  for (auto const& f : files->first) {
    (void)descriptor_pool.FindFileByName(f.relative_proto_path());
  }

  if (enable_parallel_write_for_discovery_protos) {
    std::vector<std::future<google::cloud::Status>> tasks;
    tasks.reserve(files->first.size());
    for (auto f : files->first) {
      tasks.push_back(std::async(
          std::launch::async,
          [](DiscoveryFile const& f,
             DiscoveryDocumentProperties const& document_properties,
             std::map<std::string, DiscoveryTypeVertex> const& types) {
            return f.WriteFile(document_properties, types);
          },
          std::move(f), document_properties, *types));
    }

    for (auto f : files->second) {
      tasks.push_back(std::async(
          std::launch::async,
          [](DiscoveryProtoExportFile const& f) { return f.WriteFile(); },
          std::move(f)));
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
          "Error encountered writing file. Check log for additional "
          "details.");
    }
  } else {
    for (auto const& f : files->first) {
      auto s = f.WriteFile(document_properties, *types);
      if (!s.ok()) return s;
    }

    for (auto const& f : files->second) {
      auto s = f.WriteFile();
      if (!s.ok()) return s;
    }
  }

  return {};
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
