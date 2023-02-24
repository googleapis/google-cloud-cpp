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

#include "docfx/doxygen2markdown.h"
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace {

[[noreturn]] void UnknownChildType(std::string_view where,
                                   pugi::xml_node const& child) {
  std::ostringstream os;
  os << "Unknown child in " << where << "(): node=";
  child.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
  throw std::runtime_error(std::move(os).str());
}

[[noreturn]] void MissingElement(std::string_view where, std::string_view name,
                                 pugi::xml_node const& node) {
  std::ostringstream os;
  os << "Missing element <" << name << " in " << where << "(): node=";
  node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
  throw std::runtime_error(std::move(os).str());
}

}  // namespace

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
std::string Page2Markdown(pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "compounddef" ||
      std::string_view{node.attribute("kind").as_string()} != "page") {
    std::ostringstream os;
    os << "The node is not a page " << __func__ << "(): node=";
    node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw,
               /*encoding=*/pugi::encoding_auto, /*depth=*/1);
    throw std::runtime_error(std::move(os).str());
  }
  std::stringstream os;
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

// A "sect4" node type is defined as (note the lack of sect5):
//
// clang-format off
//    <xsd:complexType name="docSect4Type" mixed="true">
//      <xsd:sequence>
//        <xsd:element name="title" type="xsd:string" />
//        <xsd:choice maxOccurs="unbounded">
//          <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//          <xsd:element name="internal" type="docInternalS4Type" minOccurs="0" />
//        </xsd:choice>
//      </xsd:sequence>
//      <xsd:attribute name="id" type="xsd:string" />
//    </xsd:complexType>
// clang-format on
bool AppendIfSect4(std::ostream& os, MarkdownContext const& ctx,
                   pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "sect4") return false;
  // A single '#' title is reserved for the document title. The sect4 title uses
  // '#####':
  os << "\n\n##### ";
  AppendTitle(os, ctx, node);
  for (auto const& child : node) {
    // Unexpected: internal -> we do not use this.
    if (std::string_view(child.name()) == "title") continue;  // already handled
    if (AppendIfParagraph(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// A "sect3" node type is defined as:
//
// clang-format off
//   <xsd:complexType name="docSect3Type" mixed="true">
//     <xsd:sequence>
//       <xsd:element name="title" type="xsd:string" minOccurs="0" />
//       <xsd:choice maxOccurs="unbounded">
//         <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//         <xsd:element name="internal" type="docInternalS1Type" minOccurs="0"  maxOccurs="unbounded" />
//         <xsd:element name="sect2" type="docSect2Type" minOccurs="0" maxOccurs="unbounded" />
//       </xsd:choice>
//     </xsd:sequence>
//     <xsd:attribute name="id" type="xsd:string" />
//   </xsd:complexType>
// clang-format on
bool AppendIfSect3(std::ostream& os, MarkdownContext const& ctx,
                   pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "sect3") return false;
  // A single '#' title is reserved for the document title. The sect3 title
  // uses '####'.
  os << "\n\n#### ";
  AppendTitle(os, ctx, node);
  for (auto const& child : node) {
    // Unexpected: internal -> we do not use this.
    if (std::string_view(child.name()) == "title") continue;  // already handled
    if (AppendIfParagraph(os, ctx, child)) continue;
    if (AppendIfSect4(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }

  return true;
}

// A "sect2" node type is defined as:
//
// clang-format off
//   <xsd:complexType name="docSect1Type" mixed="true">
//     <xsd:sequence>
//       <xsd:element name="title" type="xsd:string" minOccurs="0" />
//       <xsd:choice maxOccurs="unbounded">
//         <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//         <xsd:element name="internal" type="docInternalS1Type" minOccurs="0"  maxOccurs="unbounded" />
//         <xsd:element name="sect3" type="docSect2Type" minOccurs="0" maxOccurs="unbounded" />
//       </xsd:choice>
//     </xsd:sequence>
//     <xsd:attribute name="id" type="xsd:string" />
//   </xsd:complexType>
// clang-format on
bool AppendIfSect2(std::ostream& os, MarkdownContext const& ctx,
                   pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "sect2") return false;
  // A single '#' title is reserved for the document title. The sect2 title
  // uses '###':
  os << "\n\n### ";
  AppendTitle(os, ctx, node);
  for (auto const& child : node) {
    // Unexpected: internal -> we do not use this.
    if (std::string_view(child.name()) == "title") continue;  // already handled
    if (AppendIfParagraph(os, ctx, child)) continue;
    if (AppendIfSect3(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// A "sect1" node type is defined as:
//
// clang-format off
//   <xsd:complexType name="docSect1Type" mixed="true">
//     <xsd:sequence>
//       <xsd:element name="title" type="xsd:string" minOccurs="0" />
//       <xsd:choice maxOccurs="unbounded">
//         <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//         <xsd:element name="internal" type="docInternalS1Type" minOccurs="0"  maxOccurs="unbounded" />
//         <xsd:element name="sect2" type="docSect2Type" minOccurs="0" maxOccurs="unbounded" />
//       </xsd:choice>
//     </xsd:sequence>
//     <xsd:attribute name="id" type="xsd:string" />
//   </xsd:complexType>
// clang-format on
bool AppendIfSect1(std::ostream& os, MarkdownContext const& ctx,
                   pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "sect1") return false;
  // A single '#' title is reserved for the document title. The sect1 title
  // uses '##':
  os << "\n\n## ";
  AppendTitle(os, ctx, node);
  for (auto const& child : node) {
    // Unexpected: internal -> we do not use this.
    if (std::string_view(child.name()) == "title") continue;  // already handled
    if (AppendIfParagraph(os, ctx, child)) continue;
    if (AppendIfSect2(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }

  return true;
}

// A "detaileddescription" node type is defined as:
//
// clang-format off
//   <xsd:complexType name="descriptionType" mixed="true">
//     <xsd:sequence>
//       <xsd:element name="title" type="xsd:string" minOccurs="0"/>
//       <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//       <xsd:element name="internal" type="docInternalType" minOccurs="0" maxOccurs="unbounded"/>
//       <xsd:element name="sect1" type="docSect1Type" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//   </xsd:complexType>
// clang-format on
bool AppendIfDetailedDescription(std::ostream& os, MarkdownContext const& ctx,
                                 pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "detaileddescription") return false;
  for (auto const& child : node) {
    // Unexpected: title, internal -> we do not use this...
    if (AppendIfParagraph(os, ctx, child)) continue;
    if (AppendIfSect1(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

bool AppendIfPlainText(std::ostream& os, MarkdownContext const& ctx,
                       pugi::xml_node const& node) {
  if (!std::string_view{node.name()}.empty() || !node.attributes().empty()) {
    return false;
  }
  for (auto const& d : ctx.decorators) os << d;
  os << node.value();
  for (auto i = ctx.decorators.rbegin(); i != ctx.decorators.rend(); ++i) {
    os << *i;
  }
  return true;
}

// The `ulink` elements must satisfy:
//
//   <xsd:complexType name="docURLLink" mixed="true">
//     <xsd:group ref="docTitleCmdGroup" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:attribute name="url" type="xsd:string" />
//   </xsd:complexType>
bool AppendIfULink(std::ostream& os, MarkdownContext const& ctx,
                   pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "ulink") return false;
  os << "[";
  for (auto const child : node) {
    if (AppendIfDocTitleCmdGroup(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << "](" << node.attribute("url").as_string() << ")";
  return true;
}

// The `bold` elements are of type `docMarkupType`. This is basically
// a sequence of `docCmdGroup` elements:
//
//   <xsd:complexType name="docMarkupType" mixed="true">
//     <xsd:group ref="docCmdGroup" minOccurs="0" maxOccurs="unbounded" />
//   </xsd:complexType>
bool AppendIfBold(std::ostream& os, MarkdownContext const& ctx,
                  pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "bold") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("**");
  for (auto const& child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `strike` elements are of type `docMarkupType`. More details in
// `AppendIfBold()`.
bool AppendIfStrike(std::ostream& os, MarkdownContext const& ctx,
                    pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "strike") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("~");
  for (auto const& child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `strike` elements are of type `docMarkupType`. More details in
// `AppendIfBold()`.
bool AppendIfEmphasis(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "emphasis") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("*");
  for (auto const& child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `computeroutput` elements are of type `docMarkupType`. More details in
// `AppendIfBold()`.
bool AppendIfComputerOutput(std::ostream& os, MarkdownContext const& ctx,
                            pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "computeroutput") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("`");
  for (auto const& child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `ref` node element type in Doxygen is defined as below.
//
// Note the recursive definition with docTitleCmdGroup. DoxRefKind is either
// "compound" or "member". That is, the link may refer to a page, namespace,
// class, or similar (the "compound" case) or a terminal member function, type,
// variable, or similar within a compound.
//
// <xsd:complexType name="docRefTextType" mixed="true">
//   <xsd:group ref="docTitleCmdGroup" minOccurs="0" maxOccurs="unbounded" />
//   <xsd:attribute name="refid" type="xsd:string" />
//   <xsd:attribute name="kindref" type="DoxRefKind" />
//   <xsd:attribute name="external" type="xsd:string" />
// </xsd:complexType>
bool AppendIfRef(std::ostream& os, MarkdownContext const& ctx,
                 pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "ref") return false;
  os << "[";
  for (auto const child : node) {
    if (AppendIfDocTitleCmdGroup(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << "]";
  if (!std::string_view(node.attribute("external").as_string()).empty()) {
    os << "(" << node.attribute("external").as_string() << ")";
  } else {
    // DocFX YAML supports `xref:` as the syntax to cross link other documents
    // generated from the same DoxFX YAML source:
    //    https://dotnet.github.io/docfx/tutorial/links_and_cross_references.html#using-cross-reference
    os << "(xref:" << node.attribute("refid").as_string() << ")";
  }
  return true;
}

// The `docTitleCmdGroup` element type in Doxygen is defined as below.
//
// Only one is possible. We will ignore most of them because they do not
// appear in our documents, but we record them here in case they become
// relevant.
//
// <xsd:group name="docTitleCmdGroup">
//   <xsd:choice>
//     <xsd:element name="ulink" type="docURLLink" />
//     <xsd:element name="bold" type="docMarkupType" />
//     <xsd:element name="s" type="docMarkupType" />
//     <xsd:element name="strike" type="docMarkupType" />
//     <xsd:element name="underline" type="docMarkupType" />
//     <xsd:element name="emphasis" type="docMarkupType" />
//     <xsd:element name="computeroutput" type="docMarkupType" />
//     <xsd:element name="subscript" type="docMarkupType" />
//     <xsd:element name="superscript" type="docMarkupType" />
//     <xsd:element name="center" type="docMarkupType" />
//     <xsd:element name="small" type="docMarkupType" />
//     <xsd:element name="del" type="docMarkupType" />
//     <xsd:element name="ins" type="docMarkupType" />
//     <xsd:element name="htmlonly" type="docHtmlOnlyType" />
//     <xsd:element name="manonly" type="xsd:string" />
//     <xsd:element name="xmlonly" type="xsd:string" />
//     <xsd:element name="rtfonly" type="xsd:string" />
//     <xsd:element name="latexonly" type="xsd:string" />
//     <xsd:element name="docbookonly" type="xsd:string" />
//     <xsd:element name="image" type="docImageType" />
//     <xsd:element name="dot" type="docImageType" />
//     <xsd:element name="msc" type="docImageType" />
//     <xsd:element name="plantuml" type="docImageType" />
//     <xsd:element name="anchor" type="docAnchorType" />
//     <xsd:element name="formula" type="docFormulaType" />
//     <xsd:element name="ref" type="docRefTextType" />
//     <xsd:element name="emoji" type="docEmojiType" />
//     <xsd:element name="linebreak" type="docEmptyType" />
//     <xsd:element name="nonbreakablespace" type="docEmptyType" />
//     <xsd:element name="iexcl" type="docEmptyType" />
// ... other "symbols", such as currency, math formulas, accents, etc. ...
// ... upper case greek letters ...
// ... lower case greek letters ...
//   </xsd:choice>
// </xsd:group>
bool AppendIfDocTitleCmdGroup(std::ostream& os, MarkdownContext const& ctx,
                              pugi::xml_node const& node) {
  if (AppendIfPlainText(os, ctx, node)) return true;
  if (AppendIfULink(os, ctx, node)) return true;
  if (AppendIfBold(os, ctx, node)) return true;
  // Unexpected: s
  if (AppendIfStrike(os, ctx, node)) return true;
  // Unexpected: underline
  if (AppendIfEmphasis(os, ctx, node)) return true;
  if (AppendIfComputerOutput(os, ctx, node)) return true;
  // Unexpected: subscript, superscript, center, small, del, ins
  // Unexpected: htmlonly, manonly, rtfonly, latexonly, docbookonly
  // Unexpected: image, dot, msc, plantuml
  if (AppendIfAnchor(os, ctx, node)) return true;
  // Unexpected: formula
  if (AppendIfRef(os, ctx, node)) return true;
  // Unexpected: emoji
  // Unexpected: linebreak
  // Unexpected: nonbreakablespace
  // Unexpected: many many symbols
  return false;
}

// The `docCmdGroup` element type in Doxygen is defined as below.
//
// The use of `xsd:choice` signifies that only one of the options is allowed.
//   <xsd:group name="docCmdGroup">
//   <xsd:choice>
//     <xsd:group ref="docTitleCmdGroup"/>
//     <xsd:element name="hruler" type="docEmptyType" />
//     <xsd:element name="preformatted" type="docMarkupType" />
//     <xsd:element name="programlisting" type="listingType" />
//     <xsd:element name="verbatim" type="xsd:string" />
//     <xsd:element name="indexentry" type="docIndexEntryType" />
//     <xsd:element name="orderedlist" type="docListType" />
//     <xsd:element name="itemizedlist" type="docListType" />
//     <xsd:element name="simplesect" type="docSimpleSectType" />
//     <xsd:element name="title" type="docTitleType" />
//     <xsd:element name="variablelist" type="docVariableListType" />
//     <xsd:element name="table" type="docTableType" />
//     <xsd:element name="heading" type="docHeadingType" />
//     <xsd:element name="dotfile" type="docImageType" />
//     <xsd:element name="mscfile" type="docImageType" />
//     <xsd:element name="diafile" type="docImageType" />
//     <xsd:element name="toclist" type="docTocListType" />
//     <xsd:element name="language" type="docLanguageType" />
//     <xsd:element name="parameterlist" type="docParamListType" />
//     <xsd:element name="xrefsect" type="docXRefSectType" />
//     <xsd:element name="copydoc" type="docCopyType" />
//     <xsd:element name="blockquote" type="docBlockQuoteType" />
//     <xsd:element name="parblock" type="docParBlockType" />
//   </xsd:choice>
// </xsd:group>
bool AppendIfDocCmdGroup(std::ostream& os, MarkdownContext const& ctx,
                         pugi::xml_node const& node) {
  if (AppendIfDocTitleCmdGroup(os, ctx, node)) return true;
  // Unexpected: hruler, preformatted
  if (AppendIfProgramListing(os, ctx, node)) return true;
  // Unexpected: verbatim, indexentry
  if (AppendIfOrderedList(os, ctx, node)) return true;
  if (AppendIfItemizedList(os, ctx, node)) return true;
  if (AppendIfSimpleSect(os, ctx, node)) return true;
  // Unexpected: title
  if (AppendIfVariableList(os, ctx, node)) return true;
  // Unexpected: table, header, dotfile, mscfile, diafile, toclist, language
  // Unexpected: parameterlist, xrefsect, copydoc, blockquote, parblock
  return false;
}

// A `para` element is defined by the `docParaType` in Doxygen, which is defined
// as:
//
// <xsd:complexType name="docParaType" mixed="true">
//   <xsd:group ref="docCmdGroup" minOccurs="0" maxOccurs="unbounded" />
// </xsd:complexType>
//
// The `mixed="true"` signifies that there may be plain text between the
// child elements.
//
// The `<xsd:group>` signifies that there may be 0 or more (unbounded) number of
// `docCmdGroup` child elements.
bool AppendIfParagraph(std::ostream& os, MarkdownContext const& ctx,
                       pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "para") return false;
  os << ctx.paragraph_start << ctx.paragraph_indent;
  for (auto const& child : node) {
    if (AppendIfDocCmdGroup(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The type for `programlisting` elements is basically a sequence of `codeline`
// elements.  Defined as:
//
// clang-format off
//   <xsd:complexType name="listingType">
//     <xsd:sequence>
//       <xsd:element name="codeline" type="codelineType" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//     <xsd:attribute name="filename" type="xsd:string" use="optional"/>
//   </xsd:complexType>
// clang-format on
bool AppendIfProgramListing(std::ostream& os, MarkdownContext const& ctx,
                            pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "programlisting") return false;
  // Start with a new paragraph, with the right level of indentation, and a new
  // code fence.
  os << ctx.paragraph_start << ctx.paragraph_indent << "```cpp";
  for (auto const& child : node) {
    if (AppendIfCodeline(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << "\n" << ctx.paragraph_indent << "```";
  return true;
}

// The type for `codeline` is basically a sequence of highlights (think "syntax
// highlighting", not "important things"). We will discard this information and
// rely in the target markdown to generate the right coloring.
//
// clang-format off
//   <xsd:complexType name="codelineType">
//     <xsd:sequence>
//       <xsd:element name="highlight" type="highlightType" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//     <xsd:attribute name="lineno" type="xsd:integer" />
//     <xsd:attribute name="refid" type="xsd:string" />
//     <xsd:attribute name="refkind" type="DoxRefKind" />
//     <xsd:attribute name="external" type="DoxBool" />
//   </xsd:complexType>
// clang-format on
bool AppendIfCodeline(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "codeline") return false;
  os << "\n" << ctx.paragraph_indent;
  for (auto const& child : node) {
    if (AppendIfHighlight(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The type for `highlight` is basically a sequence of `<sp>` and `<ref>`
// elements. The `<ref>` elements are where the text appears.
//
//   <xsd:complexType name="highlightType" mixed="true">
//     <xsd:choice minOccurs="0" maxOccurs="unbounded">
//       <xsd:element name="sp" type="spType" />
//       <xsd:element name="ref" type="refTextType" />
//     </xsd:choice>
//     <xsd:attribute name="class" type="DoxHighlightClass" />
//   </xsd:complexType>
bool AppendIfHighlight(std::ostream& os, MarkdownContext const& ctx,
                       pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "highlight") return false;
  for (auto const& child : node) {
    if (AppendIfPlainText(os, ctx, child)) continue;
    if (AppendIfHighlightSp(os, ctx, child)) continue;
    if (AppendIfHighlightRef(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// A `<sp>` element is just a space. It seems that Doxygen does not use the
// `value` attribute, so we will leave that unhandled.
//
//   <xsd:complexType name="spType" mixed="true">
//     <xsd:attribute name="value" type="xsd:integer" use="optional"/>
//   </xsd:complexType>
bool AppendIfHighlightSp(std::ostream& os, MarkdownContext const& /*ctx*/,
                         pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "sp") return false;
  // Leave the 'value' attribute unhandled. It is probably the number of spaces,
  // but without documentation it is hard to say. Since it is unused, this
  // approach seems safer.
  if (!std::string_view{node.attribute("value").name()}.empty()) return false;
  os << ' ';
  return true;
}

// A `ref` element inside a `highlight` element has `refTextType`, which is
// defined as:
//
//   <xsd:complexType name="docRefTextType" mixed="true">
//     <xsd:group ref="docTitleCmdGroup" minOccurs="0" maxOccurs="unbounded" />
//     <xsd:attribute name="refid" type="xsd:string" />
//     <xsd:attribute name="kindref" type="DoxRefKind" />
//     <xsd:attribute name="external" type="xsd:string" />
//   </xsd:complexType>
bool AppendIfHighlightRef(std::ostream& os, MarkdownContext const& ctx,
                          pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "ref") return false;
  for (auto const& child : node) {
    if (AppendIfDocTitleCmdGroup(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

/// Handle `orderedlist` elements.
bool AppendIfOrderedList(std::ostream& os, MarkdownContext const& ctx,
                         pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "orderedlist") return false;
  auto nested = ctx;
  nested.paragraph_indent = std::string(ctx.paragraph_indent.size(), ' ');
  nested.item_prefix = "1. ";
  for (auto const& child : node) {
    if (AppendIfListItem(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

bool AppendIfItemizedList(std::ostream& os, MarkdownContext const& ctx,
                          pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "itemizedlist") return false;
  auto nested = ctx;
  nested.paragraph_indent = std::string(ctx.paragraph_indent.size(), ' ');
  nested.item_prefix = "- ";
  for (auto const& child : node) {
    if (AppendIfListItem(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

bool AppendIfListItem(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "listitem") return false;
  // The first paragraph is the list item is indented as needed, and starts
  // with the item prefix (typically "- " or "1. ").
  auto nested = ctx;
  nested.paragraph_start = "\n";
  nested.paragraph_indent = ctx.paragraph_indent + ctx.item_prefix;
  for (auto const& child : node) {
    if (AppendIfParagraph(os, nested, child)) {
      // Subsequence paragraphs within the same list item require a blank line
      nested.paragraph_start = "\n\n";
      nested.paragraph_indent =
          ctx.paragraph_indent + std::string(ctx.item_prefix.size(), ' ');
      continue;
    }
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `variablelist` element type is defined as a sequence of "groups".
// Groups do not create an XML element, they are simply a description of
// "element A is followed by element B". This requires some funky processing.
//
//   <xsd:complexType name="docVariableListType">
//     <xsd:sequence>
//       <xsd:group ref="docVariableListGroup" maxOccurs="unbounded" />
//     </xsd:sequence>
//   </xsd:complexType>
//
//   <xsd:group name="docVariableListGroup">
//     <xsd:sequence>
//       <xsd:element name="varlistentry" type="docVarListEntryType" />
//       <xsd:element name="listitem" type="docListItemType" />
//     </xsd:sequence>
//   </xsd:group>
bool AppendIfVariableList(std::ostream& os, MarkdownContext const& ctx,
                          pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "variablelist") return false;

  auto nested = ctx;
  nested.paragraph_start = "\n";
  nested.paragraph_indent = ctx.paragraph_indent + "- ";
  for (auto const& child : node) {
    if (AppendIfVariableListEntry(os, nested, child)) {
      // Subsequence paragraphs within the same list item require a blank line
      nested.paragraph_start = "\n\n";
      nested.paragraph_indent = ctx.paragraph_indent + "  ";
      continue;
    }
    if (AppendIfVariableListItem(os, nested, child)) {
      nested.paragraph_start = "\n";
      nested.paragraph_indent = ctx.paragraph_indent + "- ";
      continue;
    }
    UnknownChildType(__func__, child);
  }
  return true;
}

// A `varlistentry` contains a single `term` element.
//
//   <xsd:complexType name="docVarListEntryType">
//     <xsd:sequence>
//       <xsd:element name="term" type="docTitleType" />
//     </xsd:sequence>
//   </xsd:complexType>
//
//   <xsd:complexType name="docTitleType" mixed="true">
//     <xsd:group ref="docTitleCmdGroup" minOccurs="0" maxOccurs="unbounded" />
//   </xsd:complexType>
bool AppendIfVariableListEntry(std::ostream& os, MarkdownContext const& ctx,
                               pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "varlistentry") return false;
  auto const term = node.child("term");
  if (!term) MissingElement(__func__, "term", node);
  os << ctx.paragraph_start << ctx.paragraph_indent;
  for (auto const& child : term) {
    if (AppendIfDocTitleCmdGroup(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// A `listitem` in the middle of a `variablelist` is a sequence of paragraphs.
//
// clang-format off
//   <xsd:complexType name="docListItemType">
//     <xsd:sequence>
//       <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//   </xsd:complexType>
// clang-format on
bool AppendIfVariableListItem(std::ostream& os, MarkdownContext const& ctx,
                              pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "listitem") return false;
  for (auto const& child : node) {
    if (AppendIfParagraph(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `simplesect` element type in Doxygen is defined as below.
//
// These are small sections, such as the `@see` notes, or a `@warning`
// callout.  How we want to render them depends on their type. For most we will
// use a simple H6 header, but things like 'warning' or 'note' deserve a block
// quote.
//
// clang-format off
//   <xsd:simpleType name="DoxSimpleSectKind">
//     <xsd:restriction base="xsd:string">
//       <xsd:enumeration value="see" />
//       <xsd:enumeration value="return" />
//       <xsd:enumeration value="author" />
//       <xsd:enumeration value="authors" />
//       <xsd:enumeration value="version" />
//       <xsd:enumeration value="since" />
//       <xsd:enumeration value="date" />
//       <xsd:enumeration value="note" />
//       <xsd:enumeration value="warning" />
//       <xsd:enumeration value="pre" />
//       <xsd:enumeration value="post" />
//       <xsd:enumeration value="copyright" />
//       <xsd:enumeration value="invariant" />
//       <xsd:enumeration value="remark" />
//       <xsd:enumeration value="attention" />
//       <xsd:enumeration value="par" />
//       <xsd:enumeration value="rcs" />
//     </xsd:restriction>
//   </xsd:simpleType>
//
//   <xsd:complexType name="docSimpleSectType">
//     <xsd:sequence>
//       <xsd:element name="title" type="docTitleType" minOccurs="0" />
//       <xsd:sequence minOccurs="0" maxOccurs="unbounded">
//         <xsd:element name="para" type="docParaType" minOccurs="1" maxOccurs="unbounded" />
//       </xsd:sequence>
//     </xsd:sequence>
//     <xsd:attribute name="kind" type="DoxSimpleSectKind" />
//   </xsd:complexType>
// clang-format on
bool AppendIfSimpleSect(std::ostream& os, MarkdownContext const& ctx,
                        pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "simplesect") return false;
  static auto const* const kUseH6 = [] {
    return new std::unordered_set<std::string>{
        "see", "return", "author",    "authors",   "version", "since", "date",
        "pre", "post",   "copyright", "invariant", "par",     "rcs",
    };
  }();

  auto nested = ctx;
  nested.paragraph_start = "\n";
  nested.paragraph_indent = ctx.paragraph_indent + "> ";

  auto const kind = std::string{node.attribute("kind").as_string()};
  if (kUseH6->count(kind) != 0) {
    nested = ctx;
    os << "\n\n###### ";
    AppendTitle(os, nested, node);
  } else if (kind == "note") {
    os << "\n";
    os << nested.paragraph_start << nested.paragraph_indent << "**Note:**";
  } else if (kind == "warning") {
    os << "\n";
    os << nested.paragraph_start << nested.paragraph_indent << "**Warning:**";
  } else if (kind == "remark") {
    os << "\n";
    os << nested.paragraph_start << nested.paragraph_indent << "Remark:";
  } else if (kind == "attention") {
    os << "\n";
    os << nested.paragraph_start << nested.paragraph_indent << "Attention:";
  } else {
    std::ostringstream os;
    os << "Unknown simplesect kind in " << __func__ << "(): node=";
    node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
    throw std::runtime_error(std::move(os).str());
  }

  for (auto const& child : node) {
    if (std::string_view{child.name()} == "title") continue;
    if (AppendIfParagraph(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

bool AppendIfAnchor(std::ostream& /*os*/, MarkdownContext const& /*ctx*/,
                    pugi::xml_node const& node) {
  // Do not generate any code for anchors, they have no obvious mapping to
  // Markdown.
  return std::string_view{node.name()} == "anchor";
}

void AppendTitle(std::ostream& os, MarkdownContext const& ctx,
                 pugi::xml_node const& node) {
  // The XML schema says there is only one of these, but it is easier to write
  // the loop.
  for (auto const& title : node.children("title")) {
    for (auto const& child : title) {
      if (AppendIfPlainText(os, ctx, child)) continue;
      UnknownChildType(__func__, child);
    }
  }
}
