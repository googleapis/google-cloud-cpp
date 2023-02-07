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
#include <gmock/gmock.h>
#include <sstream>

namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(Doxygen2Markdown, PlainText) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id="plain-text">test-only-value 42</para>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='plain-text']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfPlainText(os, *selected.node().children().begin()));
  EXPECT_EQ(os.str(), "test-only-value 42");
}

TEST(Doxygen2Markdown, ComputerOutput) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <computeroutput id="test-node">int f() { return 42; }</computeroutput>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfComputerOutput(os, selected.node()));
  EXPECT_EQ(os.str(), "`int f() { return 42; }`");
}

TEST(Doxygen2Markdown, Paragraph) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>Try using <computeroutput id="test-node">int f() { return 42; }</computeroutput> in your code.</para>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, selected.node()));
  EXPECT_EQ(os.str(), "Try using `int f() { return 42; }` in your code.\n");
}

TEST(Doxygen2Markdown, ParagraphWithUnknown) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>Uh oh: <itemizedlist></itemizedlist></para>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  EXPECT_THROW(AppendIfParagraph(os, selected.node()), std::runtime_error);
}

TEST(Doxygen2Markdown, ParagraphWithUnknownOutput) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>Uh oh:
          <itemizedlist a1="attr1" a2="attr2">
            <listitem>1</listitem>
            <listitem>2</listitem>
          </itemizedlist></para>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  EXPECT_THROW(
      try {
        AppendIfParagraph(os, selected.node());
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), Not(HasSubstr("\n")));
        EXPECT_THAT(ex.what(),
                    HasSubstr("<itemizedlist a1=\"attr1\" a2=\"attr2\">"));
        EXPECT_THAT(ex.what(), HasSubstr("<listitem>1</listitem>"));
        EXPECT_THAT(ex.what(), HasSubstr("<listitem>2</listitem>"));
        EXPECT_THAT(ex.what(), HasSubstr("</itemizedlist>"));
        throw;
      },
      std::runtime_error);
}

}  // namespace
