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

#include "docfx/doxygen_pages.h"
#include "docfx/config.h"
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen_errors.h"
#include "docfx/public_docs.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string_view>

namespace docfx {

// A "page" appears in the generated XML as:
// clang-format off
//   <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="indexpage" kind="page">
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
std::string Page2Markdown(pugi::xml_node node) {
  if (std::string_view{node.name()} != "compounddef" ||
      std::string_view{node.attribute("kind").as_string()} != "page") {
    std::ostringstream os;
    os << "The node is not a page " << __func__ << "(): node=";
    node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw,
               /*encoding=*/pugi::encoding_auto, /*depth=*/1);
    throw std::runtime_error(std::move(os).str());
  }
  std::ostringstream os;
  os << "---\n";
  auto const id = std::string_view{node.attribute("id").as_string()};
  os << "uid: " << id << "\n";
  os << "---\n\n";
  os << "# ";

  MarkdownContext ctx;
  AppendTitle(os, ctx, node);
  os << "\n";
  for (auto child : node) {
    auto name = std::string_view(child.name());
    if (name == "compoundname") continue;      // no markdown output
    if (name == "briefdescription") continue;  // no markdown output
    if (name == "location") continue;          // no markdown output
    if (name == "title") continue;             // already handled
    // These are unexpected in a page: basecompoundref, derivedcompoundref,
    //    includes, includedby, incdepgraph, invincdepgraph, innerdir,
    //    innerfile, innerclass, innernamespace, innerpage, innergroup,
    //    templateparamlist, sectiondef, inheritancegraph, collaborationgraph,
    //    programlisting, listofallmembers.
    if (AppendIfDetailedDescription(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << "\n";
  return std::move(os).str();
}

}  // namespace docfx
