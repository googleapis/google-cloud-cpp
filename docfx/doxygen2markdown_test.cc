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

namespace docfx {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

TEST(Doxygen2Markdown, Sect4) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <sect4 id='tested-node'>
          <title>This is the title</title>
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
        </sect4>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

##### This is the title

First paragraph.

Second paragraph.)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfSect4(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, Sect3) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <sect3 id='tested-node'>
          <title>This is the title</title>
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
          <sect4><title>This is a section4</title><para>Sect4 paragraph.</para></sect4>
        </sect3>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

#### This is the title

First paragraph.

Second paragraph.

##### This is a section4

Sect4 paragraph.)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfSect3(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, Sect2) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <sect2 id='tested-node'>
          <title>This is the title</title>
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
          <sect3><title>This is a section3</title><para>Sect3 paragraph.</para></sect3>
        </sect2>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

### This is the title

First paragraph.

Second paragraph.

#### This is a section3

Sect3 paragraph.)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfSect2(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, Sect1) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <sect1 id='tested-node'>
          <title>This is the title</title>
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
          <sect2><title>This is a section2</title><para>Sect2 paragraph.</para></sect2>
        </sect1>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

## This is the title

First paragraph.

Second paragraph.

### This is a section2

Sect2 paragraph.)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfSect1(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, DetailedDescription) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <detaileddescription id='tested-node'>
          <sect1>
            <title>This is the title (1)</title>
            <para>First paragraph (1).</para>
            <para>Second paragraph (1).</para>
            <sect2><title>This is a section2</title><para>Sect2 paragraph.</para></sect2>
          </sect1>
          <sect1>
            <title>This is the title (2)</title>
            <para>First paragraph (2).</para>
            <para>Second paragraph (2).</para>
          </sect1>
        </detaileddescription>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

## This is the title (1)

First paragraph (1).

Second paragraph (1).

### This is a section2

Sect2 paragraph.

## This is the title (2)

First paragraph (2).

Second paragraph (2).)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfDetailedDescription(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, DetailedDescriptionSkipSect1) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <detaileddescription id='tested-node'>
          <sect2>
            <title>This is the title (2)</title>
            <para>First paragraph (2).</para>
            <para>Second paragraph (2).</para>
          </sect2>
          <sect3>
            <title>This is the title (3)</title>
            <para>First paragraph (3).</para>
            <para>Second paragraph (3).</para>
          </sect3>
          <sect4>
            <title>This is the title (4)</title>
            <para>First paragraph (4).</para>
            <para>Second paragraph (4).</para>
          </sect4>
        </detaileddescription>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

### This is the title (2)

First paragraph (2).

Second paragraph (2).

#### This is the title (3)

First paragraph (3).

Second paragraph (3).

##### This is the title (4)

First paragraph (4).

Second paragraph (4).)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfDetailedDescription(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, DetailedDescriptionNotSkipped) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud_1_1kms" kind="namespace" language="C++">
          <compoundname>google::cloud::kms</compoundname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
            <para><xrefsect id="deprecated_1_deprecated000001"><xreftitle>Deprecated</xreftitle><xrefdescription><para>This namespace exists for backwards compatibility. Use the types defined in <ref refid="namespacegoogle_1_1cloud_1_1kms__v1" kindref="compound">kms_v1</ref> instead of the aliases defined in this namespace.</para>
            </xrefdescription></xrefsect></para>
            <para><xrefsect id="deprecated_1_deprecated000014"><xreftitle>Deprecated</xreftitle><xrefdescription><para>This namespace exists for backwards compatibility. Use the types defined in <ref refid="namespacegoogle_1_1cloud_1_1kms__v1" kindref="compound">kms_v1</ref> instead of the aliases defined in this namespace.</para>
            </xrefdescription></xrefsect></para>
          </detaileddescription>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(<aside class="deprecated"><b>Deprecated:</b>
This namespace exists for backwards compatibility. Use the types defined in [kms_v1](xref:namespacegoogle_1_1cloud_1_1kms__v1) instead of the aliases defined in this namespace.
</aside>

<aside class="deprecated"><b>Deprecated:</b>


This namespace exists for backwards compatibility. Use the types defined in [kms_v1](xref:namespacegoogle_1_1cloud_1_1kms__v1) instead of the aliases defined in this namespace.
</aside>)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//detaileddescription");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  MarkdownContext ctx;
  ctx.paragraph_start = "";
  EXPECT_TRUE(AppendIfDetailedDescription(os, ctx, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, DetailedDescriptionSkipped) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud_1_1kms" kind="namespace" language="C++">
          <compoundname>google::cloud::kms</compoundname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
            <para><xrefsect id="deprecated_1_deprecated000001"><xreftitle>Deprecated</xreftitle><xrefdescription><para>This namespace exists for backwards compatibility. Use the types defined in <ref refid="namespacegoogle_1_1cloud_1_1kms__v1" kindref="compound">kms_v1</ref> instead of the aliases defined in this namespace.</para>
</xrefdescription></xrefsect></para>
            <para><xrefsect id="deprecated_1_deprecated000014"><xreftitle>Deprecated</xreftitle><xrefdescription><para>This namespace exists for backwards compatibility. Use the types defined in <ref refid="namespacegoogle_1_1cloud_1_1kms__v1" kindref="compound">kms_v1</ref> instead of the aliases defined in this namespace.</para>
</xrefdescription></xrefsect></para>
          </detaileddescription>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//detaileddescription");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  MarkdownContext ctx;
  ctx.paragraph_start = "";
  ctx.skip_xrefsect = true;
  EXPECT_TRUE(AppendIfDetailedDescription(os, ctx, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, BriefDescription) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <briefdescription id='tested-node'>
          <para>This is <bold>not</bold> too detailed.</para>
        </briefdescription>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

This is **not** too detailed.)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfBriefDescription(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

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

TEST(Doxygen2Markdown, ULink) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <ulink id="test-node" url="https://example.com/">some text</ulink>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfULink(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "[some text](https://example.com/)");
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

TEST(Doxygen2Markdown, BoldComplex) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <bold id="test-node"><ref refid="group__test" kindref="compound">Some Text</ref></bold>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfBold(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "[**Some Text**](xref:group__test)");
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

TEST(Doxygen2Markdown, StrikeComplex) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <strike id="test-node"><ref refid="group__test" kindref="compound">Some Text</ref></strike>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfStrike(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "[~Some Text~](xref:group__test)");
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

TEST(Doxygen2Markdown, EmphasisComplex) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <emphasis id="test-node"><ref refid="group__test" kindref="compound">Some Text</ref></emphasis>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfEmphasis(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "[*Some Text*](xref:group__test)");
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

TEST(Doxygen2Markdown, ComputerOutputComplex) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <computeroutput id="test-node"><ref refid="group__test" kindref="compound">Some Text</ref></computeroutput>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfComputerOutput(os, {}, selected.node()));
  EXPECT_EQ(os.str(), "[`Some Text`](xref:group__test)");
}

TEST(Doxygen2Markdown, RefExternal) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <ref id="test-node" external="/workspace/google/cloud/cloud.tag" refid="classgoogle_1_1cloud_1_1StatusOr">Reference Text</ref>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfRef(os, {}, selected.node()));
  EXPECT_EQ("[Reference Text](xref:classgoogle_1_1cloud_1_1StatusOr)",
            os.str());
}

TEST(Doxygen2Markdown, RefInternal) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <ref id="test-node" refid="some_id">Reference Text</ref>
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfRef(os, {}, selected.node()));
  EXPECT_EQ("[Reference Text](xref:some_id)", os.str());
}

TEST(Doxygen2Markdown, NDash) {
  pugi::xml_document doc;
  doc.load_string(R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <ndash id="test-node" />
    </doxygen>)xml");
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfNDash(os, {}, selected.node()));
  EXPECT_EQ("&ndash;", os.str());
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

TEST(Doxygen2Markdown, ParagraphSimpleContents) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-000'>The answer is 42.</para>
        <para id='test-001'><bold>The answer is 42.</bold></para>
        <para id='test-002'><strike>The answer is 42.</strike></para>
        <para id='test-003'><emphasis>The answer is 42.</emphasis></para>
        <para id='test-004'><computeroutput>The answer is 42.</computeroutput></para>
        <para id='test-005'><ref refid="test_id">The answer is 42.</ref></para>
        <para id='test-006'><ulink url="https://example.com/">The answer is 42.</ulink></para>
        <para id='test-007'><ndash/></para>
        <para id='test-008'><ref refid="group__guac" kindref="compound">Authentication Components</ref></para>
        <para id='test-009'><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound">google::cloud::Options</ref></para>
        <para id='test-010'><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound">Options</ref></para>
        <para id='test-011'>abc<zwj/>123</para>
        <para id='test-012'><ulink url="https://example.com/">google::cloud::Test</ulink></para>
        <para id='test-013'><computeroutput>projects/*&zwj;/secrets/*&zwj;/versions/*</computeroutput></para>
    </doxygen>)xml";

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"test-000", "\n\nThe answer is 42."},
      {"test-001", "\n\n**The answer is 42.**"},
      {"test-002", "\n\n~The answer is 42.~"},
      {"test-003", "\n\n*The answer is 42.*"},
      {"test-004", "\n\n`The answer is 42.`"},
      {"test-005", "\n\n[The answer is 42.](xref:test_id)"},
      {"test-006", "\n\n[The answer is 42.](https://example.com/)"},
      {"test-007", "\n\n&ndash;"},
      {"test-008", "\n\n[Authentication Components](xref:group__guac)"},
      {"test-009",
       "\n\n[`google::cloud::Options`](xref:classgoogle_1_1cloud_1_1Options)"},
      {"test-010", "\n\n[Options](xref:classgoogle_1_1cloud_1_1Options)"},
      {"test-011", "\n\nabc123"},
      {"test-012", "\n\n[`google::cloud::Test`](https://example.com/)"},
      {"test-013", "\n\n`projects/*/secrets/*/versions/*`"},
  };

  pugi::xml_document doc;
  doc.load_string(kXml);
  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with id=" + test.id + ", expected=" + test.expected);
    auto selected = doc.select_node(("//*[@id='" + test.id + "']").c_str());
    std::ostringstream os;
    ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
    EXPECT_EQ(test.expected, os.str());
  }
}

TEST(Doxygen2Markdown, ParagraphSimpleSect) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <simplesect kind="remark">
            <para>First remark paragraph.</para>
            <para>Second remark paragraph.</para>
          </simplesect>
          <simplesect kind="warning">
            <para>First warning paragraph.</para>
            <para>Second warning paragraph.</para>
          </simplesect>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(



<aside class="note"><b>Remark:</b>
First remark paragraph.
Second remark paragraph.
</aside>

<aside class="warning"><b>Warning:</b>
First warning paragraph.
Second warning paragraph.
</aside>)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphOrderedList) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <orderedlist>
            <listitem><para>First item.</para></listitem>
            <listitem>
              <para>Second item.</para><para>With a second paragraph.</para>
            </listitem>
          </orderedlist>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(


1. First item.
1. Second item.

   With a second paragraph.)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphItemizedList) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <itemizedlist>
            <listitem><para>First item.</para></listitem>
            <listitem>
              <para>Second item.</para><para>With a second paragraph.</para>
            </listitem>
          </itemizedlist>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(


- First item.
- Second item.

  With a second paragraph.)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphProgramListing) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <programlisting><codeline><highlight class="normal">auto<sp/>sr<sp/>=<sp/>MakeStreamRange&lt;T&gt;({t1,<sp/>t2});</highlight></codeline>
          <codeline><highlight class="normal">for<sp/>(StatusOr&lt;int&gt;<sp/>const&amp;<sp/>v<sp/>:<sp/>sr)<sp/>{</highlight></codeline>
          <codeline><highlight class="normal"><sp/><sp/>//<sp/>Yields<sp/>t1<sp/>-&gt;<sp/>t2</highlight></codeline>
          <codeline><highlight class="normal">}</highlight></codeline>
          <codeline/>
          <codeline><highlight class="normal">sr<sp/>=<sp/>MakeStreamRange&lt;T&gt;({t1,<sp/>t2},<sp/>BadStatus());</highlight></codeline>
          <codeline><highlight class="normal">for<sp/>(StatusOr&lt;int&gt;<sp/>const&amp;<sp/>v<sp/>:<sp/>sr)<sp/>{</highlight></codeline>
          <codeline><highlight class="normal"><sp/><sp/>//<sp/>Yields<sp/>t1<sp/>-&gt;<sp/>t2<sp/>-&gt;<sp/>BadStatus()</highlight></codeline>
          <codeline><highlight class="normal">}</highlight></codeline>
          </programlisting>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(



```cpp
auto sr = MakeStreamRange<T>({t1, t2});
for (StatusOr<int> const& v : sr) {
  // Yields t1 -> t2
}

sr = MakeStreamRange<T>({t1, t2}, BadStatus());
for (StatusOr<int> const& v : sr) {
  // Yields t1 -> t2 -> BadStatus()
}
```)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphProgramListingAddsNewLine) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
    <detaileddescription  id='test-node'>
      <para>
        <simplesect kind="par">
          <title>Error Handling</title>
            <para>Description goes here.</para>
        </simplesect>
        <programlisting>
          <codeline><highlight class="keyword">namespace<sp/></highlight><highlight class="normal">cbt<sp/>=<sp/><ref refid="namespacegoogle_1_1cloud_1_1bigtable" kindref="compound">google::cloud::bigtable</ref>;</highlight></codeline>
          <codeline><highlight class="normal"></highlight><highlight class="keyword">namespace<sp/></highlight><highlight class="normal">btadmin<sp/>=<sp/>google::bigtable::admin::v2;</highlight></codeline>
        </programlisting>
      </para>
    </detaileddescription>
 </doxygen>)xml";

  auto constexpr kExpected = R"md(

###### Error Handling

Description goes here.

```cpp
namespace cbt = google::cloud::bigtable;
namespace btadmin = google::bigtable::admin::v2;
```)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  MarkdownContext mdctx;
  mdctx.paragraph_start = "";
  ASSERT_TRUE(AppendIfDetailedDescription(os, mdctx, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphVerbatim) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <verbatim>https://cloud.google.com/storage/docs/transcoding
</verbatim>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(



```
https://cloud.google.com/storage/docs/transcoding

```)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphParBlock) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <parblock>
            <para>First paragraph</para>
            <para>Second paragraph</para>
          </parblock>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(



First paragraph

Second paragraph)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphTable) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <para id='test-node'>
          <table rows="3" cols="2">
            <row>
              <entry thead="yes"><para>Environment Variable</para></entry>
              <entry thead="yes"><para><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound" external="/workspace/cmake-out/google/cloud/cloud.tag">Options</ref> setting</para></entry>
            </row>
            <row>
              <entry thead="no"><para><computeroutput>SPANNER_OPTIMIZER_VERSION</computeroutput></para></entry>
              <entry thead="no"><para><computeroutput><ref refid="structgoogle_1_1cloud_1_1spanner_1_1QueryOptimizerVersionOption" kindref="compound">QueryOptimizerVersionOption</ref></computeroutput></para></entry>
            </row>
            <row>
              <entry thead="no"><para><computeroutput>SPANNER_OPTIMIZER_STATISTICS_PACKAGE</computeroutput></para></entry>
              <entry thead="no">
                <para><computeroutput><ref refid="structgoogle_1_1cloud_1_1spanner_1_1QueryOptimizerStatisticsPackageOption" kindref="compound">QueryOptimizerStatisticsPackageOption</ref></computeroutput></para>
                <para>With another paragraph</para>
              </entry>
            </row>
          </table>
        </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(


| Environment Variable | [Options](xref:classgoogle_1_1cloud_1_1Options) setting |
| ---- | ---- |
| `SPANNER_OPTIMIZER_VERSION` | [`QueryOptimizerVersionOption`](xref:structgoogle_1_1cloud_1_1spanner_1_1QueryOptimizerVersionOption) |
| `SPANNER_OPTIMIZER_STATISTICS_PACKAGE` | [`QueryOptimizerStatisticsPackageOption`](xref:structgoogle_1_1cloud_1_1spanner_1_1QueryOptimizerStatisticsPackageOption) With another paragraph |)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphXrefSect) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <para id='tested-node'>
        <xrefsect id="deprecated_1_deprecated000001">
          <xreftitle>Deprecated</xreftitle>
          <xrefdescription>
            <para>Use <computeroutput><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound">Options</ref></computeroutput> and <computeroutput><ref refid="structgoogle_1_1cloud_1_1EndpointOption" kindref="compound">EndpointOption</ref></computeroutput>.</para>
          </xrefdescription>
        </xrefsect>
      </para>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(<aside class="deprecated"><b>Deprecated:</b>
Use [`Options`](xref:classgoogle_1_1cloud_1_1Options) and [`EndpointOption`](xref:structgoogle_1_1cloud_1_1EndpointOption).
</aside>)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  MarkdownContext ctx;
  ctx.paragraph_start = "";
  ASSERT_TRUE(AppendIfParagraph(os, ctx, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphLinebreak) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
    <para id='tested-node'>Required. Parent resource name. The format of this value varies depending on the scope of the request (project or organization) and whether you have <ulink url="https://cloud.google.com/dlp/docs/specifying-location">specified a processing location</ulink>:<itemizedlist>
        <listitem><para>Projects scope, location specified:<linebreak/>
        <computeroutput>projects/</computeroutput><emphasis>PROJECT_ID</emphasis><computeroutput>/locations/</computeroutput><emphasis>LOCATION_ID</emphasis></para>
        </listitem><listitem><para>Projects scope, no location specified (defaults to global):<linebreak/>
        <computeroutput>projects/</computeroutput><emphasis>PROJECT_ID</emphasis></para>
        </listitem><listitem><para>Organizations scope, location specified:<linebreak/>
        <computeroutput>organizations/</computeroutput><emphasis>ORG_ID</emphasis><computeroutput>/locations/</computeroutput><emphasis>LOCATION_ID</emphasis></para>
        </listitem><listitem><para>Organizations scope, no location specified (defaults to global):<linebreak/>
        <computeroutput>organizations/</computeroutput><emphasis>ORG_ID</emphasis>
        </listitem>
      </itemizedlist>
    </para>
  )xml";
  auto constexpr kExpected = R"md(

Required. Parent resource name. The format of this value varies depending on the scope of the request (project or organization) and whether you have [specified a processing location](https://cloud.google.com/dlp/docs/specifying-location):
- Projects scope, location specified:<br>`projects/`*PROJECT_ID*`/locations/`*LOCATION_ID*
- Projects scope, no location specified (defaults to global):<br>`projects/`*PROJECT_ID*
- Organizations scope, location specified:<br>`organizations/`*ORG_ID*`/locations/`*LOCATION_ID*
- Organizations scope, no location specified (defaults to global):<br>`organizations/`*ORG_ID*)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, ParagraphWithParagraph) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
    <para id='tested-node'>
      <para>
        <simplesect kind="warning">
        <para>Something clever about the warning.</para>
      </para>
    </para>
    </doxygen>)xml";
  auto constexpr kExpected = R"md(





<aside class="warning"><b>Warning:</b>
Something clever about the warning.
</aside>)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='tested-node']");
  ASSERT_TRUE(selected);
  std::ostringstream os;
  ASSERT_TRUE(AppendIfParagraph(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
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

TEST(Doxygen2Markdown, OrderedListWithParagraphs) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <orderedlist id='test-node'>
          <listitem><para>Item 1</para><para>More about Item 1</para></listitem>
          <listitem><para>Item 2: <computeroutput>brrr</computeroutput></para></listitem>
        </orderedlist>
    </doxygen>)xml";
  auto constexpr kExpected = R"md(
1. Item 1

   More about Item 1
1. Item 2: `brrr`)md";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfOrderedList(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, VariableListSimple) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <variablelist id='test-node'>
          <varlistentry><term>Class <ref refid="classgoogle_1_1cloud_1_1ConnectionOptions" kindref="compound">google::cloud::ConnectionOptions&lt; ConnectionTraits &gt;</ref>  </term></varlistentry>
          <listitem><para>Use <computeroutput><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound">Options</ref></computeroutput> instead.</para></listitem>
          <varlistentry><term>Member <ref refid="test_only" kindref="member">TestClass::test</ref>(std::string prefix)</term></varlistentry>
          <listitem><para>Use <ref refid="test_ref" kindref="compound">Options</ref> instead.</para></listitem>
        </variablelist>
    </doxygen>)xml";
  auto constexpr kExpected = R"md(
- Class [`google::cloud::ConnectionOptions< ConnectionTraits >`](xref:classgoogle_1_1cloud_1_1ConnectionOptions)

  Use [`Options`](xref:classgoogle_1_1cloud_1_1Options) instead.
- Member [`TestClass::test`](xref:test_only)(std::string prefix)

  Use [Options](xref:test_ref) instead.)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfVariableList(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
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
      "author", "authors",   "version",   "since", "date", "pre",
      "post",   "copyright", "invariant", "par",   "rcs",
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

TEST(Doxygen2Markdown, SimpleSectSeeAlso) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <simplesect id='test-node' kind="see">
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
        </simplesect>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

###### See Also

First paragraph.

Second paragraph.)md";

  pugi::xml_document doc;
  doc.load_string(kXml);

  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfSimpleSect(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
}

TEST(Doxygen2Markdown, SimpleSectSeeAlsoContext) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <simplesect id='test-node' kind="see">
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
        </simplesect>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(

###### See Also

First paragraph.

Second paragraph.)md";

  pugi::xml_document doc;
  doc.load_string(kXml);

  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  MarkdownContext ctx;
  ctx.paragraph_start = "";
  ASSERT_TRUE(AppendIfSimpleSect(os, ctx, selected.node()));
  EXPECT_EQ(kExpected, os.str());
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
First paragraph.
Second paragraph.
</aside>)md";

  struct TestCase {
    std::string kind;
    std::string header;
  } const cases[] = {
      {"warning", "<aside class=\"warning\"><b>Warning:</b>"},
      {"note", "<aside class=\"note\"><b>Note:</b>"},
      {"remark", "<aside class=\"note\"><b>Remark:</b>"},
      {"attention", "<aside class=\"caution\"><b>Attention:</b>"},
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

TEST(Doxygen2Markdown, SimpleSectReturn) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <simplesect id='test-node' kind='return'>
          <para>First paragraph.</para>
          <para>Second paragraph.</para>
        </simplesect>
    </doxygen>)xml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfSimpleSect(os, {}, selected.node()));
  EXPECT_THAT(os.str(), IsEmpty());
}

TEST(Doxygen2Markdown, Anchor) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <anchor id="test-node"/>
    </doxygen>)xml";

  auto constexpr kExpected = R"md()md";
  pugi::xml_document doc;
  doc.load_string(kXml);

  auto selected = doc.select_node("//*[@id='test-node']");
  std::ostringstream os;
  ASSERT_TRUE(AppendIfAnchor(os, {}, selected.node()));
  EXPECT_EQ(kExpected, os.str());
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
}  // namespace docfx
