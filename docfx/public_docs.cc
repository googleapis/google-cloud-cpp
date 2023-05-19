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

#include "docfx/public_docs.h"

namespace docfx {
namespace {

auto kind(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()};
}

}  // namespace

bool IncludeInPublicDocuments(Config const& cfg, pugi::xml_node node) {
  // We do not generate documents for files and directories.
  if (kind(node) == "file" || kind(node) == "dir") return false;
  // Doxygen groups private attributes / functions in <sectiondef> elements of
  // these kinds:
  if (kind(node) == "private-attrib" || kind(node) == "private-func") {
    return false;
  }
  // We do not generate documents for types in the C++ `std::` namespace or the
  // `absl::` namespace.
  auto const id = std::string_view{node.attribute("id").as_string()};
  for (std::string_view prefix : {"namespacestd", "classstd", "structstd",
                                  "namespaceabsl", "classabsl", "structabsl"}) {
    if (id.substr(0, prefix.size()) == prefix) return false;
  }
  // Doxygen generates a page listing all deprecated symbols. It does not seem
  // to add enough value (each symbol already says if it is deprecated), and
  // we need more work to render this correctly in the DocFX format.
  if (kind(node) == "page" && id == "deprecated") return false;
  // Don't include the top-level `::google` namespace. This is shared with
  // Protobuf and other libraries, we should not be including it in our
  // documentation.
  if (id == "namespacegoogle") return false;
  // Unless this is the 'cloud' library, we do not generate the
  // `google::cloud::` namespace.
  if (cfg.library != "cloud" && id == "namespacegoogle_1_1cloud") {
    return false;
  }
  // Skip destructors in the public documents. There is rarely something
  // interesting to say about them, and we would need to create a completely
  // new organization
  if (kind(node) == "function") {
    auto const name = std::string_view{node.child_value("name")};
    if (name[0] == '~') return false;
  }
  // We do not generate documentation for private members or sections.
  auto const prot = std::string_view{node.attribute("prot").as_string()};
  return prot != "private";
}

}  // namespace docfx
