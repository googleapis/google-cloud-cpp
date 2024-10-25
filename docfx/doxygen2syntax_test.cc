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

#include "docfx/doxygen2syntax.h"
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
            <para>Whether a [truncated].</para>
        </briefdescription>
        <detaileddescription>
            <para>When a RPC fails with a retryable error [truncated]</para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="idempotency.h" line="55" column="1" bodyfile="idempotency.h" bodystart="55" bodyend="61"/>
      </memberdef>
    </doxygen>
)xml";

auto constexpr kTypedefXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
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

auto constexpr kVariableXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="variable" id="structgoogle_1_1cloud_1_1LogRecord_1a830f8e86e1581dddbbb2cd922cbc" prot="public" static="no" mutable="no">
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
  <para>A runtime error that wraps a <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">google::cloud::Status</ref></computeroutput>. </para>
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

auto constexpr kStructXml = R"xml(xml(xml(<?xml version="1.0" standalone="yes"?>
  <doxygen version="1.9.1" xml:lang="en-US">
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="structgoogle_1_1cloud_1_1AsyncTimerResult" kind="struct" language="C++" prot="public">
      <compoundname>google::cloud::AsyncTimerResult</compoundname>
      <includes refid="async__operation_8h" local="no">google/cloud/async_operation.h</includes>
        <sectiondef kind="public-attrib">
        <memberdef kind="variable" id="structgoogle_1_1cloud_1_1AsyncTimerResult_1a691ee4398edc058285993e47977d2127" prot="public" static="no" mutable="no">
          <type>std::chrono::system_clock::time_point</type>
          <definition>std::chrono::system_clock::time_point google::cloud::AsyncTimerResult::deadline</definition>
          <argsstring/>
          <name>deadline</name>
          <qualifiedname>google::cloud::AsyncTimerResult::deadline</qualifiedname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
          <inbodydescription>
          </inbodydescription>
          <location file="async_operation.h" line="32" column="41" bodyfile="async_operation.h" bodystart="32" bodyend="-1"/>
        </memberdef>
        <memberdef kind="variable" id="structgoogle_1_1cloud_1_1AsyncTimerResult_1a199c812e0d81c7adfd5720386cd5030a" prot="public" static="no" mutable="no">
          <type>bool</type>
          <definition>bool google::cloud::AsyncTimerResult::cancelled</definition>
          <argsstring/>
          <name>cancelled</name>
          <qualifiedname>google::cloud::AsyncTimerResult::cancelled</qualifiedname>
          <briefdescription>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
          <inbodydescription>
          </inbodydescription>
          <location file="async_operation.h" line="33" column="8" bodyfile="async_operation.h" bodystart="33" bodyend="-1"/>
        </memberdef>
        </sectiondef>
      <briefdescription>
        <para>The result of an async timer operation.</para>
      </briefdescription>
      <detaileddescription>
        <para>Callbacks for async timers will receive an object of this class.</para>
      </detaileddescription>
      <location file="async_operation.h" line="31" column="1" bodyfile="async_operation.h" bodystart="31" bodyend="34"/>
      <listofallmembers>
        <member refid="structgoogle_1_1cloud_1_1AsyncTimerResult_1a199c812e0d81c7adfd5720386cd5030a" prot="public" virt="non-virtual"><scope>google::cloud::AsyncTimerResult</scope><name>cancelled</name></member>
        <member refid="structgoogle_1_1cloud_1_1AsyncTimerResult_1a691ee4398edc058285993e47977d2127" prot="public" virt="non-virtual"><scope>google::cloud::AsyncTimerResult</scope><name>deadline</name></member>
      </listofallmembers>
    </compounddef>
  </doxygen>)xml";

auto constexpr kNamespaceXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud_1_1mocks" kind="namespace" language="C++">
        <compoundname>google::cloud::mocks</compoundname>
        <briefdescription>
          <para>Contains helpers for testing the Google Cloud C++ Client Libraries.</para>
        </briefdescription>
        <detaileddescription>
          <para>The symbols defined in this namespace are offered for public consumption.</para>
        </detaileddescription>
        <location file="mocks/mock_stream_range.h" line="30" column="1"/>
      </compounddef>
  </doxygen>)xml";

TEST(Doxygen2SyntaxContent, Enum) {
  auto constexpr kExpected = R"""(enum class google::cloud::Idempotency {
  kIdempotent,
  kNonIdempotent,
};)""";
  pugi::xml_document doc;
  doc.load_string(kEnumXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c"
      "']");
  ASSERT_TRUE(selected);
  auto const actual = EnumSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Typedef) {
  auto constexpr kExpected =
      R"""(using google::cloud::BackgroundThreadsFactory =
  std::function< std::unique_ptr< BackgroundThreads >()>;)""";
  pugi::xml_document doc;
  doc.load_string(kTypedefXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f"
      "']");
  ASSERT_TRUE(selected);
  auto const actual = TypedefSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, FriendStruct) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="friend" id="structgoogle_1_1cloud_1_1DoxygenTest2_1a4ebc3a917ee916646f8af7ce94f83248" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
        <type>struct</type>
        <definition>friend struct DoxygenTest1</definition>
        <argsstring></argsstring>
        <name>DoxygenTest1</name>
        <qualifiedname>google::cloud::DoxygenTest2::DoxygenTest1</qualifiedname>
        <param>
          <type><ref refid="structgoogle_1_1cloud_1_1DoxygenTest1" kindref="compound">DoxygenTest1</ref></type>
        </param>
        <briefdescription>
<para>The BFF friendliest friend ever. </para>
        </briefdescription>
        <detaileddescription>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="docfx/testdata/test.h" line="40" column="17" bodyfile="docfx/testdata/test.h" bodystart="40" bodyend="-1"/>
      </memberdef>
    </doxygen>)xml";
  auto constexpr kExpected =
      R"""(friend struct google::cloud::DoxygenTest2::DoxygenTest1;)""";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "structgoogle_1_1cloud_1_1DoxygenTest2_1a4ebc3a917ee916646f8af7ce94f83248"
      "']");
  ASSERT_TRUE(selected);
  auto const actual = FriendSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, FriendFunction) {
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
        </briefdescription>
        <detaileddescription>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="status.h" line="86" column="15"/>
      </memberdef>
    </doxygen>)xml";
  auto constexpr kExpected =
      R"""(friend bool
google::cloud::ErrorInfo::operator== (
    ErrorInfo const &,
    ErrorInfo const &
  ))""";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1ErrorInfo_1a3e7a9be9a728e13d3333784f63270555"
      "']");
  ASSERT_TRUE(selected);
  auto const actual = FriendSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Variable) {
  auto constexpr kExpected = R"""(Severity severity;)""";
  pugi::xml_document doc;
  doc.load_string(kVariableXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "structgoogle_1_1cloud_1_1LogRecord_1a830f8e86e1581dddbbb2cd922cbc"
      "']");
  ASSERT_TRUE(selected);
  auto const actual = VariableSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Function) {
  auto constexpr kExpected =
      R"""(template <
    typename Rep,
    typename Period>
future< StatusOr< std::chrono::system_clock::time_point > >
google::cloud::CompletionQueue::MakeRelativeTimer (
    std::chrono::duration< Rep, Period > duration
  ))""";
  pugi::xml_document doc;
  doc.load_string(docfx_testing::FunctionXml().c_str());
  auto selected = doc.select_node(
      ("//*[@id='" + docfx_testing::FunctionXmlId() + "']").c_str());
  ASSERT_TRUE(selected);
  auto const actual = FunctionSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Constructor) {
  auto constexpr kExpected =
      R"""(google::cloud::RuntimeStatusError::RuntimeStatusError (
    Status status
  ))""";
  pugi::xml_document doc;
  doc.load_string(kClassXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1RuntimeStatusError"
      "_1aac6b78160cce6468696ce77eb1276a95']");
  ASSERT_TRUE(selected);
  auto const actual = FunctionSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Struct) {
  auto constexpr kExpected =
      R"""(// Found in #include "google/cloud/async_operation.h"
struct google::cloud::AsyncTimerResult { ... };)""";
  pugi::xml_document doc;
  doc.load_string(kStructXml);
  auto selected =
      doc.select_node("//*[@id='structgoogle_1_1cloud_1_1AsyncTimerResult']");
  ASSERT_TRUE(selected);
  auto const actual = StructSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Class) {
  auto constexpr kExpected =
      R"""(// Found in #include "google/cloud/status.h"
class google::cloud::RuntimeStatusError { ... };)""";
  pugi::xml_document doc;
  doc.load_string(kClassXml);
  auto selected =
      doc.select_node("//*[@id='classgoogle_1_1cloud_1_1RuntimeStatusError']");
  ASSERT_TRUE(selected);
  auto const actual = ClassSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, TemplateClass) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1StatusOr" kind="class" language="C++" prot="public" final="yes">
    <compoundname>google::cloud::StatusOr</compoundname>
    <includes refid="status__or_8h" local="no">google/cloud/status_or.h</includes>
    <templateparamlist>
      <param>
        <type>typename T</type>
      </param>
    </templateparamlist>
    <briefdescription>
<para>Holds a value or a <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput> indicating why there is no value.</para>
    </briefdescription>
    <location file="status_or.h" line="89" column="1" bodyfile="status_or.h" bodystart="89" bodyend="279"/>
  </compounddef>)xml";

  auto constexpr kExpected =
      R"""(// Found in #include "google/cloud/status_or.h"
template <
    typename T>
class google::cloud::StatusOr { ... };)""";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected =
      doc.select_node("//*[@id='classgoogle_1_1cloud_1_1StatusOr']");
  ASSERT_TRUE(selected);
  auto const actual = ClassSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, TemplateStruct) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="structgoogle_1_1cloud_1_1StatusOr" kind="struct" language="C++" prot="public" final="yes">
    <compoundname>google::cloud::StatusOr</compoundname>
    <includes refid="status__or_8h" local="no">google/cloud/status_or.h</includes>
    <templateparamlist>
      <param>
        <type>typename T</type>
      </param>
    </templateparamlist>
    <briefdescription>
      <para>Holds a value or a <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput> indicating why there is no value.</para>
    </briefdescription>
    <location file="status_or.h" line="89" column="1" bodyfile="status_or.h" bodystart="89" bodyend="279"/>
  </compounddef>)xml";

  auto constexpr kExpected =
      R"""(// Found in #include "google/cloud/status_or.h"
template <
    typename T>
struct google::cloud::StatusOr { ... };)""";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected =
      doc.select_node("//*[@id='structgoogle_1_1cloud_1_1StatusOr']");
  ASSERT_TRUE(selected);
  auto const actual = StructSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, Namespace) {
  auto constexpr kExpected = R"""(namespace google::cloud::mocks { ... };)""";
  pugi::xml_document doc;
  doc.load_string(kNamespaceXml);
  auto selected =
      doc.select_node("//*[@id='namespacegoogle_1_1cloud_1_1mocks']");
  ASSERT_TRUE(selected);
  auto const actual = NamespaceSyntaxContent(selected.node());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Enum) {
  auto constexpr kExpected = R"yml(syntax:
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
      path: google/cloud/idempotency.h)yml";

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
  yaml << YAML::BeginMap;
  AppendEnumSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Typedef) {
  auto constexpr kExpected = R"yml(syntax:
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
      path: google/cloud/grpc_options.h)yml";

  pugi::xml_document doc;
  doc.load_string(kTypedefXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "namespacegoogle_1_1cloud_1a1498c1ea55d81842f37bbc42d003df1f"
      "']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendTypedefSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Friend) {
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
        </briefdescription>
        <detaileddescription>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="status.h" line="86" column="15"/>
      </memberdef>
    </doxygen>)xml";

  auto constexpr kExpected = R"yml(syntax:
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
      path: google/cloud/status.h)yml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1ErrorInfo_1a3e7a9be9a728e13d3333784f63270555"
      "']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendFriendSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Variable) {
  auto constexpr kExpected = R"yml(syntax:
  contents: |
    Severity severity;
  source:
    id: severity
    path: google/cloud/log.h
    startLine: 152
    remote:
      repo: https://github.com/googleapis/google-cloud-cpp/
      branch: main
      path: google/cloud/log.h)yml";

  pugi::xml_document doc;
  doc.load_string(kVariableXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "structgoogle_1_1cloud_1_1LogRecord_1a830f8e86e1581dddbbb2cd922cbc"
      "']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendVariableSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Function) {
  auto constexpr kExpected = R"yml(syntax:
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
      path: google/cloud/completion_queue.h)yml";
  pugi::xml_document doc;
  doc.load_string(docfx_testing::FunctionXml().c_str());
  auto selected = doc.select_node(
      ("//*[@id='" + docfx_testing::FunctionXmlId() + "']").c_str());
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendFunctionSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Constructor) {
  auto constexpr kExpected = R"yml(syntax:
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
      path: google/cloud/status.h)yml";
  pugi::xml_document doc;
  doc.load_string(kClassXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1RuntimeStatusError"
      "_1aac6b78160cce6468696ce77eb1276a95']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendFunctionSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2SyntaxContent, FunctionException) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
      <memberdef kind="function" id="classgoogle_1_1cloud_1_1future_1a23b7c9cabdcf116d3b908c32e627c7af" prot="public" static="no" const="no" explicit="no" inline="yes" virt="non-virtual">
        <type>T</type>
        <definition>T google::cloud::future&lt; T &gt;::get</definition>
        <argsstring>()</argsstring>
        <name>get</name>
        <qualifiedname>google::cloud::future::get</qualifiedname>
        <briefdescription>
          <para>Waits until the shared state becomes ready, then retrieves the value stored in the shared state. </para>
        </briefdescription>
        <detaileddescription>
          <para><simplesect kind="note"><para>This operation invalidates the future, subsequent calls will fail, the application should capture the returned value because it would.</para>
            </simplesect>
            <parameterlist kind="exception"><parameteritem>
            <parameternamelist>
            <parametername>...</parametername>
            </parameternamelist>
            <parameterdescription>
            <para>any exceptions stored in the shared state.</para>
            </parameterdescription>
            </parameteritem>
            <parameteritem>
            <parameternamelist>
            <parametername>std::future_error</parametername>
            </parameternamelist>
            <parameterdescription>
            <para>with std::no_state if the future does not have a shared state.</para>
            </parameterdescription>
            </parameteritem>
            </parameterlist>
          </para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="future_generic.h" line="84" column="5" bodyfile="future_generic.h" bodystart="84" bodyend="89"/>
      </memberdef>)xml";

  auto constexpr kExpected =
      R"""(syntax:
  contents: |
    T
    google::cloud::future::get ()
  return:
    type:
      - "T"
  exceptions:
    - var_type: "..."
      description: |
        any exceptions stored in the shared state.
    - var_type: "std::future_error"
      description: |
        with std::no_state if the future does not have a shared state.
  source:
    id: get
    path: google/cloud/future_generic.h
    startLine: 84
    remote:
      repo: https://github.com/googleapis/google-cloud-cpp/
      branch: main
      path: google/cloud/future_generic.h)""";
  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//"
      "*[@id='classgoogle_1_1cloud_1_1future_"
      "1a23b7c9cabdcf116d3b908c32e627c7af']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendFunctionSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Class) {
  auto constexpr kExpected = R"yml(syntax:
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
      path: google/cloud/status.h)yml";
  pugi::xml_document doc;
  doc.load_string(kClassXml);
  auto selected =
      doc.select_node("//*[@id='classgoogle_1_1cloud_1_1RuntimeStatusError']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendClassSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Struct) {
  auto constexpr kExpected = R"yml(syntax:
  contents: |
    // Found in #include "google/cloud/async_operation.h"
    struct google::cloud::AsyncTimerResult { ... };
  source:
    id: google::cloud::AsyncTimerResult
    path: google/cloud/async_operation.h
    startLine: 31
    remote:
      repo: https://github.com/googleapis/google-cloud-cpp/
      branch: main
      path: google/cloud/async_operation.h)yml";
  pugi::xml_document doc;
  doc.load_string(kStructXml);
  auto selected =
      doc.select_node("//*[@id='structgoogle_1_1cloud_1_1AsyncTimerResult']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendStructSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Syntax, Namespace) {
  auto constexpr kExpected = R"yml(syntax:
  contents: |
    namespace google::cloud::mocks { ... };
  source:
    id: google::cloud::mocks
    path: google/cloud/mocks/mock_stream_range.h
    startLine: 30
    remote:
      repo: https://github.com/googleapis/google-cloud-cpp/
      branch: main
      path: google/cloud/mocks/mock_stream_range.h)yml";
  pugi::xml_document doc;
  doc.load_string(kNamespaceXml);
  auto selected =
      doc.select_node("//*[@id='namespacegoogle_1_1cloud_1_1mocks']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  YAML::Emitter yaml;
  yaml << YAML::BeginMap;
  AppendNamespaceSyntax(yaml, ctx, selected.node());
  yaml << YAML::EndMap;
  auto const actual = std::string{yaml.c_str()};
  EXPECT_EQ(actual, kExpected);
}

}  // namespace
}  // namespace docfx
