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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_PROTO_EXPORT_FILE_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_PROTO_EXPORT_FILE_H

#include "google/cloud/status.h"
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

// Creates a IWYU pragma export file of the internal protos needed by a service.
class DiscoveryProtoExportFile {
 public:
  DiscoveryProtoExportFile(std::string output_file_path,
                           std::string relative_file_path,
                           std::set<std::string> proto_includes,
                           std::string compatibility_output_file_path = {},
                           std::string compatibility_relative_file_path = {});

  std::string const& output_file_path() const { return output_file_path_; }
  std::string const& relative_file_path() const { return relative_file_path_; }
  std::set<std::string> const& proto_includes() const {
    return proto_includes_;
  }

  // Writes the file to output_stream.
  Status FormatFile(std::ostream& output_stream) const;

  // Writes the file to output_stream.
  Status FormatCompatibilityFile(std::ostream& output_stream) const;

  // Creates necessary directories and writes the file to disk.
  Status WriteFile() const;

 private:
  std::string output_file_path_;
  std::string relative_file_path_;
  std::set<std::string> proto_includes_;
  std::string compatibility_output_file_path_;
  std::string compatibility_relative_file_path_;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_PROTO_EXPORT_FILE_H
