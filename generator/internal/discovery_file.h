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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_FILE_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_FILE_H

#include "generator/internal/discovery_document.h"
#include "generator/internal/discovery_resource.h"
#include "generator/internal/discovery_type_vertex.h"
#include "google/cloud/status.h"
#include <set>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

// Contains a resource and/or types that are to be written to a protobuf file.
class DiscoveryFile {
 public:
  DiscoveryFile() = default;

  // Set resources == nullptr to indicate the file only contains messages.
  DiscoveryFile(DiscoveryResource const* resource, std::string file_path,
                std::string relative_proto_path, std::string package_name,
                std::vector<DiscoveryTypeVertex*> types);

  std::string const& file_path() const { return file_path_; }
  std::string const& relative_proto_path() const {
    return relative_proto_path_;
  }
  std::string const& package_name() const { return package_name_; }
  std::string resource_name() const {
    return (resource_ ? resource_->name() : "");
  }
  std::vector<DiscoveryTypeVertex*> const& types() const { return types_; }
  std::set<std::string> const& import_paths() const { return import_paths_; }

  DiscoveryFile& AddType(DiscoveryTypeVertex* type) {
    types_.push_back(type);
    return *this;
  }

  DiscoveryFile& AddImportPath(std::string import_path) {
    import_paths_.insert(std::move(import_path));
    return *this;
  }

  // Writes the file to output_stream.
  Status FormatFile(DiscoveryDocumentProperties const& document_properties,
                    std::map<std::string, DiscoveryTypeVertex> const& types,
                    std::ostream& output_stream) const;

  // Creates necessary directories and writes the file to disk.
  Status WriteFile(
      DiscoveryDocumentProperties const& document_properties,
      std::map<std::string, DiscoveryTypeVertex> const& types) const;

 private:
  DiscoveryResource const* resource_;
  std::string file_path_;
  std::string relative_proto_path_;
  std::string package_name_;
  std::set<std::string> import_paths_;
  std::vector<DiscoveryTypeVertex*> types_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_FILE_H
