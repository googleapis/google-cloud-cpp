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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2MARKDOWN_H
#define GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2MARKDOWN_H

#include <pugixml.hpp>
#include <iosfwd>

/**
 * Keeps the state for markdown generation.
 *
 * As we recurse through the XML tree, we need to keep some information to
 * generate valid Markdown text.
 */
struct MarkdownContext {
  std::string paragraph_start = "\n\n";
  std::string paragraph_indent = "";
};

/**
 * Handle plain text nodes.
 *
 * pugixml adds such nodes to represent the text between the markups. For
 * example, something like:
 *
 * <foo>Some text<bar>more stuff</bar>hopefully the end</foo>
 *
 * Would have a node for "foo", which 3 children:
 * 1. The first child is plain text, with value "Some text".
 * 2. The second child has name "bar", and contains another child with
 *     one with value "more stuff".
 * 3. Finally, the third child has value "hopefully the end", and it is also
 *    plain text.
 *
 * This is rather convenient when converting the XML nodes to the markdown
 * representation.
 */
bool AppendIfPlainText(std::ostream& os, MarkdownContext const& ctx,
                       pugi::xml_node const& node);

/// Handles nodes with **bold** text.
bool AppendIfBold(std::ostream& os, MarkdownContext const& ctx,
                  pugi::xml_node const& node);

/// Handles nodes with ~strike through~ text.
bool AppendIfStrike(std::ostream& os, MarkdownContext const& ctx,
                    pugi::xml_node const& node);

/// Handles *emphasis* in text.
bool AppendIfEmphasis(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node const& node);

/// Handles nodes with `computer output`.
bool AppendIfComputerOutput(std::ostream& os, MarkdownContext const& ctx,
                            pugi::xml_node const& node);

/// Part of the implementation of AppendIfParagraph().
bool AppendIfDocTitleCmdGroup(std::ostream& os, MarkdownContext const& ctx,
                              pugi::xml_node const& node);

/// Part of the implementation of AppendIfParagraph().
bool AppendIfDocCmdGroup(std::ostream& os, MarkdownContext const& ctx,
                         pugi::xml_node const& node);

/// Handle full paragraphs.
bool AppendIfParagraph(std::ostream& os, MarkdownContext const& ctx,
                       pugi::xml_node const& node);

/// Handle itemized lists
bool AppendIfItemizedList(std::ostream& os, MarkdownContext const& ctx,
                          pugi::xml_node const& node);

/// Handle a single list item.
bool AppendIfListItem(std::ostream& os, MarkdownContext const& ctx,
                      pugi::xml_node const& node);

/// Handle a `simplesect` element (a section without sub-sections).
bool AppendIfSimpleSect(std::ostream& os, MarkdownContext const& ctx,
                        pugi::xml_node const& node);

/// Handle the title for a section-like element.
void AppendTitle(std::ostream& os, MarkdownContext const& ctx,
                 pugi::xml_node const& node);

#endif  // GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2MARKDOWN_H
