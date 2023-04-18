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
#include <gmock/gmock.h>

namespace docfx {
namespace {

using ::testing::ElementsAre;
using ::testing::FieldsAre;

TEST(DoxygenGroups, CommonPage) {
  auto constexpr kXml =
      R"xml(<?xml version="1.0" standalone="yes"?><doxygen version="1.9.1" xml:lang="en-US">
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__terminate" kind="group">
    <compoundname>terminate</compoundname>
    <title>Intercepting Unrecoverable Errors</title>
      <sectiondef kind="typedef">
      <memberdef kind="typedef" id="group__terminate_1gacc215b41a0bf17a7ea762fd5bb205348" prot="public" static="no">
        <type>std::function&lt; void(char const  *msg)&gt;</type>
        <definition>using google::cloud::TerminateHandler = typedef std::function&lt;void(char const* msg)&gt;</definition>
        <argsstring/>
        <name>TerminateHandler</name>
        <qualifiedname>google::cloud::TerminateHandler</qualifiedname>
        <briefdescription>
<para>Terminate handler. </para>
        </briefdescription>
        <detaileddescription>
<para>It should handle the error, whose description are given in <emphasis>msg</emphasis> and should never return. </para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="terminate_handler.h" line="69" column="1" bodyfile="terminate_handler.h" bodystart="69" bodyend="-1"/>
      </memberdef>
      </sectiondef>
    <innerclass refid="classgoogle_1_1cloud_1_1Credentials" prot="public">google::cloud::Credentials</innerclass>
    </compounddef>
  </doxygen>)xml";

  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: group__terminate
    name: group__terminate
    id: group__terminate
    type: module
    langs:
      - cpp
    summary: |
      # Intercepting Unrecoverable Errors


    references:
      - group__terminate_1gacc215b41a0bf17a7ea762fd5bb205348
      - classgoogle_1_1cloud_1_1Credentials
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='group__terminate']");
  ASSERT_TRUE(selected);
  auto const actual = Group2Yaml(selected.node());
  EXPECT_EQ(kExpected, actual);
}

TEST(DoxygenGroups, GroupsToc) {
  auto constexpr kXml =
      R"xml(<?xml version="1.0" standalone="yes"?><doxygen version="1.9.1" xml:lang="en-US">
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__g1" kind="group">
          <compoundname>g1</compoundname>
          <title>Group 1</title>
          <briefdescription><para>The description for Group 1.</para>
          </briefdescription>
          <detaileddescription><para>More details about Group 1.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__g2" kind="group">
          <compoundname>g2</compoundname>
          <title>Group 2</title>
          <briefdescription><para>The description for Group 2.</para>
          </briefdescription>
          <detaileddescription><para>More details about Group 2.</para></detaileddescription>
        </compounddef>
      </doxygen>)xml";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(kXml));
  auto const actual = GroupsToc(doc);

  EXPECT_THAT(actual,
              ElementsAre(FieldsAre("group__g1", "Group 1", "group__g1.yml"),
                          FieldsAre("group__g2", "Group 2", "group__g2.yml")));
}

}  // namespace
}  // namespace docfx
