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

#include "docfx/doxygen2children.h"
#include "docfx/public_docs.h"
#include "docfx/yaml_emit.h"
#include <iterator>
#include <string_view>

namespace docfx {

std::vector<std::string> Children(YamlContext const& ctx, pugi::xml_node node) {
  std::vector<std::string> children;
  auto const nested = NestedYamlContext(ctx, node);
  for (auto const child : node.children("sectiondef")) {
    if (!IncludeInPublicDocuments(nested.config, child)) continue;
    auto more = Children(nested, child);
    children.insert(children.end(), std::make_move_iterator(more.begin()),
                    std::make_move_iterator(more.end()));
  }
  // Skip the <innernamespace> elements. All namespaces appear in the ToC
  // (the left-side navigation). Repeating them as children renders incorrectly.
  // We could fix that, but we do not have enough namespaces to make this
  // worthwhile.
  for (auto const child : node.children("innerclass")) {
    if (!IncludeInPublicDocuments(nested.config, child)) continue;
    auto const refid = std::string_view{child.attribute("refid").as_string()};
    if (!refid.empty()) children.emplace_back(refid);
  }
  for (auto const child : node.children("memberdef")) {
    if (!IncludeInPublicDocuments(nested.config, child)) continue;
    if (IsSkippedChild(nested, child)) continue;
    auto id = std::string{child.attribute("id").as_string()};
    if (!id.empty()) children.push_back(std::move(id));
  }
  for (auto const child : node.children("enumvalue")) {
    if (!IncludeInPublicDocuments(nested.config, child)) continue;
    auto const id = std::string_view{child.attribute("id").as_string()};
    if (!id.empty()) children.emplace_back(id);
  }
  return children;
}

}  // namespace docfx
