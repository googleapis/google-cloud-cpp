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
#include "docfx/doxygen_errors.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace docfx {
namespace {

std::string EscapeCodeLink(std::string link) {
  // C++ names that are fully qualified often contain markdown emoji's (e.g.
  // `:cloud:`). We need to escape them as "computer output", but only if they
  // are not escaped already.
  if (link.find("::") != std::string::npos &&
      link.find('`') == std::string::npos) {
    link = "`" + link + "`";
  }
  return link;
}

}  // namespace

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
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "sect4") return false;
  // A single '#' title is reserved for the document title. The sect4 title uses
  // '#####':
  os << "\n\n##### ";
  AppendTitle(os, ctx, node);
  for (auto const child : node) {
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
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "sect3") return false;
  // A single '#' title is reserved for the document title. The sect3 title
  // uses '####'.
  os << "\n\n#### ";
  AppendTitle(os, ctx, node);
  for (auto const child : node) {
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
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "sect2") return false;
  // A single '#' title is reserved for the document title. The sect2 title
  // uses '###':
  os << "\n\n### ";
  AppendTitle(os, ctx, node);
  for (auto const child : node) {
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
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "sect1") return false;
  // A single '#' title is reserved for the document title. The sect1 title
  // uses '##':
  os << "\n\n## ";
  AppendTitle(os, ctx, node);
  for (auto const child : node) {
    // Unexpected: internal -> we do not use this.
    if (std::string_view(child.name()) == "title") continue;  // already handled
    if (AppendIfParagraph(os, ctx, child)) continue;
    if (AppendIfSect2(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }

  return true;
}

// A "xrefsect" node type is defined as:
//
// clang-format off
//   <xsd:complexType name="docXRefSectType">
//     <xsd:sequence>
//       <xsd:element name="xreftitle" type="xsd:string" minOccurs="0" maxOccurs="unbounded" />
//       <xsd:element name="xrefdescription" type="descriptionType" />
//     </xsd:sequence>
//     <xsd:attribute name="id" type="xsd:string" />
//   </xsd:complexType>
// clang-format on
bool AppendIfXRefSect(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "xrefsect") return false;
  if (ctx.skip_xrefsect) return true;
  auto const title = std::string_view{node.child_value("xreftitle")};
  if (title == "Deprecated") {
    // The GCP site has a special representation for deprecated elements.
    os << R"""(<aside class="deprecated"><b>Deprecated:</b>)""" << "\n";
    AppendDescriptionType(os, ctx, node.child("xrefdescription"));
    os << "\n</aside>";
    return true;
  }
  // Add the title in bold, then the description.
  os << "**" << title << "**\n\n";
  AppendDescriptionType(os, ctx, node.child("xrefdescription"));
  return true;
}

// All "*description" nodes have this type:
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
void AppendDescriptionType(std::ostream& os, MarkdownContext const& ctx,
                           pugi::xml_node node) {
  auto nested = ctx;
  bool first_paragraph = true;
  for (auto const child : node) {
    if (!first_paragraph) nested.paragraph_start = "\n\n";
    first_paragraph = false;
    // Unexpected: title, internal -> we do not use this...
    if (AppendIfParagraph(os, nested, child)) continue;
    if (AppendIfSect1(os, nested, child)) continue;
    // While the XML schema does not allow for `sect2`, `sect3`, or `sect4`
    // elements, in practice Doxygen does generate them. And we use them in at
    // least one page.
    if (AppendIfSect2(os, nested, child)) continue;
    if (AppendIfSect3(os, nested, child)) continue;
    if (AppendIfSect4(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
}

bool AppendIfDetailedDescription(std::ostream& os, MarkdownContext const& ctx,
                                 pugi::xml_node node) {
  if (std::string_view{node.name()} != "detaileddescription") return false;
  AppendDescriptionType(os, ctx, node);
  return true;
}

bool AppendIfBriefDescription(std::ostream& os, MarkdownContext const& ctx,
                              pugi::xml_node node) {
  if (std::string_view{node.name()} != "briefdescription") return false;
  AppendDescriptionType(os, ctx, node);
  return true;
}

bool AppendIfPlainText(std::ostream& os, MarkdownContext const& ctx,
                       pugi::xml_node node) {
  if (!std::string_view{node.name()}.empty() || !node.attributes().empty()) {
    return false;
  }
  // Doxygen injects the following sequence when a zero-width joiner character
  // is in the middle of a code span.  We need to remove them.
  auto const zwj = std::string_view{"&zwj;"};
  auto value = std::string{node.value()};
  auto const end = std::string::npos;
  std::string::size_type pos = 0;
  for (pos = value.find(zwj, pos); pos != end; pos = value.find(zwj, pos)) {
    value = value.erase(pos, zwj.size());
  }

  for (auto const& d : ctx.decorators) os << d;
  os << value;
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
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "ulink") return false;
  std::ostringstream link;
  for (auto const child : node) {
    if (AppendIfDocTitleCmdGroup(link, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  auto const ref = EscapeCodeLink(std::move(link).str());
  os << "[" << ref << "](" << node.attribute("url").as_string() << ")";
  return true;
}

// The `bold` elements are of type `docMarkupType`. This is basically
// a sequence of `docCmdGroup` elements:
//
//   <xsd:complexType name="docMarkupType" mixed="true">
//     <xsd:group ref="docCmdGroup" minOccurs="0" maxOccurs="unbounded" />
//   </xsd:complexType>
bool AppendIfBold(std::ostream& os, MarkdownContext const& ctx,
                  pugi::xml_node node) {
  if (std::string_view{node.name()} != "bold") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("**");
  for (auto const child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `strike` elements are of type `docMarkupType`. More details in
// `AppendIfBold()`.
bool AppendIfStrike(std::ostream& os, MarkdownContext const& ctx,
                    pugi::xml_node node) {
  if (std::string_view{node.name()} != "strike") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("~");
  for (auto const child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `strike` elements are of type `docMarkupType`. More details in
// `AppendIfBold()`.
bool AppendIfEmphasis(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "emphasis") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("*");
  for (auto const child : node) {
    if (AppendIfDocCmdGroup(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The `computeroutput` elements are of type `docMarkupType`. More details in
// `AppendIfBold()`.
bool AppendIfComputerOutput(std::ostream& os, MarkdownContext const& ctx,
                            pugi::xml_node node) {
  if (std::string_view{node.name()} != "computeroutput") return false;
  auto nested = ctx;
  nested.decorators.emplace_back("`");
  for (auto const child : node) {
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
                 pugi::xml_node node) {
  if (std::string_view{node.name()} != "ref") return false;

  std::ostringstream link;
  for (auto const child : node) {
    if (AppendIfDocTitleCmdGroup(link, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  auto const ref = EscapeCodeLink(std::move(link).str());

  // DocFX YAML supports `xref:` as the syntax to cross link other documents
  // generated from the same DoxFX YAML source:
  //    https://dotnet.github.io/docfx/tutorial/links_and_cross_references.html#using-cross-reference
  os << "[" << ref << "]" << "(xref:" << node.attribute("refid").as_string()
     << ")";
  return true;
}

// The `ndash` element is just a convenient way to represent long dashes.
bool AppendIfNDash(std::ostream& os, MarkdownContext const& /*ctx*/,
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "ndash") return false;
  os << "&ndash;";
  return true;
}

// The `linebreak` represents a linebreak. Use `<br>` because we are targeting
// a dialect of markdown that supports it.
bool AppendIfLinebreak(std::ostream& os, MarkdownContext const& /*ctx*/,
                       pugi::xml_node node) {
  if (std::string_view{node.name()} != "linebreak") return false;
  os << "<br>";
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
//     <xsd:element name="para" type="docEmptyType" />
// ... upper case greek letters ...
// ... lower case greek letters ...
//   </xsd:choice>
// </xsd:group>
bool AppendIfDocTitleCmdGroup(std::ostream& os, MarkdownContext const& ctx,
                              pugi::xml_node node) {
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
  if (AppendIfLinebreak(os, ctx, node)) return true;
  // Unexpected: nonbreakablespace
  // Unexpected: many many symbols
  if (AppendIfNDash(os, ctx, node)) return true;
  if (AppendIfParagraph(os, ctx, node)) return true;
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
                         pugi::xml_node node) {
  auto const name = std::string_view{node.name()};
  // <parameterlist> is part of the detailed description for functions. In
  // DocFX YAML the parameters get their own YAML elements, and do not need
  // to be documented in the markdown too.
  if (name == "parameterlist") return true;
  if (AppendIfDocTitleCmdGroup(os, ctx, node)) return true;
  // Unexpected: hruler, preformatted
  if (AppendIfProgramListing(os, ctx, node)) return true;
  // Unexpected: indexentry
  if (AppendIfVerbatim(os, ctx, node)) return true;
  if (AppendIfOrderedList(os, ctx, node)) return true;
  if (AppendIfItemizedList(os, ctx, node)) return true;
  if (AppendIfSimpleSect(os, ctx, node)) return true;
  // Unexpected: title
  if (AppendIfVariableList(os, ctx, node)) return true;
  if (AppendIfTable(os, ctx, node)) return true;
  // Unexpected: header, dotfile, mscfile, diafile, toclist, language
  if (AppendIfXRefSect(os, ctx, node)) return true;
  // Unexpected: copydoc, blockquote
  if (AppendIfParBlock(os, ctx, node)) return true;
  // zero-width joiner, just ignore them.
  if (name == "zwj") return true;
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
                       pugi::xml_node node) {
  if (std::string_view{node.name()} != "para") return false;
  os << ctx.paragraph_start << ctx.paragraph_indent;
  auto nested = ctx;
  for (auto const child : node) {
    // After the first successful item we need to insert a blank line before
    // each additional item.
    if (AppendIfDocCmdGroup(os, nested, child)) {
      nested.paragraph_start = "\n\n";
      continue;
    }
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
                            pugi::xml_node node) {
  if (std::string_view{node.name()} != "programlisting") return false;
  os << ctx.paragraph_start << ctx.paragraph_indent << "```cpp";
  for (auto const child : node) {
    if (AppendIfCodeline(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << "\n" << ctx.paragraph_indent << "```";
  return true;
}

// The type for `verbatim` is a simple string.
bool AppendIfVerbatim(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "verbatim") return false;
  os << ctx.paragraph_start << ctx.paragraph_indent << "```\n"
     << ctx.paragraph_indent << node.child_value() << "\n"
     << ctx.paragraph_indent << "```";
  return true;
}

// The type for `parblock` is a sequence of paragraphs.
//
// clang-format off
//   <xsd:complexType name="docParBlockType">
//     <xsd:sequence>
//       <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//   </xsd:complexType>
// clang-format on
bool AppendIfParBlock(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "parblock") return false;
  for (auto const child : node) {
    if (AppendIfParagraph(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The type for `table` is a sequence of rows, maybe with a caption.
//
// clang-format off
//   <xsd:complexType name="docTableType">
//     <xsd:sequence>
//       <xsd:element name="caption" type="docCaptionType" minOccurs="0" maxOccurs="1" />
//       <xsd:element name="row" type="docRowType" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//     <xsd:attribute name="rows" type="xsd:integer" />
//     <xsd:attribute name="cols" type="xsd:integer" />
//     <xsd:attribute name="width" type="xsd:string" />
//   </xsd:complexType>
// clang-format on
bool AppendIfTable(std::ostream& os, MarkdownContext const& ctx,
                   pugi::xml_node node) {
  if (std::string_view{node.name()} != "table") return false;
  for (auto const child : node) {
    if (AppendIfTableRow(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

// The type for `row` is a sequence of `<entry>` elements.
//
// clang-format off
//    <xsd:complexType name="docRowType">
//      <xsd:sequence>
//        <xsd:element name="entry" type="docEntryType" minOccurs="0" maxOccurs="unbounded" />
//      </xsd:sequence>
//    </xsd:complexType>
// clang-format on
bool AppendIfTableRow(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "row") return false;
  os << "\n" << ctx.paragraph_indent;
  auto nested = ctx;
  nested.paragraph_indent = "";
  nested.paragraph_start = "| ";
  for (auto const child : node) {
    if (AppendIfTableEntry(os, nested, child)) {
      nested.paragraph_start = " | ";
      continue;
    }
    UnknownChildType(__func__, child);
  }
  os << " |";
  // This may not work for tables with colspan, but it is good enough for the
  // documents we have in `google-cloud-cpp`.
  auto nheaders =
      std::count_if(node.begin(), node.end(), [](pugi::xml_node child) {
        return std::string_view{child.name()} == "entry" &&
               std::string_view{child.attribute("thead").as_string()} == "yes";
      });
  if (nheaders != 0) {
    os << "\n" << ctx.paragraph_indent;
    for (; nheaders != 0; --nheaders) {
      os << "| ---- ";
    }
    os << "|";
  }
  return true;
}

// The type for a `<entry>` element is a sequence of `<para>` elements, maybe
// with some attributes. We will ignore most of the attributes for now.
//
// clang-format off
//   <xsd:complexType name="docEntryType">
//      <xsd:sequence>
//        <xsd:element name="para" type="docParaType" minOccurs="0" maxOccurs="unbounded" />
//      </xsd:sequence>
//      <xsd:attribute name="thead" type="DoxBool" />
//      <xsd:attribute name="colspan" type="xsd:integer" />
//      <xsd:attribute name="rowspan" type="xsd:integer" />
//      <xsd:attribute name="align" type="DoxAlign" />
//      <xsd:attribute name="valign" type="DoxVerticalAlign" />
//      <xsd:attribute name="width" type="xsd:string" />
//      <xsd:attribute name="class" type="xsd:string" />
//      <xsd:anyAttribute processContents="skip"/>
//    </xsd:complexType>
// clang-format on
bool AppendIfTableEntry(std::ostream& os, MarkdownContext const& ctx,
                        pugi::xml_node node) {
  if (std::string_view{node.name()} != "entry") return false;
  auto nested = ctx;
  for (auto const child : node) {
    if (AppendIfParagraph(os, nested, child)) {
      nested.paragraph_start = " ";
      continue;
    }
    UnknownChildType(__func__, child);
  }
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
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "codeline") return false;
  os << "\n" << ctx.paragraph_indent;
  for (auto const child : node) {
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
                       pugi::xml_node node) {
  if (std::string_view{node.name()} != "highlight") return false;
  for (auto const child : node) {
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
                         pugi::xml_node node) {
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
                          pugi::xml_node node) {
  if (std::string_view{node.name()} != "ref") return false;
  for (auto const child : node) {
    if (AppendIfDocTitleCmdGroup(os, ctx, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

/// Handle `orderedlist` elements.
bool AppendIfOrderedList(std::ostream& os, MarkdownContext const& ctx,
                         pugi::xml_node node) {
  if (std::string_view{node.name()} != "orderedlist") return false;
  auto nested = ctx;
  nested.paragraph_indent = std::string(ctx.paragraph_indent.size(), ' ');
  nested.item_prefix = "1. ";
  for (auto const child : node) {
    if (AppendIfListItem(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

bool AppendIfItemizedList(std::ostream& os, MarkdownContext const& ctx,
                          pugi::xml_node node) {
  if (std::string_view{node.name()} != "itemizedlist") return false;
  auto nested = ctx;
  nested.paragraph_indent = std::string(ctx.paragraph_indent.size(), ' ');
  nested.item_prefix = "- ";
  for (auto const child : node) {
    if (AppendIfListItem(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  return true;
}

bool AppendIfListItem(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node node) {
  if (std::string_view{node.name()} != "listitem") return false;
  // The first paragraph is the list item is indented as needed, and starts
  // with the item prefix (typically "- " or "1. ").
  auto nested = ctx;
  nested.paragraph_start = "\n";
  nested.paragraph_indent = ctx.paragraph_indent + ctx.item_prefix;
  for (auto const child : node) {
    if (AppendIfParagraph(os, nested, child)) {
      // Subsequent paragraphs within the same list item require a blank line
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
                          pugi::xml_node node) {
  if (std::string_view{node.name()} != "variablelist") return false;

  auto nested = ctx;
  nested.paragraph_start = "\n";
  nested.paragraph_indent = ctx.paragraph_indent + "- ";
  for (auto const child : node) {
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
                               pugi::xml_node node) {
  if (std::string_view{node.name()} != "varlistentry") return false;
  auto const term = node.child("term");
  if (!term) MissingElement(__func__, "term", node);
  os << ctx.paragraph_start << ctx.paragraph_indent;
  for (auto const child : term) {
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
                              pugi::xml_node node) {
  if (std::string_view{node.name()} != "listitem") return false;
  for (auto const child : node) {
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
                        pugi::xml_node node) {
  if (std::string_view{node.name()} != "simplesect") return false;
  static auto const* const kUseH6 = [] {
    return new std::unordered_set<std::string>{
        "author", "authors",   "version",   "since", "date", "pre",
        "post",   "copyright", "invariant", "par",   "rcs",
    };
  }();

  auto nested = ctx;
  nested.paragraph_start = "\n";

  auto const kind = std::string{node.attribute("kind").as_string()};
  // In DocFX YAML the return description and type are captured as
  // separate YAML elements. Including them in the markdown section would
  // just repeat the text.
  if (kind == "return") return true;

  auto closing = std::string_view{};
  if (kind == "see") {
    nested = ctx;
    os << "\n\n###### See Also";
    nested.paragraph_start = "\n\n";
  } else if (kUseH6->count(kind) != 0) {
    nested = ctx;
    os << "\n\n###### ";
    AppendTitle(os, nested, node);
    nested.paragraph_start = "\n\n";
  } else if (kind == "note") {
    os << "\n"
       << nested.paragraph_start << nested.paragraph_indent
       << R"""(<aside class="note"><b>Note:</b>)""";
    closing = "\n</aside>";
  } else if (kind == "remark") {
    // Use `note` class because the GCP side does not have something that
    // strictly matches "remark".
    os << "\n"
       << nested.paragraph_start << nested.paragraph_indent
       << R"""(<aside class="note"><b>Remark:</b>)""";
    closing = "\n</aside>";
  } else if (kind == "warning") {
    os << "\n"
       << nested.paragraph_start << nested.paragraph_indent
       << R"""(<aside class="warning"><b>Warning:</b>)""";
    closing = "\n</aside>";
  } else if (kind == "attention") {
    os << "\n"
       << nested.paragraph_start << nested.paragraph_indent
       << R"""(<aside class="caution"><b>Attention:</b>)""";
    closing = "\n</aside>";
  } else {
    std::ostringstream os;
    os << "Unknown simplesect kind in " << __func__ << "(): node=";
    node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
    throw std::runtime_error(std::move(os).str());
  }

  for (auto const child : node) {
    if (std::string_view{child.name()} == "title") continue;
    if (AppendIfParagraph(os, nested, child)) continue;
    UnknownChildType(__func__, child);
  }
  os << closing;
  return true;
}

bool AppendIfAnchor(std::ostream& /*os*/, MarkdownContext const& /*ctx*/,
                    pugi::xml_node node) {
  // Do not generate any code for anchors, they have no obvious mapping to
  // Markdown.
  return std::string_view{node.name()} == "anchor";
}

void AppendTitle(std::ostream& os, MarkdownContext const& ctx,
                 pugi::xml_node node) {
  // The XML schema says there is only one of these, but it is easier to write
  // the loop.
  for (auto const title : node.children("title")) {
    for (auto const child : title) {
      if (AppendIfPlainText(os, ctx, child)) continue;
      UnknownChildType(__func__, child);
    }
  }
}

}  // namespace docfx
