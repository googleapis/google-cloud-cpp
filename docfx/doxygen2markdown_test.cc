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
  ASSERT_TRUE(AppendIfPlainText(os, {}, *selected.node().children().begin()));
  EXPECT_EQ(os.str(), "test-only-value 42");
}

TEST(Doxygen2Markdown, Bold) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <bold id="test-node">some text</bold>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfBold(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "**some text**");
}

TEST(Doxygen2Markdown, Strike) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <strike id="test-node">some text</strike>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfStrike(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "~some text~");
}

TEST(Doxygen2Markdown, Emphasis) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <emphasis id="test-node">some text</emphasis>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfEmphasis(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "*some text*");
}

TEST(Doxygen2Markdown, ComputerOutput) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <computeroutput id="test-node">int f() { return 42; }</computeroutput>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfComputerOutput(os, {}, selected.node()));
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
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "\n\nTry using `int f() { return 42; }` in your code.");
}

TEST(Doxygen2Markdown, ParagraphWithUnknown) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>Uh oh: <unknown></unknown></para>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  EXPECT_THROW(AppendIfParagraph(os, {}, selected.node()), std::runtime_error);
}

TEST(Doxygen2Markdown, ParagraphWithUnknownOutput) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>Uh oh:
          <unknown a1="attr1" a2="attr2">
            <child>1</child><child>2</child>
          </unknown></para>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  EXPECT_THROW(
      try {
        AppendIfParagraph(os, {}, selected.node());
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), Not(HasSubstr("\n")));
        EXPECT_THAT(ex.what(),
                    HasSubstr("<unknown a1=\"attr1\" a2=\"attr2\">"));
        EXPECT_THAT(ex.what(), HasSubstr("<child>1</child>"));
        EXPECT_THAT(ex.what(), HasSubstr("<child>2</child>"));
        EXPECT_THAT(ex.what(), HasSubstr("</unknown>"));
        throw;
      },
      std::runtime_error);
}

TEST(Doxygen2Markdown, ItemizedListSimple) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <itemizedlist id='test-node'>
        <listitem><para>Item 1</para></listitem>
        <listitem><para>Item 2: <computeroutput>brrr</computeroutput></para></listitem>
        </itemizedlist>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfItemizedList(os, {}, selected.node()));
  EXPECT_EQ(os.str(), R"md(
- Item 1
- Item 2: `brrr`)md");
}

TEST(Doxygen2Markdown, ItemizedListSimpleWithParagraphs) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <itemizedlist id='test-node'>
        <listitem><para>Item 1</para><para>More about Item 1</para></listitem>
        <listitem><para>Item 2: <computeroutput>brrr</computeroutput></para></listitem>
        </itemizedlist>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfItemizedList(os, {}, selected.node()));
  EXPECT_EQ(os.str(), R"md(
- Item 1

  More about Item 1
- Item 2: `brrr`)md");
}

TEST(Doxygen2Markdown, ItemizedListNested) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <itemizedlist id='test-node'>
        <listitem><para>Item 1</para><para>More about Item 1</para></listitem>
        <listitem><para>Item 2: <computeroutput>brrr</computeroutput>
          <itemizedlist>
            <listitem>
              <para>Sub 1</para>
            </listitem>
            <listitem><para>Sub 2</para>
              <para>More about Sub 2<itemizedlist>
                  <listitem><para>Sub 2.1</para></listitem>
                  <listitem><para>Sub 2.2</para><para>More about Sub 2.2</para></listitem>
                </itemizedlist>
               </para>
            </listitem>
            <listitem><para>Sub 3</para></listitem>
          </itemizedlist></para>
        </listitem>
        </itemizedlist>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfItemizedList(os, {}, selected.node()));
  EXPECT_EQ(os.str(), R"md(
- Item 1

  More about Item 1
- Item 2: `brrr`
  - Sub 1
  - Sub 2

    More about Sub 2
    - Sub 2.1
    - Sub 2.2

      More about Sub 2.2
  - Sub 3)md");
}

TEST(Doxygen2Markdown, SimpleSectH6) {
  auto constexpr kXmlPrefix = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">)xml";
  auto constexpr kXmlSuffix = R"xml(
          <title>This is the title</title>
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
        </simplesect>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

###### This is the title

First paragraph.

Second paragraph.)md";

  auto const cases = std::vector<std::string>{
      "see", "return", "author",    "authors",   "version", "since", "date",
      "pre", "post",   "copyright", "invariant", "par",     "rcs",
  };

  for (auto const& kind : cases) {
    SCOPED_TRACE("Testing with kind=" + kind);

    pugi::xml_document doc;
    auto xml = std::string(kXmlPrefix) + "<simplesect id='test-node' kind='" +
               kind + "'>" + kXmlSuffix;
    doc.load_string(xml.c_str());

    auto selected = doc.select_node("//*[@id='test-node']");
    std::ostringstream os;
    ASSERT_TRUE(AppendIfSimpleSect(os, {}, selected.node()));
    EXPECT_EQ(kExpected, os.str());
  }
}

TEST(Doxygen2Markdown, SimpleSectBlockQuote) {
  auto constexpr kXmlPrefix = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">)xml";
  auto constexpr kXmlSuffix = R"xml(
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
        </simplesect>
    </doxygen>)xml";

  auto constexpr kExpectedBody = R"md(
> First paragraph.
> Second paragraph.)md";

  struct TestCase {
    std::string kind;
    std::string header;
  } const cases[] = {
      {"warning", "> **Warning:**"},
      {"note", "> **Note:**"},
      {"remark", "> Remark:"},
      {"attention", "> Attention:"},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with kind=" + test.kind);

    pugi::xml_document doc;
    auto xml = std::string(kXmlPrefix) + "<simplesect id='test-node' kind='" +
               test.kind + "'>" + kXmlSuffix;
    doc.load_string(xml.c_str());

    auto selected = doc.select_node("//*[@id='test-node']");
    std::ostringstream os;
    ASSERT_TRUE(AppendIfSimpleSect(os, {}, selected.node()));
    auto const expected = std::string("\n\n") + test.header + kExpectedBody;
    EXPECT_EQ(expected, os.str());
  }
}

TEST(Doxygen2Markdown, Title) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <simplesect id='test-node'>
          <title>This is the title</title>
          <para>unused</para>
        </simplesect>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(This is the title)md";
  pugi::xml_document doc;
  doc.load_string(kXml);

  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  AppendTitle(os, {}, selected.node());
  EXPECT_EQ(kExpected, os.str());
}

}  // namespace
