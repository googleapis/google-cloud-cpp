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
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen2syntax.h"
#include "docfx/doxygen_errors.h"
#include "docfx/yaml_emit.h"
#include <sstream>
#include <string_view>

namespace docfx {
namespace {

auto kind(pugi::xml_node const& node) {
  return std::string_view{node.attribute("kind").as_string()};
}

bool IgnoreForRecurse(pugi::xml_node const& node) {
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
                     pugi::xml_node const& node) {
  for (auto const& child : node) {
    if (!IncludeInPublicDocuments(child)) continue;
    if (IgnoreForRecurse(child)) continue;
    if (AppendIfSectionDef(yaml, ctx, child)) continue;
    if (AppendIfNamespace(yaml, ctx, child)) continue;
    if (AppendIfClass(yaml, ctx, child)) continue;
    if (AppendIfStruct(yaml, ctx, child)) continue;
    if (AppendIfEnumValue(yaml, ctx, child)) continue;
    if (AppendIfEnum(yaml, ctx, child)) continue;
    if (AppendIfTypedef(yaml, ctx, child)) continue;
    if (AppendIfVariable(yaml, ctx, child)) continue;
    if (AppendIfFunction(yaml, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
}

YamlContext NestedYamlContext(YamlContext const& ctx,
                              pugi::xml_node const& node) {
  auto const id = std::string{node.attribute("id").as_string()};
  auto nested = ctx;
  nested.parent_id = id;
  return nested;
}

std::string Summary(pugi::xml_node const& node) {
  std::ostringstream os;
  MarkdownContext ctx;
  ctx.paragraph_start = "";
  auto brief = node.child("briefdescription");
  auto detailed = node.child("detaileddescription");
  if (!brief.empty()) {
    AppendIfBriefDescription(os, ctx, brief);
    ctx = MarkdownContext{};
  }
  if (!detailed.empty()) AppendIfDetailedDescription(os, ctx, detailed);
  return std::move(os).str();
}

}  // namespace

std::vector<TocEntry> CompoundToc(pugi::xml_document const& doc) {
  std::vector<TocEntry> result;
  // Insert only namespaces in the TOC. Other entities (functions, typedefs,
  // classes, structs) are always part of a namespace and will appear in the
  // references from them.
  for (auto const& i : doc.select_nodes("//compounddef[@kind='namespace']")) {
    auto const& node = i.node();
    if (!IncludeInPublicDocuments(node)) continue;
    auto const id = std::string{node.attribute("id").as_string()};
    result.push_back(TocEntry{id + ".yml", id});
  }
  return result;
}

std::string Compound2Yaml(pugi::xml_node const& node) {
  YAML::Emitter yaml;
  StartDocFxYaml(yaml);
  YamlContext ctx;
  (void)AppendIfEnum(yaml, ctx, node);
  (void)AppendIfTypedef(yaml, ctx, node);
  (void)AppendIfVariable(yaml, ctx, node);
  (void)AppendIfFunction(yaml, ctx, node);
  (void)AppendIfNamespace(yaml, ctx, node);
  (void)AppendIfClass(yaml, ctx, node);
  (void)AppendIfStruct(yaml, ctx, node);
  return EndDocFxYaml(yaml);
}

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

void StartDocFxYaml(YAML::Emitter& yaml) {
  yaml << YAML::BeginMap << YAML::Key << "items" << YAML::Value
       << YAML::BeginSeq;
}

std::string EndDocFxYaml(YAML::Emitter& yaml) {
  yaml << YAML::EndSeq << YAML::EndMap;
  auto result = std::string{"### YamlMime:UniversalReference\n"};
  result += yaml.c_str();
  result += "\n";
  return result;
}

bool AppendIfEnumValue(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "enumvalue") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const summary = Summary(node);

  yaml << YAML::BeginMap                                               //
       << YAML::Key << "uid" << YAML::Value << id                      //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name  //
       << YAML::Key << "id" << YAML::Value << id                       //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id        //
       << YAML::Key << "type" << YAML::Value << "enumvalue"            //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfEnum(YAML::Emitter& yaml, YamlContext const& ctx,
                  pugi::xml_node const& node) {
  if (kind(node) != "enum") return false;
  if (node.attribute("id").empty()) MissingAttribute(__func__, "id", node);
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name       //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << full_name                         //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "enum"                      //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendEnumSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  auto nested = ctx;
  nested.parent_id = id;
  for (auto const& child : node) {
    if (AppendIfEnumValue(yaml, nested, child)) continue;
  }
  return true;
}

bool AppendIfTypedef(YAML::Emitter& yaml, YamlContext const& ctx,
                     pugi::xml_node const& node) {
  if (kind(node) != "typedef") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name       //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << full_name                         //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "typedef"                   //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendTypedefSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfVariable(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node const& node) {
  if (kind(node) != "variable") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const qualified_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name       //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << qualified_name                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "variable"                  //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendVariableSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfFunction(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node const& node) {
  if (kind(node) != "function") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const qualified_name =
      std::string_view{node.child("qualifiedname").child_value()};
  auto const name = std::string_view{node.child("name").child_value()};

  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << name                        //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << qualified_name                    //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "function"                  //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendFunctionSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfSectionDef(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "sectiondef") return false;
  CompoundRecurse(yaml, ctx, node);
  return true;
}

bool AppendIfNamespace(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node const& node) {
  if (kind(node) != "namespace") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("compoundname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << name                        //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "namespace"                 //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendNamespaceSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  CompoundRecurse(yaml, NestedYamlContext(ctx, node), node);
  return true;
}

bool AppendIfClass(YAML::Emitter& yaml, YamlContext const& ctx,
                   pugi::xml_node const& node) {
  if (kind(node) != "class") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("compoundname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << name                        //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "class"                     //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendClassSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  CompoundRecurse(yaml, NestedYamlContext(ctx, node), node);
  return true;
}

bool AppendIfStruct(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node const& node) {
  if (kind(node) != "struct") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("compoundname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << name                        //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "struct"                    //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  AppendStructSyntax(yaml, ctx, node);
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  CompoundRecurse(yaml, NestedYamlContext(ctx, node), node);
  return true;
}

}  // namespace docfx
