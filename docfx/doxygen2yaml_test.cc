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

}  // namespace
}  // namespace docfx
