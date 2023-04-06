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

#include "generator/internal/discovery_file.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "absl/strings/str_format.h"
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <fstream>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif  // _WIN32

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

void MakeDirectory(std::string const& path) {
#if _WIN32
  _mkdir(path.c_str());
#else
  mkdir(path.c_str(), 0755);
#endif  // _WIN32
}

}  // namespace

DiscoveryFile::DiscoveryFile(DiscoveryResource const* resource,
                             std::string file_path, std::string package_name,
                             std::string version,
                             std::vector<DiscoveryTypeVertex const*> types)
    : resource_(resource),
      file_path_(std::move(file_path)),
      package_name_(std::move(package_name)),
      version_(std::move(version)),
      types_(std::move(types)) {}

Status DiscoveryFile::FormatFile(std::string const& product_name,
                                 std::ostream& output_stream) const {
  std::map<std::string, std::string> const vars = {
      {"copyright_year", CurrentCopyrightYear()},
      {"package_name", package_name_},
      {"version", version_},
      {"product_name", product_name},
      {"resource_name", (resource_ ? resource_->name() : "")}};
  google::protobuf::io::OstreamOutputStream output(&output_stream);
  google::protobuf::io::Printer printer(&output, '$');
  printer.Print(vars, CopyrightLicenseFileHeader().c_str());
  printer.Print(vars, R"""(
syntax = "proto3";

package $package_name$;

)""");

  if (!import_paths_.empty()) {
    printer.Print(vars, absl::StrJoin(import_paths_, "\n",
                                      [](std::string* s, std::string const& p) {
                                        *s += absl::StrFormat("import \"%s\";",
                                                              p);
                                      })
                            .c_str());
    printer.Print("\n\n");
  }

  if (resource_) {
    auto service_definition = resource_->JsonToProtobufService();
    if (!service_definition) {
      return std::move(service_definition).status();
    }
    printer.Print(vars, std::move(service_definition)->c_str());
    if (!types_.empty()) printer.Print("\n");
  }

  std::vector<std::string> formatted_types;
  for (auto const& t : types_) {
    auto message = t->JsonToProtobufMessage();
    if (!message) return message.status();
    formatted_types.push_back(*std::move(message));
  }

  if (!formatted_types.empty()) {
    printer.Print(vars, absl::StrJoin(formatted_types, "\n").c_str());
  }

  return {};
}

Status DiscoveryFile::WriteFile(std::string const& product_name) const {
  std::string version_dir_path = file_path_.substr(0, file_path_.rfind('/'));
  std::string service_dir_path =
      version_dir_path.substr(0, version_dir_path.rfind('/'));
  MakeDirectory(service_dir_path);
  MakeDirectory(version_dir_path);
  std::ofstream os(file_path_);
  return FormatFile(product_name, os);
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
