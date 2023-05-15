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

TEST(DoxygenGroups, CommonPage) {
  auto constexpr kXml =
      R"xml(<?xml version="1.0" standalone="yes"?><doxygen version="1.9.1" xml:lang="en-US">
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__terminate" kind="group">
    <compoundname>terminate</compoundname>
    <title>Intercepting Unrecoverable Errors</title>
      <briefdescription><para>Something pithy.</para></briefdescription>
      <detaileddescription><para>Something very long-winded.</para></detaileddescription>
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
    title: Intercepting Unrecoverable Errors
    id: group__terminate
    type: module
    langs:
      - cpp
    summary: |
      # Intercepting Unrecoverable Errors


      Something very long-winded.

      ### Classes

      - [`google::cloud::Credentials`](xref:classgoogle_1_1cloud_1_1Credentials)

      ### Types

      - [`TerminateHandler`](xref:group__terminate_1gacc215b41a0bf17a7ea762fd5bb205348)

references:
  - uid: group__terminate_1gacc215b41a0bf17a7ea762fd5bb205348
    name: TerminateHandler
  - uid: classgoogle_1_1cloud_1_1Credentials
    name: google::cloud::Credentials
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='group__terminate']");
  ASSERT_TRUE(selected);
  auto const actual = Group2Yaml(selected.node());
  EXPECT_EQ(kExpected, actual);
}

}  // namespace
}  // namespace docfx
