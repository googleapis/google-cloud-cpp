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

#include "docfx/doxygen_groups.h"
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen2yaml.h"
#include "docfx/doxygen_errors.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>

namespace docfx {
namespace {

void AppendReferences(YAML::Emitter& yaml, pugi::xml_node const& node) {
  for (auto const& child : node) {
    auto const name = std::string_view{child.name()};
    if (name == "sectiondef") {
      AppendReferences(yaml, child);
      continue;
    }
    if (name == "innergroup") {
      AppendReferences(yaml, child);
      continue;
    }
    if (name == "memberdef") {
      yaml << std::string{child.attribute("id").as_string()};
      continue;
    }
    if (name == "innerclass") {
      yaml << std::string{child.attribute("refid").as_string()};
      continue;
    }
  }
}

}  // namespace

std::string Group2Yaml(Config const& /*config*/, pugi::xml_node const& node) {
  auto const id = std::string{node.attribute("id").as_string()};
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  yaml << YAML::Key << "items" << YAML::Value << YAML::BeginSeq;
  yaml << YAML::BeginMap;
  yaml << YAML::Key << "uid" << YAML::Value << id         //
       << YAML::Key << "name" << YAML::Value << id        //
       << YAML::Key << "id" << YAML::Value << id          //
       << YAML::Key << "type" << YAML::Value << "module"  //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq
       << YAML::Key << "summary" << YAML::Literal
       << Group2SummaryMarkdown(node);
  yaml << YAML::Key << "references" << YAML::BeginSeq;
  AppendReferences(yaml, node);
  yaml << YAML::EndSeq;
  yaml << YAML::EndMap;
  yaml << YAML::EndSeq << YAML::EndMap;
  return std::string{"### YamlMime:UniversalReference\n"} + yaml.c_str() + "\n";
}

// A "group" appears in the generated XML as:
// clang-format off
//   <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__guac" kind="group">
// clang-format on
//
// That is, they are generic `compounddef` nodes -- the same entity used to
// represent class or function reference docs. The definition is fairly complex
// (see below).  We will ignore things that we do not expect, such as
// include diagrams, inner classes, etc.
//
// clang-format off
//   <xsd:complexType name="DoxygenType">
//     <xsd:sequence maxOccurs="unbounded">
//       <xsd:element name="compounddef" type="compounddefType" minOccurs="0" />
//     </xsd:sequence>
//     <xsd:attribute name="version" type="DoxVersionNumber" use="required" />
//     <xsd:attribute ref="xml:lang" use="required"/>
//   </xsd:complexType>
//
//   <xsd:complexType name="compounddefType">
//   <xsd:sequence>
//     <xsd:element name="compoundname" type="xsd:string"/>
//     <xsd:element name="title" type="xsd:string" minOccurs="0" />
//     <xsd:element name="basecompoundref" type="compoundRefType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="derivedcompoundref" type="compoundRefType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="includes" type="incType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="includedby" type="incType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="incdepgraph" type="graphType" minOccurs="0" />
//     <xsd:element name="invincdepgraph" type="graphType" minOccurs="0" />
//     <xsd:element name="innerdir" type="refType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="innerfile" type="refType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="innerclass" type="refType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="innernamespace" type="refType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="innerpage" type="refType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="innergroup" type="refType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="templateparamlist" type="templateparamlistType" minOccurs="0" />
//     <xsd:element name="sectiondef" type="sectiondefType" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:element name="tableofcontents" type="tableofcontentsType" minOccurs="0" maxOccurs="1" />
//     <xsd:element name="briefdescription" type="descriptionType" minOccurs="0" />
//     <xsd:element name="detaileddescription" type="descriptionType" minOccurs="0" />
//     <xsd:element name="inheritancegraph" type="graphType" minOccurs="0" />
//     <xsd:element name="collaborationgraph" type="graphType" minOccurs="0" />
//     <xsd:element name="programlisting" type="listingType" minOccurs="0" />
//     <xsd:element name="location" type="locationType" minOccurs="0" />
//     <xsd:element name="listofallmembers" type="listofallmembersType" minOccurs="0" />
//   </xsd:sequence>
//   <xsd:attribute name="id" type="xsd:string" />
//   <xsd:attribute name="kind" type="DoxCompoundKind" />
//   <xsd:attribute name="language" type="DoxLanguage" use="optional"/>
//   <xsd:attribute name="prot" type="DoxProtectionKind" />
//   <xsd:attribute name="final" type="DoxBool" use="optional"/>
//   <xsd:attribute name="inline" type="DoxBool" use="optional"/>
//   <xsd:attribute name="sealed" type="DoxBool" use="optional"/>
//   <xsd:attribute name="abstract" type="DoxBool" use="optional"/>
// </xsd:complexType>
// clang-format on
std::string Group2SummaryMarkdown(pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "compounddef" ||
      std::string_view{node.attribute("kind").as_string()} != "group") {
    std::ostringstream os;
    os << "The node is not a group " << __func__ << "(): node=";
    node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw,
               /*encoding=*/pugi::encoding_auto, /*depth=*/1);
    throw std::runtime_error(std::move(os).str());
  }
  std::ostringstream os;
  MarkdownContext ctx;
  os << "# ";
  AppendTitle(os, ctx, node);
  os << "\n";
  for (auto const& child : node) {
    auto name = std::string_view(child.name());
    if (name == "compoundname") continue;      // no markdown output
    if (name == "briefdescription") continue;  // no markdown output
    if (name == "location") continue;          // no markdown output
    if (name == "title") continue;             // already handled
    if (name == "sectiondef") continue;        // no markdown output
    if (name == "innerclass") continue;        // no markdown output
    if (name == "innergroup") continue;        // no markdown output
    // These are unexpected in a page: basecompoundref, derivedcompoundref,
    //    includes, includedby, incdepgraph, invincdepgraph, innerdir,
    //    innerfile, innerpage, templateparamlist, inheritancegraph,
    //    collaborationgraph, programlisting, listofallmembers.
    if (AppendIfDetailedDescription(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << "\n";
  return std::move(os).str();
}

std::vector<TocEntry> GroupsToc(pugi::xml_document const& doc) {
  std::vector<TocEntry> result;
  auto nodes = doc.select_nodes("//*[@kind='group']");
  std::transform(nodes.begin(), nodes.end(), std::back_inserter(result),
                 [](auto const& i) {
                   auto const& page = i.node();
                   auto const id =
                       std::string_view{page.attribute("id").as_string()};
                   return TocEntry{std::string(id) + ".yml", std::string(id)};
                 });
  return result;
}

}  // namespace docfx
