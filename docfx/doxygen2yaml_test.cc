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

#include "docfx/doxygen2yaml.h"
#include "docfx/testing/inputs.h"
#include "docfx/yaml_context.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

auto constexpr kEnumXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="enum" id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c" prot="public" static="no" strong="yes">
        <type/>
        <name>Idempotency</name>
        <qualifiedname>google::cloud::Idempotency</qualifiedname>
        <enumvalue id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa" prot="public">
          <name>kIdempotent</name>
          <briefdescription>
<para>The operation is idempotent and can be retried after a transient failure.</para>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
        </enumvalue>
        <enumvalue id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190" prot="public">
          <name>kNonIdempotent</name>
          <briefdescription>
<para>The operation is not idempotent and should <bold>not</bold> be retried after a transient failure.</para>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
        </enumvalue>
        <briefdescription>
<para>Whether a request is <ulink url="https://en.wikipedia.org/wiki/Idempotence">idempotent</ulink>.</para>
        </briefdescription>
        <detaileddescription>
<para>When a RPC fails with a retryable error, the
<computeroutput>google-cloud-cpp</computeroutput> client libraries automatically
retry the RPC <bold>if</bold> the RPC is
<ulink url="https://en.wikipedia.org/wiki/Idempotence">idempotent</ulink>.
For each service, the library define a policy that determines whether a given
request is idempotent. In many cases this can be determined statically,
for example, read-only operations are always idempotent. In some cases,
the contents of the request may need to be examined to determine if the
operation is idempotent. For example, performing operations with pre-conditions,
such that the pre-conditions change when the operation succeed, is typically
idempotent.</para>
<para>Applications may override the default idempotency policy, though we
anticipate that this would be needed only in very rare circumstances. A few
examples include:</para>
<para><itemizedlist>
<listitem><para>In some services deleting "the most recent" entry may be idempotent
if the system has been configured to keep no history or versions, as the deletion
may succeed only once. In contrast, deleting "the most recent entry" is
<bold>not</bold> idempotent if the system keeps multiple versions.
Google Cloud Storage or Bigtable can be configured either way.</para>
</listitem><listitem><para>In some applications, creating a duplicate entry may
be acceptable as the system will deduplicate them later. In such systems it may
be preferable to retry the operation even though it is not idempotent.</para>
</listitem></itemizedlist>
</para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="idempotency.h" line="55" column="1" bodyfile="idempotency.h" bodystart="55" bodyend="61"/>
      </memberdef>
    </doxygen>
)xml";

auto constexpr kStructXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
  <doxygen>
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="structgoogle_1_1cloud_1_1AccessTokenLifetimeOption" kind="struct" language="C++" prot="public">
      <compoundname>google::cloud::AccessTokenLifetimeOption</compoundname>
      <includes refid="credentials_8h" local="no">google/cloud/credentials.h</includes>
        <sectiondef kind="public-type">
        <memberdef kind="typedef" id="structgoogle_1_1cloud_1_1AccessTokenLifetimeOption_1ad6b8a4672f1c196926849229f62d0de2" prot="public" static="no">
          <type>std::chrono::seconds</type>
          <definition>using google::cloud::AccessTokenLifetimeOption::Type =  std::chrono::seconds</definition>
          <argsstring/>
          <name>Type</name>
          <qualifiedname>google::cloud::AccessTokenLifetimeOption::Type</qualifiedname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
          <inbodydescription>
          </inbodydescription>
          <location file="credentials.h" line="301" column="3" bodyfile="credentials.h" bodystart="301" bodyend="-1"/>
        </memberdef>
        </sectiondef>
      <briefdescription>
        <para>Configure the access token lifetime. </para>
      </briefdescription>
      <detaileddescription>
      </detaileddescription>
      <location file="credentials.h" line="300" column="1" bodyfile="credentials.h" bodystart="300" bodyend="302"/>
      <listofallmembers>
        <member refid="structgoogle_1_1cloud_1_1AccessTokenLifetimeOption_1ad6b8a4672f1c196926849229f62d0de2" prot="public" virt="non-virtual"><scope>google::cloud::AccessTokenLifetimeOption</scope><name>Type</name></member>
      </listofallmembers>
    </compounddef>
  </doxygen>)xml";

auto constexpr kStruct2Xml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
  <doxygen>
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="structgoogle_1_1cloud_1_1LogRecord" kind="struct" language="C++" prot="public">
        <compoundname>google::cloud::LogRecord</compoundname>
        <includes refid="log_8h" local="no">google/cloud/log.h</includes>
          <sectiondef kind="public-attrib">
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc" prot="public" static="no" mutable="no">
            <type><ref refid="namespacegoogle_1_1cloud_1aeeb9b9a1eeb3fc7f6ff13bd078172922" kindref="member">Severity</ref></type>
            <definition>Severity google::cloud::LogRecord::severity</definition>
            <argsstring/>
            <name>severity</name>
            <qualifiedname>google::cloud::LogRecord::severity</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="152" column="12" bodyfile="log.h" bodystart="152" bodyend="-1"/>
          </memberdef>
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a8a04caf649e69b55404abf2d3b72d4a6" prot="public" static="no" mutable="no">
            <type>std::string</type>
            <definition>std::string google::cloud::LogRecord::function</definition>
            <argsstring/>
            <name>function</name>
            <qualifiedname>google::cloud::LogRecord::function</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="153" column="15" bodyfile="log.h" bodystart="153" bodyend="-1"/>
          </memberdef>
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a46bc9a3adab542be80be9671d2ff82e6" prot="public" static="no" mutable="no">
            <type>std::string</type>
            <definition>std::string google::cloud::LogRecord::filename</definition>
            <argsstring/>
            <name>filename</name>
            <qualifiedname>google::cloud::LogRecord::filename</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="154" column="15" bodyfile="log.h" bodystart="154" bodyend="-1"/>
          </memberdef>
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a29f2cf2bafa97addc548c26xx48a4fe0" prot="public" static="no" mutable="no">
            <type>int</type>
            <definition>int google::cloud::LogRecord::lineno</definition>
            <argsstring/>
            <name>lineno</name>
            <qualifiedname>google::cloud::LogRecord::lineno</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="155" column="7" bodyfile="log.h" bodystart="155" bodyend="-1"/>
          </memberdef>
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a9acea199684809e231263a486559f834" prot="public" static="no" mutable="no">
            <type>std::thread::id</type>
            <definition>std::thread::id google::cloud::LogRecord::thread_id</definition>
            <argsstring/>
            <name>thread_id</name>
            <qualifiedname>google::cloud::LogRecord::thread_id</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="156" column="19" bodyfile="log.h" bodystart="156" bodyend="-1"/>
          </memberdef>
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a949e7b4cb62d085ee13b107e63f83152" prot="public" static="no" mutable="no">
            <type>std::chrono::system_clock::time_point</type>
            <definition>std::chrono::system_clock::time_point google::cloud::LogRecord::timestamp</definition>
            <argsstring/>
            <name>timestamp</name>
            <qualifiedname>google::cloud::LogRecord::timestamp</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="157" column="41" bodyfile="log.h" bodystart="157" bodyend="-1"/>
          </memberdef>
          <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a95652739567b944a4ffbbb6d31b3f2e0" prot="public" static="no" mutable="no">
            <type>std::string</type>
            <definition>std::string google::cloud::LogRecord::message</definition>
            <argsstring/>
            <name>message</name>
            <qualifiedname>google::cloud::LogRecord::message</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="log.h" line="158" column="15" bodyfile="log.h" bodystart="158" bodyend="-1"/>
          </memberdef>
          </sectiondef>
        <briefdescription>
    <para>Represents a single log message.</para>
        </briefdescription>
        <detaileddescription>
        </detaileddescription>
        <collaborationgraph>
          <node id="1">
            <label>google::cloud::LogRecord</label>
            <link refid="structgoogle_1_1cloud_1_1LogRecord"/>
            <childnode refid="2" relation="usage">
              <edgelabel>filename</edgelabel>
              <edgelabel>function</edgelabel>
              <edgelabel>message</edgelabel>
            </childnode>
          </node>
          <node id="3">
            <label>std::basic_string&lt; Char &gt;</label>
          </node>
          <node id="2">
            <label>std::string</label>
            <childnode refid="3" relation="public-inheritance">
            </childnode>
          </node>
        </collaborationgraph>
        <location file="log.h" line="151" column="1" bodyfile="log.h" bodystart="151" bodyend="159"/>
        <listofallmembers>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a46bc9a3adab542be80be9671d2ff82e6" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>filename</name></member>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a8a04caf649e69b55404abf2d3b72d4a6" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>function</name></member>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a29f2cf2bafa97addc548c26xx48a4fe0" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>lineno</name></member>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a95652739567b944a4ffbbb6d31b3f2e0" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>message</name></member>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>severity</name></member>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a9acea199684809e231263a486559f834" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>thread_id</name></member>
          <member refid="structgoogle_1_1cloud_1_1LogRecord_1a949e7b4cb62d085ee13b107e63f83152" prot="public" virt="non-virtual"><scope>google::cloud::LogRecord</scope><name>timestamp</name></member>
        </listofallmembers>
      </compounddef>
    </doxygen>)xml";

auto constexpr kNamespaceXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud_1_1mocks" kind="namespace" language="C++">
        <compoundname>google::cloud::mocks</compoundname>
          <sectiondef kind="func">
          </sectiondef>
        <briefdescription>
          <para>Contains helpers for testing the Google Cloud C++ Client Libraries.</para>
        </briefdescription>
        <detaileddescription>
<para>The symbols defined in this namespace are part of
<computeroutput>google-cloud-cpp</computeroutput>'s public API. Application
developers may use them when mocking the client libraries in their own
tests.</para>
        </detaileddescription>
        <location file="mocks/mock_stream_range.h" line="30" column="1"/>
      </compounddef>
    </doxygen>)xml";

auto constexpr kClassXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1RuntimeStatusError" kind="class" language="C++" prot="public">
      <compoundname>google::cloud::RuntimeStatusError</compoundname>
      <basecompoundref prot="public" virt="non-virtual">std::runtime_error</basecompoundref>
      <includes refid="status_8h" local="no">google/cloud/status.h</includes>
        <sectiondef kind="private-attrib">
        <memberdef kind="variable" id="classgoogle_1_1cloud_1_1RuntimeStatusError_1a85bebd1a98468aff6b7f5fe54f7b4241" prot="private" static="no" mutable="no">
          <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></type>
          <definition>Status google::cloud::RuntimeStatusError::status_</definition>
          <argsstring/>
          <name>status_</name>
          <qualifiedname>google::cloud::RuntimeStatusError::status_</qualifiedname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
          <inbodydescription>
          </inbodydescription>
          <location file="status.h" line="168" column="10" bodyfile="status.h" bodystart="168" bodyend="-1"/>
        </memberdef>
        </sectiondef>
        <sectiondef kind="public-func">
        <memberdef kind="function" id="classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95" prot="public" static="no" const="no" explicit="yes" inline="no" virt="non-virtual">
          <type/>
          <definition>google::cloud::RuntimeStatusError::RuntimeStatusError</definition>
          <argsstring>(Status status)</argsstring>
          <name>RuntimeStatusError</name>
          <qualifiedname>google::cloud::RuntimeStatusError::RuntimeStatusError</qualifiedname>
          <param>
            <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></type>
            <declname>status</declname>
          </param>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
          <inbodydescription>
          </inbodydescription>
          <location file="status.h" line="163" column="12"/>
        </memberdef>
        <memberdef kind="function" id="classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e" prot="public" static="no" const="yes" explicit="no" inline="yes" virt="non-virtual">
          <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
          <definition>Status const  &amp; google::cloud::RuntimeStatusError::status</definition>
          <argsstring>() const</argsstring>
          <name>status</name>
          <qualifiedname>google::cloud::RuntimeStatusError::status</qualifiedname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
          <inbodydescription>
          </inbodydescription>
          <location file="status.h" line="165" column="16" bodyfile="status.h" bodystart="165" bodyend="165"/>
        </memberdef>
        </sectiondef>
      <briefdescription>
  <para>A runtime error that wraps a <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">google::cloud::Status</ref></computeroutput>.</para>
      </briefdescription>
      <detaileddescription>
      </detaileddescription>
      <inheritancegraph>
        <node id="1">
          <label>google::cloud::RuntimeStatusError</label>
          <link refid="classgoogle_1_1cloud_1_1RuntimeStatusError"/>
          <childnode refid="2" relation="public-inheritance">
          </childnode>
        </node>
        <node id="3">
          <label>std::exception</label>
        </node>
        <node id="2">
          <label>std::runtime_error</label>
          <childnode refid="3" relation="public-inheritance">
          </childnode>
        </node>
      </inheritancegraph>
      <collaborationgraph>
        <node id="1">
          <label>google::cloud::RuntimeStatusError</label>
          <link refid="classgoogle_1_1cloud_1_1RuntimeStatusError"/>
          <childnode refid="2" relation="public-inheritance">
          </childnode>
        </node>
        <node id="3">
          <label>std::exception</label>
        </node>
        <node id="2">
          <label>std::runtime_error</label>
          <childnode refid="3" relation="public-inheritance">
          </childnode>
        </node>
      </collaborationgraph>
      <location file="status.h" line="161" column="1" bodyfile="status.h" bodystart="161" bodyend="169"/>
      <listofallmembers>
        <member refid="classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95" prot="public" virt="non-virtual"><scope>google::cloud::RuntimeStatusError</scope><name>RuntimeStatusError</name></member>
        <member refid="classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e" prot="public" virt="non-virtual"><scope>google::cloud::RuntimeStatusError</scope><name>status</name></member>
        <member refid="classgoogle_1_1cloud_1_1RuntimeStatusError_1a85bebd1a98468aff6b7f5fe54f7b4241" prot="private" virt="non-virtual"><scope>google::cloud::RuntimeStatusError</scope><name>status_</name></member>
      </listofallmembers>
    </compounddef>
  </doxygen>)xml";

void TestPre(YAML::Emitter& yaml) {
  yaml << YAML::BeginMap << YAML::Key << "items" << YAML::Value
       << YAML::BeginSeq;
}

void TestPost(YAML::Emitter& yaml) { yaml << YAML::EndSeq << YAML::EndMap; }

TEST(Doxygen2Yaml, End) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - 1
  - 2
  - 3
)yml";

  pugi::xml_document doc;
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  yaml << 1 << 2 << 3;
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, EnumValue) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    name: "kIdempotent"
    id: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    parent: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    type: enumvalue
    langs:
      - cpp
    summary: |
      The operation is idempotent and can be retried after a transient failure.
)yml";

  pugi::xml_document doc;
  doc.load_string(kEnumXml);
  auto selected = doc.select_node(
      "//"
      "*[@id='namespacegoogle_1_1cloud_"
      "1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c";
  ASSERT_TRUE(AppendIfEnumValue(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Enum) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    name: "Idempotency"
    fullName: |
      google::cloud::Idempotency
    id: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    parent: test-only-parent-id
    type: enum
    langs:
      - cpp
    syntax:
      contents: |
        enum class google::cloud::Idempotency {
          kIdempotent,
          kNonIdempotent,
        };
      source:
        id: Idempotency
        path: google/cloud/idempotency.h
        startLine: 55
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/idempotency.h
    summary: |
      Whether a request is [idempotent](https://en.wikipedia.org/wiki/Idempotence).
    conceptual: |
      When a RPC fails with a retryable error, the
      `google-cloud-cpp` client libraries automatically
      retry the RPC **if** the RPC is
      [idempotent](https://en.wikipedia.org/wiki/Idempotence).
      For each service, the library define a policy that determines whether a given
      request is idempotent. In many cases this can be determined statically,
      for example, read-only operations are always idempotent. In some cases,
      the contents of the request may need to be examined to determine if the
      operation is idempotent. For example, performing operations with pre-conditions,
      such that the pre-conditions change when the operation succeed, is typically
      idempotent.

      Applications may override the default idempotency policy, though we
      anticipate that this would be needed only in very rare circumstances. A few
      examples include:


      - In some services deleting "the most recent" entry may be idempotent
      if the system has been configured to keep no history or versions, as the deletion
      may succeed only once. In contrast, deleting "the most recent entry" is
      **not** idempotent if the system keeps multiple versions.
      Google Cloud Storage or Bigtable can be configured either way.
      - In some applications, creating a duplicate entry may
      be acceptable as the system will deduplicate them later. In such systems it may
      be preferable to retry the operation even though it is not idempotent.
    children:
      - namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
      - namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    name: "kIdempotent"
    id: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    parent: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    type: enumvalue
    langs:
      - cpp
    summary: |
      The operation is idempotent and can be retried after a transient failure.
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190
    name: "kNonIdempotent"
    id: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190
    parent: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    type: enumvalue
    langs:
      - cpp
    summary: |
      The operation is not idempotent and should **not** be retried after a transient failure.
)yml";

  pugi::xml_document doc;
  doc.load_string(kEnumXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c"
      "']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  TestPre(yaml);
  ASSERT_TRUE(AppendIfEnum(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Typedef) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="typedef" id="namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f" prot="public" static="no">
        <type>std::function&lt; std::unique_ptr&lt; <ref refid="classgoogle_1_1cloud_1_1BackgroundThreads" kindref="compound">BackgroundThreads</ref> &gt;()&gt;</type>
        <definition>using google::cloud::BackgroundThreadsFactory = typedef std::function&lt;std::unique_ptr&lt;BackgroundThreads&gt;()&gt;</definition>
        <argsstring/>
        <name>BackgroundThreadsFactory</name>
        <qualifiedname>google::cloud::BackgroundThreadsFactory</qualifiedname>
        <briefdescription>
        <para>A short description of the thing.</para>
        </briefdescription>
        <detaileddescription>
        <para>A longer description would go here.</para>
        </detaileddescription>
        <description>
          <para>The rarely used <computeroutput>&lt;description&gt;</computeroutput> would go here.</para>
        </description>
        <inbodydescription>
        </inbodydescription>
        <location file="grpc_options.h" line="148" column="1" bodyfile="grpc_options.h" bodystart="149" bodyend="-1"/>
      </memberdef>
    </doxygen>)xml";
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f
    name: "BackgroundThreadsFactory"
    fullName: "google::cloud::BackgroundThreadsFactory"
    id: namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f
    parent: test-only-parent-id
    type: typealias
    langs:
      - cpp
    syntax:
      contents: |
        using google::cloud::BackgroundThreadsFactory =
          std::function< std::unique_ptr< BackgroundThreads >()>;
      aliasof: |
        <code>std::function&lt; std::unique_ptr&lt; BackgroundThreads &gt;()&gt;</code>
      source:
        id: BackgroundThreadsFactory
        path: google/cloud/grpc_options.h
        startLine: 148
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/grpc_options.h
    summary: |
      A short description of the thing.
    conceptual: |
      The rarely used `<description>` would go here.

      A longer description would go here.
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f"
      "']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfTypedef(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Friend) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="friend" id="classgoogle_1_1cloud_1_1ErrorInfo_1a3e7a9be9a728e13d3333784f63270555" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
        <type>bool</type>
        <definition>bool operator==</definition>
        <argsstring>(ErrorInfo const &amp;, ErrorInfo const &amp;)</argsstring>
        <name>operator==</name>
        <qualifiedname>google::cloud::ErrorInfo::operator==</qualifiedname>
        <param>
          <type><ref refid="classgoogle_1_1cloud_1_1ErrorInfo" kindref="compound">ErrorInfo</ref> const &amp;</type>
        </param>
        <param>
          <type><ref refid="classgoogle_1_1cloud_1_1ErrorInfo" kindref="compound">ErrorInfo</ref> const &amp;</type>
        </param>
        <briefdescription>
          <para>A short description of the thing.</para>
        </briefdescription>
        <detaileddescription>
          <para>A longer description would go here.</para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="status.h" line="86" column="15"/>
      </memberdef>
    </doxygen>)xml";
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1ErrorInfo_1a3e7a9be9a728e13d3333784f63270555
    name: "operator==(ErrorInfo const &, ErrorInfo const &)"
    fullName: |
      google::cloud::ErrorInfo::operator==
    id: classgoogle_1_1cloud_1_1ErrorInfo_1a3e7a9be9a728e13d3333784f63270555
    parent: test-only-parent-id
    type: friend
    langs:
      - cpp
    syntax:
      contents: |
        friend bool
        google::cloud::ErrorInfo::operator== (
            ErrorInfo const &,
            ErrorInfo const &
          )
      source:
        id: operator==
        path: google/cloud/status.h
        startLine: 86
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/status.h
    summary: |
      A short description of the thing.
    conceptual: |
      A longer description would go here.
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1ErrorInfo_1a3e7a9be9a728e13d3333784f63270555"
      "']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfFriend(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Variable) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc
    name: "severity"
    fullName: |
      google::cloud::LogRecord::severity
    id: structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc
    parent: test-only-parent-id
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        Severity severity;
      source:
        id: severity
        path: google/cloud/log.h
        startLine: 152
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
)yml";

  pugi::xml_document doc;
  doc.load_string(kStruct2Xml);
  auto selected = doc.select_node(
      "//*[@id='"
      "structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc"
      "']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfVariable(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Function) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1CompletionQueue_1a760d68ec606a03ab8cc80eea8bd965b3
    name: "MakeRelativeTimer(std::chrono::duration< Rep, Period >)"
    fullName: |
      google::cloud::CompletionQueue::MakeRelativeTimer
    id: classgoogle_1_1cloud_1_1CompletionQueue_1a760d68ec606a03ab8cc80eea8bd965b3
    parent: test-only-parent-id
    type: function
    langs:
      - cpp
    syntax:
      contents: |
        template <
            typename Rep,
            typename Period>
        future< StatusOr< std::chrono::system_clock::time_point > >
        google::cloud::CompletionQueue::MakeRelativeTimer (
            std::chrono::duration< Rep, Period > duration
          )
      return:
        type:
          - "future< StatusOr< std::chrono::system_clock::time_point > >"
        description: |
          a future that becomes satisfied after `duration` time has elapsed. The result of the future is the time at which it expired, or an error [Status](xref:classgoogle_1_1cloud_1_1Status) if the timer did not run to expiration (e.g. it was cancelled).
      parameters:
        - id: duration
          var_type: "std::chrono::duration&lt; Rep, Period &gt;"
          description: |
            when should the timer expire relative to the current time.
        - id: typename Rep
          description: |
            a placeholder to match the Rep tparam for `duration` type, the semantics of this template parameter are documented in `std::chrono::duration<>` (in brief, the underlying arithmetic type used to store the number of ticks), for our purposes it is simply a formal parameter.
        - id: typename Period
          description: |
            a placeholder to match the Period tparam for `duration` type, the semantics of this template parameter are documented in `std::chrono::duration<>` (in brief, the length of the tick in seconds, expressed as a `std::ratio<>`), for our purposes it is simply a formal parameter.
      source:
        id: MakeRelativeTimer
        path: google/cloud/completion_queue.h
        startLine: 96
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/completion_queue.h
    summary: |
      Create a timer that fires after the `duration`.
    conceptual: |
      A longer description here.
)yml";

  pugi::xml_document doc;
  doc.load_string(docfx_testing::FunctionXml().c_str());
  auto selected = doc.select_node(
      ("//*[@id='" + docfx_testing::FunctionXmlId() + "']").c_str());
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfFunction(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, InheritSectionDefSummary) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1StatusOr" kind="class" language="C++" prot="public" final="yes">
        <compoundname>google::cloud::StatusOr</compoundname>
        <includes refid="status__or_8h" local="no">google/cloud/status_or.h</includes>
        <templateparamlist>
          <param>
            <type>typename T</type>
          </param>
        </templateparamlist>
        <sectiondef kind="user-defined">
          <header>Dereference operators.</header>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1StatusOr_1a95250d82418ed95673d41377347a3dbd" prot="public" static="no" const="no" explicit="no" inline="yes" refqual="lvalue" virt="non-virtual">
              <type>T &amp;</type>
              <definition>T &amp; google::cloud::StatusOr&lt; T &gt;::operator*</definition>
              <argsstring>() &amp;</argsstring>
              <name>operator*</name>
              <qualifiedname>google::cloud::StatusOr::operator*</qualifiedname>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status_or.h" line="208" column="5" bodyfile="status_or.h" bodystart="208" bodyend="208"/>
            </memberdef>
        </sectiondef>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1StatusOr
    name: "StatusOr<T>"
    id: classgoogle_1_1cloud_1_1StatusOr
    parent: test-only-parent-id
    type: class
    langs:
      - cpp
    syntax:
      contents: |
        // Found in #include "google/cloud/status_or.h"
        template <
            typename T>
        class google::cloud::StatusOr { ... };
    children:
      - classgoogle_1_1cloud_1_1StatusOr_1a95250d82418ed95673d41377347a3dbd
  - uid: classgoogle_1_1cloud_1_1StatusOr_1a95250d82418ed95673d41377347a3dbd
    name: "operator*() &"
    fullName: |
      google::cloud::StatusOr::operator*
    id: classgoogle_1_1cloud_1_1StatusOr_1a95250d82418ed95673d41377347a3dbd
    parent: classgoogle_1_1cloud_1_1StatusOr
    type: operator
    langs:
      - cpp
    syntax:
      contents: |
        T &
        google::cloud::StatusOr::operator* ()
      return:
        type:
          - "T &"
      source:
        id: operator*
        path: google/cloud/status_or.h
        startLine: 208
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/status_or.h
    summary: |
      Dereference operators.
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected =
      doc.select_node("//*[@id='classgoogle_1_1cloud_1_1StatusOr']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfClass(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, MockedFunction) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection_1a789db998d71abf9016b64832d0c7a99e
    name: "virtual ListCryptoKeys(google::cloud::kms::inventory::v1::ListCryptoKeysRequest)"
    fullName: |
      google::cloud::kms_inventory_v1::KeyDashboardServiceConnection::ListCryptoKeys
    id: classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection_1a789db998d71abf9016b64832d0c7a99e
    parent: classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection
    type: function
    langs:
      - cpp
    syntax:
      contents: |
        StreamRange< google::cloud::kms::v1::CryptoKey >
        google::cloud::kms_inventory_v1::KeyDashboardServiceConnection::ListCryptoKeys (
            google::cloud::kms::inventory::v1::ListCryptoKeysRequest request
          )
      return:
        type:
          - "StreamRange< google::cloud::kms::v1::CryptoKey >"
      parameters:
        - id: request
          var_type: "google::cloud::kms::inventory::v1::ListCryptoKeysRequest"
      source:
        id: ListCryptoKeys
        path: google/cloud/inventory/v1/key_dashboard_connection.h
        startLine: 67
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/inventory/v1/key_dashboard_connection.h
    conceptual: |
      This function is implemented using [gMock]'s `MOCK_METHOD()`.
      Consult the gMock documentation to use this mock in your tests.

      [gMock]: https://google.github.io/googletest
)yml";

  pugi::xml_document doc;
  doc.load_string(docfx_testing::MockClass().c_str());
  auto const class_filter =
      std::string{"//*[@id='" + docfx_testing::MockClassId() + "']"};
  auto parent = doc.select_node(class_filter.c_str());
  ASSERT_TRUE(parent);
  auto const function_filter =
      std::string{"//*[@id='" + docfx_testing::MockedFunctionId() + "']"};
  auto selected = doc.select_node(function_filter.c_str());
  ASSERT_TRUE(selected);
  auto const ctx = NestedYamlContext(YamlContext{}, parent.node());
  YAML::Emitter yaml;
  TestPre(yaml);
  ASSERT_TRUE(AppendIfFunction(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, SectionDef) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: structgoogle_1_1cloud_1_1AccessTokenLifetimeOption_1ad6b8a4672f1c196926849229f62d0de2
    name: "Type"
    fullName: "google::cloud::AccessTokenLifetimeOption::Type"
    id: structgoogle_1_1cloud_1_1AccessTokenLifetimeOption_1ad6b8a4672f1c196926849229f62d0de2
    parent: test-only-parent-id
    type: typealias
    langs:
      - cpp
    syntax:
      contents: |
        using google::cloud::AccessTokenLifetimeOption::Type =
          std::chrono::seconds;
      aliasof: |
        <code>std::chrono::seconds</code>
      source:
        id: Type
        path: google/cloud/credentials.h
        startLine: 301
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/credentials.h
)yml";

  pugi::xml_document doc;
  doc.load_string(kStructXml);
  auto selected = doc.select_node("//sectiondef[@kind='public-type']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfSectionDef(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Namespace) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1_1mocks
    name: "google::cloud::mocks"
    id: namespacegoogle_1_1cloud_1_1mocks
    parent: test-only-parent-id
    type: namespace
    langs:
      - cpp
    syntax:
      contents: |
        namespace google::cloud::mocks { ... };
      source:
        id: google::cloud::mocks
        path: google/cloud/mocks/mock_stream_range.h
        startLine: 30
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/mocks/mock_stream_range.h
    summary: |
      Contains helpers for testing the Google Cloud C++ Client Libraries.
    conceptual: |
      The symbols defined in this namespace are part of
      `google-cloud-cpp`'s public API. Application
      developers may use them when mocking the client libraries in their own
      tests.
)yml";

  pugi::xml_document doc;
  doc.load_string(kNamespaceXml);
  auto selected =
      doc.select_node("//*[@id='namespacegoogle_1_1cloud_1_1mocks']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfNamespace(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, NamespaceDeprecated) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud_1_1kms" kind="namespace" language="C++">
        <compoundname>google::cloud::kms</compoundname>
          <sectiondef kind="func">
          </sectiondef>
        <briefdescription>
        </briefdescription>
        <detaileddescription>
<para><xrefsect id="deprecated_1_deprecated000001"><xreftitle>Deprecated</xreftitle><xrefdescription><para>This namespace exists for backwards compatibility. Use the types defined in <ref refid="namespacegoogle_1_1cloud_1_1kms__v1" kindref="compound">kms_v1</ref> instead of the aliases defined in this namespace. </para>
</xrefdescription></xrefsect></para>
<para><xrefsect id="deprecated_1_deprecated000014"><xreftitle>Deprecated</xreftitle><xrefdescription><para>This namespace exists for backwards compatibility. Use the types defined in <ref refid="namespacegoogle_1_1cloud_1_1kms__v1" kindref="compound">kms_v1</ref> instead of the aliases defined in this namespace. </para>
</xrefdescription></xrefsect></para>
        </detaileddescription>
        <location file="ekm_client.h" line="30" column="1"/>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1_1kms
    name: "google::cloud::kms"
    id: namespacegoogle_1_1cloud_1_1kms
    parent: test-only-parent-id
    type: namespace
    langs:
      - cpp
    syntax:
      contents: |
        namespace google::cloud::kms { ... };
      source:
        id: google::cloud::kms
        path: google/cloud/kms/ekm_client.h
        startLine: 30
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/kms/ekm_client.h
    conceptual: |




      <aside class="deprecated">
          <b>Deprecated:</b> This namespace is deprecated, prefer the types defined in [`kms_v1`](xref:namespacegoogle_1_1cloud_1_1kms__v1).
      </aside>
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='namespacegoogle_1_1cloud_1_1kms']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ctx.library_root = "google/cloud/kms/";
  ASSERT_TRUE(AppendIfNamespace(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, NamespaceMemberRefid) {
  auto constexpr kXml =
      R"xml(<?xml version='1.0' encoding='UTF-8' standalone='no'?>
    <doxygen xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="compound.xsd" version="1.9.7" xml:lang="en-US">
      <compounddef id="namespacegoogle_1_1cloud" kind="namespace" language="C++">
      <compoundname>google::cloud</compoundname>
        <member refid="group__terminate_1gacc215b41a0bf17a7ea762fd5bb205348" kind="typedef"><name>TerminateHandler</name></member>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud
    name: "google::cloud"
    id: namespacegoogle_1_1cloud
    parent: test-only-parent-id
    type: namespace
    langs:
      - cpp
    syntax:
      contents: |
        namespace google::cloud { ... };
)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='namespacegoogle_1_1cloud']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ctx.library_root = "google/cloud";
  ASSERT_TRUE(AppendIfNamespace(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Class) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1RuntimeStatusError
    name: "RuntimeStatusError"
    id: classgoogle_1_1cloud_1_1RuntimeStatusError
    parent: test-only-parent-id
    type: class
    langs:
      - cpp
    syntax:
      contents: |
        // Found in #include "google/cloud/status.h"
        class google::cloud::RuntimeStatusError { ... };
      source:
        id: google::cloud::RuntimeStatusError
        path: google/cloud/status.h
        startLine: 161
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/status.h
    summary: |
      A runtime error that wraps a [`google::cloud::Status`](xref:classgoogle_1_1cloud_1_1Status).
    children:
      - classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95
      - classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e
  - uid: classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95
    name: "RuntimeStatusError(Status)"
    fullName: |
      google::cloud::RuntimeStatusError::RuntimeStatusError
    id: classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95
    parent: classgoogle_1_1cloud_1_1RuntimeStatusError
    type: constructor
    langs:
      - cpp
    syntax:
      contents: |
        google::cloud::RuntimeStatusError::RuntimeStatusError (
            Status status
          )
      parameters:
        - id: status
          var_type: "Status"
      source:
        id: RuntimeStatusError
        path: google/cloud/status.h
        startLine: 163
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/status.h
  - uid: classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e
    name: "status() const"
    fullName: |
      google::cloud::RuntimeStatusError::status
    id: classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e
    parent: classgoogle_1_1cloud_1_1RuntimeStatusError
    type: function
    langs:
      - cpp
    syntax:
      contents: |
        Status const &
        google::cloud::RuntimeStatusError::status ()
      return:
        type:
          - "Status const &"
      source:
        id: status
        path: google/cloud/status.h
        startLine: 165
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/status.h
)yml";

  pugi::xml_document doc;
  doc.load_string(kClassXml);
  auto selected =
      doc.select_node("//*[@id='classgoogle_1_1cloud_1_1RuntimeStatusError']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfClass(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Struct) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: structgoogle_1_1cloud_1_1LogRecord
    name: "LogRecord"
    id: structgoogle_1_1cloud_1_1LogRecord
    parent: test-only-parent-id
    type: struct
    langs:
      - cpp
    syntax:
      contents: |
        // Found in #include "google/cloud/log.h"
        struct google::cloud::LogRecord { ... };
      source:
        id: google::cloud::LogRecord
        path: google/cloud/log.h
        startLine: 151
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
    summary: |
      Represents a single log message.
    children:
      - structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc
      - structgoogle_1_1cloud_1_1LogRecord_1a8a04caf649e69b55404abf2d3b72d4a6
      - structgoogle_1_1cloud_1_1LogRecord_1a46bc9a3adab542be80be9671d2ff82e6
      - structgoogle_1_1cloud_1_1LogRecord_1a29f2cf2bafa97addc548c26xx48a4fe0
      - structgoogle_1_1cloud_1_1LogRecord_1a9acea199684809e231263a486559f834
      - structgoogle_1_1cloud_1_1LogRecord_1a949e7b4cb62d085ee13b107e63f83152
      - structgoogle_1_1cloud_1_1LogRecord_1a95652739567b944a4ffbbb6d31b3f2e0
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc
    name: "severity"
    fullName: |
      google::cloud::LogRecord::severity
    id: structgoogle_1_1cloud_1_1LogRecord_1a830f8xx5fe86e1581dddbbb2cd922cbc
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        Severity severity;
      source:
        id: severity
        path: google/cloud/log.h
        startLine: 152
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a8a04caf649e69b55404abf2d3b72d4a6
    name: "function"
    fullName: |
      google::cloud::LogRecord::function
    id: structgoogle_1_1cloud_1_1LogRecord_1a8a04caf649e69b55404abf2d3b72d4a6
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        std::string function;
      source:
        id: function
        path: google/cloud/log.h
        startLine: 153
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a46bc9a3adab542be80be9671d2ff82e6
    name: "filename"
    fullName: |
      google::cloud::LogRecord::filename
    id: structgoogle_1_1cloud_1_1LogRecord_1a46bc9a3adab542be80be9671d2ff82e6
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        std::string filename;
      source:
        id: filename
        path: google/cloud/log.h
        startLine: 154
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a29f2cf2bafa97addc548c26xx48a4fe0
    name: "lineno"
    fullName: |
      google::cloud::LogRecord::lineno
    id: structgoogle_1_1cloud_1_1LogRecord_1a29f2cf2bafa97addc548c26xx48a4fe0
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        int lineno;
      source:
        id: lineno
        path: google/cloud/log.h
        startLine: 155
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a9acea199684809e231263a486559f834
    name: "thread_id"
    fullName: |
      google::cloud::LogRecord::thread_id
    id: structgoogle_1_1cloud_1_1LogRecord_1a9acea199684809e231263a486559f834
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        std::thread::id thread_id;
      source:
        id: thread_id
        path: google/cloud/log.h
        startLine: 156
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a949e7b4cb62d085ee13b107e63f83152
    name: "timestamp"
    fullName: |
      google::cloud::LogRecord::timestamp
    id: structgoogle_1_1cloud_1_1LogRecord_1a949e7b4cb62d085ee13b107e63f83152
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        std::chrono::system_clock::time_point timestamp;
      source:
        id: timestamp
        path: google/cloud/log.h
        startLine: 157
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
  - uid: structgoogle_1_1cloud_1_1LogRecord_1a95652739567b944a4ffbbb6d31b3f2e0
    name: "message"
    fullName: |
      google::cloud::LogRecord::message
    id: structgoogle_1_1cloud_1_1LogRecord_1a95652739567b944a4ffbbb6d31b3f2e0
    parent: structgoogle_1_1cloud_1_1LogRecord
    type: variable
    langs:
      - cpp
    syntax:
      contents: |
        std::string message;
      source:
        id: message
        path: google/cloud/log.h
        startLine: 158
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/log.h
)yml";

  pugi::xml_document doc;
  doc.load_string(kStruct2Xml);
  auto selected =
      doc.select_node("//*[@id='structgoogle_1_1cloud_1_1LogRecord']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  TestPre(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfStruct(yaml, ctx, selected.node()));
  TestPost(yaml);
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

}  // namespace
}  // namespace docfx
