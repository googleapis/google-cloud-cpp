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

#include "docfx/doxygen2toc.h"
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen2yaml.h"
#include "docfx/doxygen_groups.h"
#include "docfx/doxygen_pages.h"
#include "docfx/function_classifiers.h"
#include "docfx/node_name.h"
#include "docfx/public_docs.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>

namespace docfx {
namespace {

std::shared_ptr<TocEntry> NamedEntry(std::string_view name) {
  auto entry = std::make_shared<TocEntry>();
  entry->name = name;
  return entry;
}

std::shared_ptr<TocEntry> CompoundEntry(pugi::xml_node node) {
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto overview = NamedEntry("Overview");
  overview->attr.emplace("uid", id);
  auto entry = NamedEntry(NodeName(node));
  entry->items.push_back(std::move(overview));
  return entry;
}

TocItems MemberEntry(YamlContext const& ctx, pugi::xml_node node) {
  // Skip MOCK_METHOD() functions.
  auto const name = NodeName(node);
  if (name.rfind("MOCK_METHOD", 0) == 0) return {};

  auto actual = MockingNode(ctx, node);
  auto const id = std::string_view{actual.attribute("id").as_string()};
  auto entry = NamedEntry(name);
  entry->attr.emplace("uid", id);
  return {entry};
}

std::shared_ptr<TocEntry> EnumValueEntry(pugi::xml_node node) {
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto entry = NamedEntry(NodeName(node));
  entry->attr.emplace("uid", id);
  return entry;
}

bool IsNamespace(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()} == "namespace";
}

bool IsClass(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()} == "class";
}

bool IsStruct(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()} == "struct";
}

bool IsEnum(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()} == "enum";
}

bool IsTypedef(pugi::xml_node node) {
  return std::string_view{node.attribute("kind").as_string()} == "typedef";
}

TocItems NamespaceToc(YamlContext const& ctx, pugi::xml_document const& doc,
                      pugi::xml_node node);
TocItems ClassToc(YamlContext const& ctx, pugi::xml_document const& doc,
                  pugi::xml_node node);
TocItems EnumToc(YamlContext const& ctx, pugi::xml_document const& doc,
                 pugi::xml_node node);

// Generate ToC entries for any elements
TocItems GenericToc(YamlContext const& ctx, pugi::xml_document const& doc,
                    pugi::xml_node node) {
  if (!IncludeInPublicDocuments(ctx.config, node)) return {};
  if (IsClass(node)) return ClassToc(ctx, doc, node);
  if (IsStruct(node)) return ClassToc(ctx, doc, node);
  if (IsEnum(node)) return EnumToc(ctx, doc, node);
  if (IsNamespace(node)) return NamespaceToc(ctx, doc, node);
  auto const element = std::string_view{node.name()};
  if (element == "memberdef") return MemberEntry(ctx, node);
  if (element == "enumvalue") return {EnumValueEntry(node)};
  return {};
}

// Recursively build the ToC.
//
// This function only generates a ToC for some elements in the first level of
// recursion. This makes it possible to "group" the elements by some predicate,
// e.g. all the "Constructors" are grouped. The filtering does not recurse
// for things like "classes" we want to list all the attributes of the matching
// classes.
using Predicate = std::function<bool(pugi::xml_node node)>;

TocItems Recurse(YamlContext const& ctx, pugi::xml_document const& doc,
                 pugi::xml_node node, Predicate const& pred) {
  if (!IncludeInPublicDocuments(ctx.config, node)) return {};
  TocItems items;
  for (auto const child : node) {
    if (!IncludeInPublicDocuments(ctx.config, child)) continue;
    auto const element = std::string_view{child.name()};
    // A <sectiondef> element defines groups of members, such as, "public
    // functions", or "private member variables". They currently do not get
    // a representation in the ToC.
    if (element == "sectiondef") {
      items.splice(items.end(),
                   Recurse(NestedYamlContext(ctx, node), doc, child, pred));
      continue;
    }
    // In the Doxygen XML file classes are referenced, but not defined, as
    // a child element of the namespace element.  That is, the XML structure is:
    //
    // clang-format off
    //   <doxygen>
    //      <compounddef kind="namespace" id="namespacefoo">
    //        <innerclass refid="classfoo_1_1Bar">foo::Bar</innerclass>
    //      </compounddef>
    //      <compounddef kind="class" id="classfoo_1_1Bar">
    //      </compounddef>
    //   </doxygen>
    // clang-format on
    //
    // We want the classes to appear inside the namespace, so we need to lookup
    // the referenced class and generate its ToC recursively.
    if (element == "innerclass") {
      pugi::xpath_variable_set vars;
      vars.add("id", pugi::xpath_type_string);
      vars.set("id", child.attribute("refid").as_string());
      auto query = pugi::xpath_query("//compounddef[@id = string($id)]", &vars);
      auto child = doc.select_node(query);
      // Skip the referenced element if it does not match the predicate.
      if (!child || !pred(child.node())) continue;
      items.splice(items.end(), GenericToc(ctx, doc, child.node()));
      continue;
    }
    // Skip the element if it does not match the predicate.
    if (!pred(child)) continue;
    items.splice(items.end(), GenericToc(ctx, doc, child));
  }
  return items;
}

using TocItemsProducer = std::function<TocItems()>;

TocItems NamespaceToc(YamlContext const& ctx, pugi::xml_document const& doc,
                      pugi::xml_node node) {
  if (!IncludeInPublicDocuments(ctx.config, node)) return {};
  auto entry = CompoundEntry(node);
  struct Node {
    std::string_view name;
    TocItemsProducer producer;
  } nodes[] = {
      {"Classes", [&] { return Recurse(ctx, doc, node, IsClass); }},
      {"Structs", [&] { return Recurse(ctx, doc, node, IsStruct); }},
      {"Functions", [&] { return Recurse(ctx, doc, node, IsPlainFunction); }},
      {"Operators", [&] { return Recurse(ctx, doc, node, IsOperator); }},
      {"Enums", [&] { return Recurse(ctx, doc, node, IsEnum); }},
      {"Types", [&] { return Recurse(ctx, doc, node, IsTypedef); }},
  };
  for (auto const& [name, generator] : nodes) {
    auto items = generator();
    if (items.empty()) continue;
    auto node = NamedEntry(name);
    node->items = std::move(items);
    entry->items.push_back(node);
  }
  return {std::move(entry)};
}

TocItems ClassToc(YamlContext const& ctx, pugi::xml_document const& doc,
                  pugi::xml_node node) {
  if (!IncludeInPublicDocuments(ctx.config, node)) return {};
  auto const nested = NestedYamlContext(ctx, node);
  auto entry = CompoundEntry(node);
  struct Node {
    std::string_view name;
    TocItemsProducer producer;
  } nodes[] = {
      // Skip these. They also appear as `<innerclass>` elements in the
      // namespace.
      // {"Classes", [&] { return Recurse(cfg, doc, node, IsClass); }},
      // {"Structs", [&] { return Recurse(cfg, doc, node, IsStruct); }},
      {"Constructors",
       [&] { return Recurse(nested, doc, node, IsConstructor); }},
      {"Operators", [&] { return Recurse(nested, doc, node, IsOperator); }},
      {"Functions",
       [&] { return Recurse(nested, doc, node, IsPlainFunction); }},
      {"Enums", [&] { return Recurse(nested, doc, node, IsEnum); }},
      {"Types", [&] { return Recurse(nested, doc, node, IsTypedef); }},
  };
  for (auto const& [name, generator] : nodes) {
    auto items = generator();
    if (items.empty()) continue;
    auto node = NamedEntry(name);
    node->items = std::move(items);
    entry->items.push_back(node);
  }
  return {std::move(entry)};
}

TocItems EnumToc(YamlContext const& ctx, pugi::xml_document const& doc,
                 pugi::xml_node node) {
  if (!IncludeInPublicDocuments(ctx.config, node)) return {};
  auto const nested = NestedYamlContext(ctx, node);
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto entry = NamedEntry(NodeName(node));
  auto overview = NamedEntry("Overview");
  overview->attr.emplace("uid", id);
  entry->items.push_back(std::move(overview));
  for (auto const child : node) {
    if (!IncludeInPublicDocuments(ctx.config, child)) continue;
    auto const element = std::string_view{child.name()};
    if (element == "enumvalue") {
      entry->items.splice(entry->items.end(), GenericToc(nested, doc, child));
      continue;
    }
  }
  return {std::move(entry)};
}

TocItems Indexpage(Config const& /*config*/, pugi::xml_document const& doc) {
  pugi::xpath_variable_set vars;
  vars.add("id", pugi::xpath_type_string);
  vars.set("id", "indexpage");
  auto query = pugi::xpath_query("//compounddef[@id = string($id)]", &vars);
  auto child = doc.select_node(query);
  if (!child) return {};
  std::ostringstream title;
  AppendTitle(title, MarkdownContext{}, child.node());
  auto entry = NamedEntry(title.str());
  entry->attr.emplace("href", "index.md");
  entry->attr.emplace("uid", "indexpage");
  return {std::move(entry)};
}

TocItems Pages(Config const& config, pugi::xml_document const& doc) {
  TocItems items;
  for (auto const& i : doc.select_nodes("//*[@kind='page']")) {
    auto const page = i.node();
    if (!IncludeInPublicDocuments(config, page)) continue;
    auto const id = std::string_view{page.attribute("id").as_string()};
    // Skip endpoint and authorization override snippets.
    if (id.find("-endpoint-snippet") != std::string_view::npos) continue;
    if (id.find("-account-snippet") != std::string_view::npos) continue;
    if (id == "indexpage") continue;
    std::ostringstream title;
    AppendTitle(title, MarkdownContext{}, page);
    auto entry = NamedEntry(title.str());
    entry->attr.emplace("href", std::string{id} + ".md");
    entry->attr.emplace("uid", id);
    items.push_back(std::move(entry));
  }
  return items;
}

TocItems Groups(Config const& /*config*/, pugi::xml_document const& doc) {
  TocItems items;
  for (auto const& i : doc.select_nodes("//*[@kind='group']")) {
    auto const group = i.node();
    auto const id = std::string{group.attribute("id").as_string()};
    std::ostringstream title;
    AppendTitle(title, MarkdownContext{}, group);
    auto entry = NamedEntry(title.str());
    entry->attr.emplace("href", std::string{id} + ".yml");
    entry->attr.emplace("uid", id);
    items.push_back(std::move(entry));
  }
  return items;
}

TocItems Namespaces(Config const& config, pugi::xml_document const& doc) {
  YamlContext ctx;
  ctx.config = config;
  TocItems items;
  for (auto const& i : doc.select_nodes("//compounddef[@kind='namespace']")) {
    if (!IncludeInPublicDocuments(config, i.node())) continue;
    items.splice(items.end(), NamespaceToc(ctx, doc, i.node()));
  }
  return items;
}

void Toc2Yaml(YAML::Emitter& out, TocEntry const& e) {
  out << YAML::BeginMap;
  out << YAML::Key << "name" << YAML::Value << YAML::DoubleQuoted << e.name;
  for (auto const& [key, value] : e.attr) {
    out << YAML::Key << key << YAML::Value << value;
  }
  if (!e.items.empty()) {
    out << YAML::Key << "items" << YAML::Value << YAML::BeginSeq;
    for (auto const& entry : e.items) Toc2Yaml(out, *entry);
    out << YAML::EndSeq;
  }
  out << YAML::EndMap;
}

TocEntry BuildToc(Config const& config, pugi::xml_document const& doc) {
  TocEntry toc;
  toc.name = std::string{"cloud.google.com/cpp/"} + config.library;
  toc.items.splice(toc.items.end(), Indexpage(config, doc));
  struct Node {
    std::string_view name;
    std::function<TocItems(Config const&, pugi::xml_document const&)> generator;
  } nodes[] = {
      {"In-Depth Topics", Pages},
      {"Modules", Groups},
      {"Namespaces", Namespaces},
  };
  for (auto const& [name, generator] : nodes) {
    auto items = generator(config, doc);
    if (items.empty()) continue;
    auto node = NamedEntry(name);
    node->items = std::move(items);
    toc.items.push_back(node);
  }
  return toc;
}

}  // namespace

std::string Doxygen2Toc(Config const& config, pugi::xml_document const& doc) {
  auto const toc = BuildToc(config, doc);
  YAML::Emitter out;
  Toc2Yaml(out, toc);
  std::ostringstream os;
  os << "### YamlMime:TableOfContent\n" << out.c_str() << "\n";
  return std::move(os).str();
}

}  // namespace docfx
