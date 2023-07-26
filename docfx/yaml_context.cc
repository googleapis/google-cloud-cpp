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
// functions of a mock appear twice.
// 1. Doxygen thinks there is a function called MOCK_METHOD(), this is the
//    function referenced from other classes or documents.  It has an `id` that
//    connects it to the mock class (as in `class...MockFoo_${method_hash}`).
// 2. Doxygen also creates an entry for the inherited (mocked) function. This
//    has the arguments, return type and so on, but its id duplicates the
//    id of the function in the base class.
//
// What we do is use the information from the inherited function (from 2) and
// give it the `uid` from the `MOCK_METHOD()` (from 1).
std::unordered_map<std::string, std::string> MockingFunctions(
    Config const& config, pugi::xml_node node) {
  std::unordered_map<std::string, std::string> mocked;
  for (auto const child : node.children("sectiondef")) {
    if (!IncludeInPublicDocuments(config, node)) continue;
    auto more = MockingFunctions(config, child);
    mocked.insert(more.begin(), more.end());
  }
  for (auto const child : node.children("memberdef")) {
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

std::unordered_map<std::string, std::string> MockedIds(
    std::unordered_map<std::string, std::string> const& mocked_functions,
    Config const& config, pugi::xml_node node) {
  std::unordered_map<std::string, std::string> mocked;
  for (auto const child : node.children("sectiondef")) {
    if (!IncludeInPublicDocuments(config, node)) continue;
    auto more = MockedIds(mocked_functions, config, child);
    mocked.insert(more.begin(), more.end());
  }
  for (auto const child : node.children("memberdef")) {
    auto const id = std::string_view{child.attribute("id").as_string()};
    auto const kind = std::string_view{child.attribute("kind").as_string()};
    if (id.empty() || kind != "function") continue;

    auto const member_name = std::string{child.child_value("name")};
    auto const it = mocked_functions.find(member_name);
    if (it == mocked_functions.end()) continue;
    mocked.emplace(id, it->second);
  }
  return mocked;
}

}  // namespace

YamlContext NestedYamlContext(YamlContext const& ctx, pugi::xml_node node) {
  auto const id = std::string{node.attribute("id").as_string()};
  auto nested = ctx;
  nested.parent_id = id;
  nested.mocking_functions = MockingFunctions(ctx.config, node);
  nested.mocking_functions_by_id.clear();
  for (auto const& [name, uid] : nested.mocking_functions) {
    nested.mocking_functions_by_id[uid] = name;
  }
  nested.mocked_ids = MockedIds(nested.mocking_functions, ctx.config, node);
  return nested;
}

bool IsSkippedChild(YamlContext const& ctx, pugi::xml_node node) {
  // Mocked functions are not children.
  auto id = std::string{node.attribute("id").as_string()};
  if (ctx.mocked_ids.count(id) != 0) return true;

  // Things that are not MOCK_METHOD() are always present.
  auto qname = std::string_view{node.child("qualifiedname").child_value()};
  auto const p = qname.find("::MOCK_METHOD");
  if (p == std::string_view::npos) return false;

  // In a few places we kept a MOCK_METHOD() definition for a function that
  // does not exist in the base class. These only exist for backwards
  // compatibility. Skip them as there is no need to document those.
  auto const m = ctx.mocking_functions_by_id.find(id);
  return m == ctx.mocking_functions_by_id.end();
}

pugi::xml_node MockingNode(YamlContext const& ctx, pugi::xml_node node) {
  auto const id = std::string{node.attribute("id").as_string()};
  auto const m = ctx.mocked_ids.find(id);
  if (m == ctx.mocked_ids.end()) return node;
  pugi::xpath_variable_set vars;
  vars.add("id", pugi::xpath_type_string);
  vars.set("id", m->second.c_str());
  auto found = node.select_node(
      pugi::xpath_query("//memberdef[@id = string($id)]", &vars));
  if (!found) return node;
  return found.node();
}

}  // namespace docfx
