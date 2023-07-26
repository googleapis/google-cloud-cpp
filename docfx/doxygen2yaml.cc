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

#include "docfx/doxygen2yaml.h"
#include "docfx/doxygen2children.h"
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen2references.h"
#include "docfx/doxygen2syntax.h"
#include "docfx/doxygen_errors.h"
#include "docfx/function_classifiers.h"
#include "docfx/node_name.h"
#include "docfx/public_docs.h"
#include "docfx/yaml_emit.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string_view>

namespace docfx {
namespace {

auto kind(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()};
}

bool IgnoreForRecurse(pugi::xml_node node) {
  static auto const* const kNames = [] {
    return new std::set<std::string>{
        // Handled by each AppendIf*() function
        "compoundname",
        "briefdescription",
        "detaileddescription",
        "description",
        "includes",
        "location",
        "templateparamlist",
        // TODO(#10895) - should be a cross-reference
        "innerclass",
        "innernamespace",
        "listofallmembers",
        // TODO(#10895) - maybe include base and derived classes?
        "basecompoundref",
        "derivedcompoundref",
        // Ignored, we will not include inheritance diagrams in DocFX YAML
        "inheritancegraph",
        "collaborationgraph",
        // TODO(#10895) - maybe include in ToC.
        //   This is a title for a sectionref (a "group" of member functions),
        //   maybe we can add this to break down each compound ToC.
        "header",
    };
  }();
  auto const name = std::string{node.name()};
  return kNames->find(name) != kNames->end();
}

void CompoundRecurse(YAML::Emitter& yaml, YamlContext const& ctx,
                     pugi::xml_node node) {
  for (auto const child : node) {
    if (!IncludeInPublicDocuments(ctx.config, child)) continue;
    if (IgnoreForRecurse(child)) continue;
    // Enums need to get their own files, so never recurse in them:
    if (kind(child) == "enum") continue;
    if (AppendIfSectionDef(yaml, ctx, child)) continue;
    if (AppendIfNamespace(yaml, ctx, child)) continue;
    if (AppendIfClass(yaml, ctx, child)) continue;
    if (AppendIfStruct(yaml, ctx, child)) continue;
    if (AppendIfEnumValue(yaml, ctx, child)) continue;
    if (AppendIfTypedef(yaml, ctx, child)) continue;
    if (AppendIfFriend(yaml, ctx, child)) continue;
    if (AppendIfVariable(yaml, ctx, child)) continue;
    if (AppendIfFunction(yaml, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
}

std::string Summary(YamlContext const& ctx, pugi::xml_node node) {
  std::ostringstream os;
  MarkdownContext mdctx;
  mdctx.paragraph_start = "";
  auto brief = node.child("briefdescription");
  if (!brief.children().empty()) {
    AppendIfBriefDescription(os, mdctx, brief);
  } else {
    os << ctx.fallback_description_brief;
  }
  return std::move(os).str();
}

std::string Conceptual(YamlContext const& ctx, pugi::xml_node node,
                       bool skip_xrefsect = false) {
  std::ostringstream os;
  MarkdownContext mdctx;
  mdctx.paragraph_start = "";
  mdctx.skip_xrefsect = skip_xrefsect;
  auto description = node.child("description");
  if (!description.children().empty()) {
    AppendDescriptionType(os, mdctx, description);
    mdctx = MarkdownContext{};
  }
  auto detailed = node.child("detaileddescription");
  if (!detailed.children().empty()) {
    AppendIfDetailedDescription(os, mdctx, detailed);
  }
  if (description.children().empty() && detailed.children().empty()) {
    os << ctx.fallback_description_detailed;
  }
  return std::move(os).str();
}

void AppendDescription(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node) {
  auto const summary = Summary(ctx, node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  auto const conceptual = Conceptual(ctx, node);
  if (!conceptual.empty()) {
    yaml << YAML::Key << "conceptual" << YAML::Value << YAML::Literal
         << conceptual;
  }
}

}  // namespace

std::string Compound2Yaml(Config const& cfg, pugi::xml_node node) {
  YAML::Emitter yaml;
  YamlContext ctx;
  ctx.config = cfg;
  yaml << YAML::BeginMap << YAML::Key << "items" << YAML::Value
       << YAML::BeginSeq;
  (void)AppendIfEnum(yaml, ctx, node);
  (void)AppendIfTypedef(yaml, ctx, node);
  (void)AppendIfFriend(yaml, ctx, node);
  (void)AppendIfVariable(yaml, ctx, node);
  (void)AppendIfFunction(yaml, ctx, node);
  (void)AppendIfNamespace(yaml, ctx, node);
  (void)AppendIfClass(yaml, ctx, node);
  (void)AppendIfStruct(yaml, ctx, node);
  yaml << YAML::EndSeq;

  yaml << YAML::Key << "references" << YAML::Value
       << ExtractReferences(ctx, node);
  yaml << YAML::EndMap;
  return EndDocFxYaml(yaml);
}

std::string EndDocFxYaml(YAML::Emitter& yaml) {
  auto result = std::string{"### YamlMime:UniversalReference\n"};
  result += yaml.c_str();
  result += "\n";
  return result;
}

bool AppendIfEnumValue(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node) {
  if (std::string_view{node.name()} != "enumvalue") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  yaml << YAML::BeginMap                                            //
       << YAML::Key << "uid" << YAML::Value << id                   //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted  //
       << NodeName(node)                                            //
       << YAML::Key << "id" << YAML::Value << id                    //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id     //
       << YAML::Key << "type" << YAML::Value << "enumvalue"         //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;
  AppendDescription(yaml, ctx, node);
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfEnum(YAML::Emitter& yaml, YamlContext const& ctx,
                  pugi::xml_node node) {
  if (kind(node) != "enum") return false;
  if (node.attribute("id").empty()) MissingAttribute(__func__, "id", node);
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << full_name                         //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "enum"                      //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendEnumSyntax(yaml, ctx, node);
  AppendDescription(yaml, ctx, node);
  auto const children = Children(ctx, node);
  if (!children.empty()) {
    yaml << YAML::Key << "children" << YAML::Value << children;
  }
  yaml << YAML::EndMap;
  auto nested = ctx;
  nested.parent_id = id;
  for (auto const child : node) {
    if (AppendIfEnumValue(yaml, nested, child)) continue;
  }
  return true;
}

bool AppendIfTypedef(YAML::Emitter& yaml, YamlContext const& ctx,
                     pugi::xml_node node) {
  if (kind(node) != "typedef") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::DoubleQuoted << full_name                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "typealias"                 //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendTypedefSyntax(yaml, ctx, node);
  AppendDescription(yaml, ctx, node);
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfFriend(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node node) {
  if (kind(node) != "friend") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << full_name                         //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "friend"                    //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendFriendSyntax(yaml, ctx, node);
  AppendDescription(yaml, ctx, node);
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfVariable(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node node) {
  if (kind(node) != "variable") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const qualified_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << qualified_name                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "variable"                  //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendVariableSyntax(yaml, ctx, node);
  AppendDescription(yaml, ctx, node);
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfFunction(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node node) {
  if (kind(node) != "function") return false;
  auto const name = std::string{node.child("name").child_value()};
  if (name == "MOCK_METHOD") return true;

  auto const it = ctx.mocking_functions.find(name);
  auto const is_mocked = it != ctx.mocking_functions.end();
  auto const id = is_mocked ? std::string{it->second}
                            : std::string{node.attribute("id").as_string()};
  auto const qualified_name =
      std::string_view{node.child("qualifiedname").child_value()};

  auto type = std::string_view{"function"};
  if (IsOperator(node)) {
    type = "operator";
  } else if (IsConstructor(node)) {
    type = "constructor";
  }
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << qualified_name                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << type                        //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendFunctionSyntax(yaml, ctx, node);
  auto const summary = Summary(ctx, node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  auto conceptual = Conceptual(ctx, node);
  if (!conceptual.empty() || is_mocked) {
    auto constexpr kMockedSummary =
        R"md(This function is implemented using [gMock]'s `MOCK_METHOD()`.
Consult the gMock documentation to use this mock in your tests.

[gMock]: https://google.github.io/googletest)md";
    auto const full = [&]() mutable {
      if (!is_mocked) return conceptual;
      return conceptual.empty()
                 ? std::string(kMockedSummary)
                 : (conceptual + "\n\n" + std::string{kMockedSummary});
    }();
    yaml << YAML::Key << "conceptual" << YAML::Value << YAML::Literal << full;
  }
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfSectionDef(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node node) {
  if (std::string_view{node.name()} != "sectiondef") return false;
  auto nested = ctx;
  nested.fallback_description_brief = Summary(ctx, node);
  if (nested.fallback_description_brief.empty()) {
    nested.fallback_description_brief = node.child_value("header");
  }
  nested.fallback_description_detailed = Conceptual(ctx, node);
  CompoundRecurse(yaml, nested, node);
  return true;
}

bool AppendIfNamespace(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node) {
  if (kind(node) != "namespace") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "namespace"                 //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendNamespaceSyntax(yaml, ctx, node);
  // Deprecated namespaces need special treatment
  auto const summary = Summary(ctx, node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  auto conceptual = Conceptual(ctx, node, true);
  // Discover all the `xrefsect` descendants that document this is a deprecated
  // namespace and list the alternatives.
  std::map<std::string, std::string> deprecated;
  for (auto const i :
       node.child("detaileddescription").select_nodes(".//xrefsect")) {
    auto const title = std::string_view{i.node().child_value("xreftitle")};
    if (title != "Deprecated") continue;
    auto xrefdescription = i.node().child("xrefdescription");
    for (auto const j : xrefdescription.select_nodes(".//ref")) {
      auto ref = j.node();
      deprecated.emplace(ref.child_value(), ref.attribute("refid").as_string());
    }
  }
  if (!deprecated.empty()) {
    std::ostringstream os;
    os << R"""(

<aside class="deprecated">
    <b>Deprecated:</b> This namespace is deprecated, prefer the types defined in)""";
    auto sep = std::string_view{" "};
    for (auto const& [name, uid] : deprecated) {
      os << sep << "[`" << name << "`](xref:" << uid << ")";
      sep = ", or ";
    }
    os << ".\n</aside>";
    conceptual.append(std::move(os.str()));
  }
  if (!conceptual.empty()) {
    yaml << YAML::Key << "conceptual" << YAML::Value << YAML::Literal
         << conceptual;
  }

  auto const children = Children(ctx, node);
  if (!children.empty()) {
    yaml << YAML::Key << "children" << YAML::Value << children;
  }
  yaml << YAML::EndMap;
  CompoundRecurse(yaml, NestedYamlContext(ctx, node), node);
  return true;
}

bool AppendIfClass(YAML::Emitter& yaml, YamlContext const& ctx,
                   pugi::xml_node node) {
  if (kind(node) != "class") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "class"                     //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendClassSyntax(yaml, ctx, node);
  AppendDescription(yaml, ctx, node);
  auto const children = Children(ctx, node);
  if (!children.empty()) {
    yaml << YAML::Key << "children" << YAML::Value << children;
  }
  yaml << YAML::EndMap;
  CompoundRecurse(yaml, NestedYamlContext(ctx, node), node);
  return true;
}

bool AppendIfStruct(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node node) {
  if (kind(node) != "struct") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted          //
       << NodeName(node)                                                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "struct"                    //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendStructSyntax(yaml, ctx, node);
  AppendDescription(yaml, ctx, node);
  auto const children = Children(ctx, node);
  if (!children.empty()) {
    yaml << YAML::Key << "children" << YAML::Value << children;
  }
  yaml << YAML::EndMap;
  CompoundRecurse(yaml, NestedYamlContext(ctx, node), node);
  return true;
}

}  // namespace docfx
