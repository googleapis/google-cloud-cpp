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

#include "docfx/doxygen2references.h"
#include "docfx/node_name.h"
#include "docfx/public_docs.h"
#include "docfx/yaml_emit.h"
#include <iterator>

namespace docfx {
namespace {

std::list<Reference> RecurseReferences(YamlContext const& ctx,
                                       pugi::xml_node node) {
  std::list<Reference> references;
  for (auto const& c : node) {
    references.splice(references.end(), ExtractReferences(ctx, c));
  }
  return references;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, Reference const& rhs) {
  return os << "Reference{uid=" << rhs.uid << ", name=" << rhs.name << "}";
}

YAML::Emitter& operator<<(YAML::Emitter& yaml, Reference const& rhs) {
  return yaml << YAML::BeginMap                                  //
              << YAML::Key << "uid" << YAML::Value << rhs.uid    //
              << YAML::Key << "name" << YAML::Value << rhs.name  //
              << YAML::EndMap;
}

// Generate the `references` element in a DocFX YAML.
std::list<Reference> ExtractReferences(YamlContext const& ctx,
                                       pugi::xml_node node) {
  if (!IncludeInPublicDocuments(ctx.config, node)) return {};

  auto const name = std::string_view{node.name()};
  // Skip <innernamespace> elements. They are listed in ToC (the left-side
  // navigation).
  if (name == "innernamespace") return {};
  if (name == "innerclass") {
    auto const uid = std::string{node.attribute("refid").as_string()};
    return {Reference(uid, node.child_value())};
  }
  if (name == "enumvalue") {
    auto const uid = std::string_view{node.attribute("id").as_string()};
    auto const name = std::string_view{node.child("name").child_value()};
    return {Reference(uid, name)};
  }
  if (name == "sectiondef") return RecurseReferences(ctx, node);
  if (name == "compounddef") {
    auto nested = NestedYamlContext(ctx, node);
    auto const uid = std::string_view{node.attribute("id").as_string()};
    auto recurse = RecurseReferences(nested, node);
    recurse.emplace_front(uid, node.child_value("compoundname"));
    return recurse;
  }
  if (name == "memberdef") {
    if (IsSkippedChild(ctx, node)) return {};
    auto const uid = std::string{node.attribute("id").as_string()};
    if (ctx.mocked_ids.count(uid) != 0) return {};
    auto const name = [&] {
      auto qname = std::string_view{node.child("qualifiedname").child_value()};
      auto it = ctx.mocking_functions_by_id.find(uid);
      if (it == ctx.mocking_functions_by_id.end()) {
        return std::string{qname};
      }
      auto const p = qname.find("::MOCK_METHOD");
      if (p == std::string_view::npos) {
        throw std::invalid_argument(
            "Mocked functions should contain ::MOCK_METHOD. uid=" + uid);
      }
      return std::string{qname.substr(0, p)} + "::" + it->second;
    }();

    auto recurse = RecurseReferences(ctx, node);
    recurse.emplace_front(std::move(uid), name);
    return recurse;
  }
  return RecurseReferences(ctx, node);
}

}  // namespace docfx
