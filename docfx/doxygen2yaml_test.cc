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

auto constexpr kFunctionXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="function" id="classgoogle_1_1cloud_1_1CompletionQueue_1a760d68ec606a03ab8cc80eea8bd965b3" prot="public" static="no" const="no" explicit="no" inline="yes" virt="non-virtual">
        <templateparamlist>
          <param>
            <type>typename Rep</type>
          </param>
          <param>
            <type>typename Period</type>
          </param>
        </templateparamlist>
        <type><ref refid="classgoogle_1_1cloud_1_1future" kindref="compound">future</ref>&lt; <ref refid="classgoogle_1_1cloud_1_1StatusOr" kindref="compound">StatusOr</ref>&lt; std::chrono::system_clock::time_point &gt; &gt;</type>
        <definition>future&lt; StatusOr&lt; std::chrono::system_clock::time_point &gt; &gt; google::cloud::CompletionQueue::MakeRelativeTimer</definition>
        <argsstring>(std::chrono::duration&lt; Rep, Period &gt; duration)</argsstring>
        <name>MakeRelativeTimer</name>
        <qualifiedname>google::cloud::CompletionQueue::MakeRelativeTimer</qualifiedname>
        <param>
          <type>std::chrono::duration&lt; Rep, Period &gt;</type>
          <declname>duration</declname>
        </param>
        <briefdescription>
<para>Create a timer that fires after the <computeroutput>duration</computeroutput>.</para>
        </briefdescription>
        <detaileddescription>
<para>A longer description here.<parameterlist kind="templateparam"><parameteritem>
<parameternamelist>
<parametername>Rep</parametername>
</parameternamelist>
<parameterdescription>
<para>a placeholder to match the Rep tparam for <computeroutput>duration</computeroutput> type, the semantics of this template parameter are documented in <computeroutput>std::chrono::duration&lt;&gt;</computeroutput> (in brief, the underlying arithmetic type used to store the number of ticks), for our purposes it is simply a formal parameter. </para>
</parameterdescription>
</parameteritem>
<parameteritem>
<parameternamelist>
<parametername>Period</parametername>
</parameternamelist>
<parameterdescription>
<para>a placeholder to match the Period tparam for <computeroutput>duration</computeroutput> type, the semantics of this template parameter are documented in <computeroutput>std::chrono::duration&lt;&gt;</computeroutput> (in brief, the length of the tick in seconds, expressed as a <computeroutput>std::ratio&lt;&gt;</computeroutput>), for our purposes it is simply a formal parameter.</para>
</parameterdescription>
</parameteritem>
</parameterlist>
<parameterlist kind="param"><parameteritem>
<parameternamelist>
<parametername>duration</parametername>
</parameternamelist>
<parameterdescription>
<para>when should the timer expire relative to the current time.</para>
</parameterdescription>
</parameteritem>
</parameterlist>
<simplesect kind="return"><para>a future that becomes satisfied after <computeroutput>duration</computeroutput> time has elapsed. The result of the future is the time at which it expired, or an error <ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> if the timer did not run to expiration (e.g. it was cancelled). </para>
</simplesect>
</para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="completion_queue.h" line="96" column="10" bodyfile="completion_queue.h" bodystart="96" bodyend="100"/>
      </memberdef>
    </doxygen>)xml";

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

TEST(Doxygen2Yaml, IncludeInPublicDocs) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <sectiondef id="id-1" kind="private-attrib"></sectiondef>
      <sectiondef id="id-2" kind="private-func"></sectiondef>
      <sectiondef id="id-3" kind="public-attrib"></sectiondef>
      <sectiondef id="id-4" kind="public-func"></sectiondef>
      <compounddef id="id-5" kind="file" language="C++"></compounddef>
      <compounddef id="id-6" kind="dir"></compounddef>
      <compounddef id="namespacestd" kind="namespace"></compounddef>
      <compounddef id="namespacestd_1_1chrono" kind="namespace"></compounddef>
      <compounddef id="classstd_1_1array"></compounddef>
      <compounddef id="classgoogle_1_1cloud_1_1Options" prot="public">google::cloud::Options</compounddef>
      <compounddef id="classgoogle_1_1cloud_1_1Options_1_1DataHolder" prot="private"></compounddef>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    bool expected;
  } const cases[] = {
      {"id-1", false},
      {"id-2", false},
      {"id-3", true},
      {"id-4", true},
      {"id-5", false},
      {"id-6", false},
      {"namespacestd", false},
      {"namespacestd_1_1chrono", false},
      {"classstd_1_1array", false},
      {"classgoogle_1_1cloud_1_1Options", true},
      {"classgoogle_1_1cloud_1_1Options_1_1DataHolder", false},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    auto const filter = "//*[@id='" + test.id + "']";
    auto selected = doc.select_node(filter.c_str());
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, IncludeInPublicDocuments(selected.node()));
  }
}

TEST(Doxygen2Yaml, StartAndEnd) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - 1
  - 2
  - 3
)yml";

  pugi::xml_document doc;
  YAML::Emitter yaml;
  StartDocFxYaml(yaml);
  YamlContext ctx;
  yaml << 1 << 2 << 3;
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, EnumValue) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    name: |
      kIdempotent
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
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c";
  ASSERT_TRUE(AppendIfEnumValue(yaml, ctx, selected.node()));

  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Enum) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    name: |
      Idempotency
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
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    name: |
      kIdempotent
    id: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
    parent: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
    type: enumvalue
    langs:
      - cpp
    summary: |
      The operation is idempotent and can be retried after a transient failure.
  - uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190
    name: |
      kNonIdempotent
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
  YAML::Emitter yaml;
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfEnum(yaml, ctx, selected.node()));
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
        <inbodydescription>
        </inbodydescription>
        <location file="grpc_options.h" line="148" column="1" bodyfile="grpc_options.h" bodystart="149" bodyend="-1"/>
      </memberdef>
    </doxygen>)xml";
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f
    name: |
      BackgroundThreadsFactory
    fullName: |
      google::cloud::BackgroundThreadsFactory
    id: namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f
    parent: test-only-parent-id
    type: typedef
    langs:
      - cpp
    syntax:
      contents: |
        using google::cloud::BackgroundThreadsFactory =
          std::function< std::unique_ptr< BackgroundThreads >()>;
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
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfTypedef(yaml, ctx, selected.node()));
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Function) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1CompletionQueue_1a760d68ec606a03ab8cc80eea8bd965b3
    name: MakeRelativeTimer
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
      returns:
        var_type: |
          future< StatusOr< std::chrono::system_clock::time_point > >
      parameters:
        - id: duration
          var_type: |
            std::chrono::duration< Rep, Period >
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

      A longer description here.
)yml";

  pugi::xml_document doc;
  doc.load_string(kFunctionXml);
  auto selected = doc.select_node(
      "//"
      "*[@id='classgoogle_1_1cloud_1_1CompletionQueue_"
      "1a760d68ec606a03ab8cc80eea8bd965b3']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfFunction(yaml, ctx, selected.node()));
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, SectionDef) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: structgoogle_1_1cloud_1_1AccessTokenLifetimeOption_1ad6b8a4672f1c196926849229f62d0de2
    name: |
      Type
    fullName: |
      google::cloud::AccessTokenLifetimeOption::Type
    id: structgoogle_1_1cloud_1_1AccessTokenLifetimeOption_1ad6b8a4672f1c196926849229f62d0de2
    parent: test-only-parent-id
    type: typedef
    langs:
      - cpp
    syntax:
      contents: |
        using google::cloud::AccessTokenLifetimeOption::Type =
          std::chrono::seconds;
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
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfSectionDef(yaml, ctx, selected.node()));
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Namespace) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: namespacegoogle_1_1cloud_1_1mocks
    name: google::cloud::mocks
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
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfNamespace(yaml, ctx, selected.node()));
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Class) {
  auto constexpr kExpected = R"yml(### YamlMime:UniversalReference
items:
  - uid: classgoogle_1_1cloud_1_1RuntimeStatusError
    name: google::cloud::RuntimeStatusError
    id: classgoogle_1_1cloud_1_1RuntimeStatusError
    parent: test-only-parent-id
    type: class
    langs:
      - cpp
    syntax:
      contents: |
        // Found in #include <google/cloud/status.h>
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
  - uid: classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95
    name: RuntimeStatusError
    fullName: |
      google::cloud::RuntimeStatusError::RuntimeStatusError
    id: classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95
    parent: classgoogle_1_1cloud_1_1RuntimeStatusError
    type: function
    langs:
      - cpp
    syntax:
      contents: |
        google::cloud::RuntimeStatusError::RuntimeStatusError (
            Status status
          )
      parameters:
        - id: status
          var_type: |
            Status
      source:
        id: RuntimeStatusError
        path: google/cloud/status.h
        startLine: 163
        remote:
          repo: https://github.com/googleapis/google-cloud-cpp/
          branch: main
          path: google/cloud/status.h
  - uid: classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e
    name: status
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
      returns:
        var_type: |
          Status const &
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
  StartDocFxYaml(yaml);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfClass(yaml, ctx, selected.node()));
  auto const actual = EndDocFxYaml(yaml);
  EXPECT_EQ(actual, kExpected);
}

}  // namespace
}  // namespace docfx
