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
<para>Create a timer that fires after the <computeroutput>duration</computeroutput>. </para>
        </briefdescription>
        <detaileddescription>
<para><parameterlist kind="templateparam"><parameteritem>
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
  doc.load_string(kFunctionXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1CompletionQueue_"
      "1a760d68ec606a03ab8cc80eea8bd965b3"
      "']");
  ASSERT_TRUE(selected);
  auto const actual = FunctionSyntaxContent(selected.node());
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
      path: google/cloud/completion_queue.h)yml";
  pugi::xml_document doc;
  doc.load_string(kFunctionXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "classgoogle_1_1cloud_1_1CompletionQueue_"
      "1a760d68ec606a03ab8cc80eea8bd965b3"
      "']");
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

}  // namespace
}  // namespace docfx
