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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_DOCUMENT_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_DOCUMENT_H

#include <set>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

struct DiscoveryDocumentProperties {
  std::string base_path;
  std::string default_hostname;
  std::string product_name;
  std::string version;
  std::string revision;
  std::string discovery_doc_url;
  std::set<std::string> operation_services;
  std::string copyright_year;
};

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DISCOVERY_DOCUMENT_H
