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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2REFERENCES_H
#define GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2REFERENCES_H

#include "docfx/yaml_context.h"
#include <pugixml.hpp>
#include <yaml-cpp/yaml.h>
#include <list>
#include <string>
#include <string_view>
#include <utility>

namespace docfx {

/**
 * A single element in the DocFX YAML `references` section.
 *
 * Each DocFX YAML file must contain `references` section listing all the items
 * (think "classes" and "member functions") in the file. This is a flattened
 * version of the hierarchical C++ objects.
 *
 * @see https://dotnet.github.io/docfx/spec/metadata_format_spec.html#25-reference-section
 */
struct Reference {
  Reference() = default;
  Reference(std::string_view u, std::string_view n) : uid(u), name(n) {}

  std::string uid;
  std::string name;
};

inline bool operator==(Reference const& a, Reference const& b) {
  return std::tie(a.uid, a.name) == std::tie(b.uid, b.name);
}
inline bool operator!=(Reference const& a, Reference const& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& os, Reference const& rhs);
YAML::Emitter& operator<<(YAML::Emitter& yaml, Reference const& rhs);

// Generate the `references` element in a DocFX YAML.
std::list<Reference> ExtractReferences(YamlContext const& ctx,
                                       pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2REFERENCES_H
