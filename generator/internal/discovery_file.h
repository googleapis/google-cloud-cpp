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

#include "generator/internal/discovery_resource.h"
#include "generator/internal/discovery_type_vertex.h"
#include "google/cloud/status.h"
#include <nlohmann/json.hpp>
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
  DiscoveryFile(DiscoveryResource const* resource, std::string file_path,
                std::string package_name, std::string version,
                std::vector<DiscoveryTypeVertex const*> types);

  void AddImportPath(std::string import_path) {
    import_paths_.insert(std::move(import_path));
  }

  // Writes the file to output_stream.
  Status FormatFile(std::string const& product_name,
                    std::ostream& output_stream) const;

  // Creates necessary directories and writes the file to disk.
  Status WriteFile(std::string const& product_name) const;

 private:
  DiscoveryResource const* resource_;
  std::string file_path_;
  std::string package_name_;
  std::string version_;
  std::set<std::string> import_paths_;
  std::vector<DiscoveryTypeVertex const*> types_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_FILE_H
