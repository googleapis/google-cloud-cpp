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

#include "docfx/yaml_context.h"
#include "docfx/public_docs.h"
#include <iostream>
#include <vector>

namespace docfx {
namespace {

// We need to build a little data structure to deal with mocks. The member
// functions of a mock appear twice. First Doxygen thinks there is a
// MOCK_METHOD()
std::unordered_map<std::string, std::string> MockedFunctions(
    Config const& config, pugi::xml_node const& node) {
  std::unordered_map<std::string, std::string> mocked;
  for (auto const& child : node.children("sectiondef")) {
    if (!IncludeInPublicDocuments(config, node)) continue;
    auto more = MockedFunctions(config, child);
    mocked.insert(more.begin(), more.end());
  }
  for (auto const& child : node.children("memberdef")) {
    auto const id = std::string_view{child.attribute("id").as_string()};
    auto const kind = std::string_view{child.attribute("kind").as_string()};
    if (id.empty() || kind != "function") continue;
    auto const member_name = std::string_view{child.child_value("name")};
    if (member_name != "MOCK_METHOD") continue;
    // Find the name of the mocked method.
    auto const children = [&] {
      auto children = child.children("param");
      return std::vector<pugi::xml_node>{children.begin(), children.end()};
    }();
    if (children.size() < 2) continue;
    // children[0] is the return type, children[1] is the name of the function.
    auto param_type = children[1].child("type").child("ref");
    auto const func_name = std::string{param_type.child_value()};
    if (func_name.empty()) continue;
    mocked.emplace(func_name, std::string{id});
  }
  return mocked;
}

std::unordered_set<std::string> MockedIds(
    std::unordered_map<std::string, std::string> const& mocked_functions,
    Config const& config, pugi::xml_node const& node) {
  std::unordered_set<std::string> mocked;
  for (auto const& child : node.children("sectiondef")) {
    if (!IncludeInPublicDocuments(config, node)) continue;
    auto more = MockedIds(mocked_functions, config, child);
    mocked.insert(more.begin(), more.end());
  }
  for (auto const& child : node.children("memberdef")) {
    auto const id = std::string_view{child.attribute("id").as_string()};
    auto const kind = std::string_view{child.attribute("kind").as_string()};
    if (id.empty() || kind != "function") continue;

    auto const member_name = std::string{child.child_value("name")};
    auto const it = mocked_functions.find(member_name);
    if (it == mocked_functions.end()) continue;
    mocked.emplace(id);
  }
  return mocked;
}

}  // namespace

YamlContext NestedYamlContext(YamlContext const& ctx,
                              pugi::xml_node const& node) {
  auto const id = std::string{node.attribute("id").as_string()};
  auto nested = ctx;
  nested.parent_id = id;
  nested.mocked_functions = MockedFunctions(ctx.config, node);
  nested.mocked_ids = MockedIds(nested.mocked_functions, ctx.config, node);
  nested.mocked_functions_by_id.clear();
  for (auto const& [name, uid] : nested.mocked_functions) {
    nested.mocked_functions_by_id[uid] = name;
  }
  return nested;
}

}  // namespace docfx
