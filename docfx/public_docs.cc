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

auto kind(pugi::xml_node const& node) {
  return std::string_view{node.attribute("kind").as_string()};
}

}  // namespace

bool IncludeInPublicDocuments(pugi::xml_node const& node) {
  // We do not generate documents for files and directories.
  if (kind(node) == "file" || kind(node) == "dir") return false;
  // Doxygen groups private attributes / functions in <sectiondef> elements of
  // these kinds:
  if (kind(node) == "private-attrib" || kind(node) == "private-func") {
    return false;
  }
  // We do not generate documents for types in the C++ `std::` namespace:
  auto const id = std::string_view{node.attribute("id").as_string()};
  for (std::string_view prefix : {"namespacestd", "classstd", "structstd"}) {
    if (id.substr(0, prefix.size()) == prefix) return false;
  }
  // We do not generate documentation for private members or sections.
  auto const prot = std::string_view{node.attribute("prot").as_string()};
  return prot != "private";
}

}  // namespace docfx
