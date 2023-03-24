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

TEST(Doxygen2Yaml, EnumValue) {
  auto constexpr kExpected =
      R"yml(uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
name: |
  kIdempotent
id: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
parent: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
type: enumvalue
langs:
  - cpp
summary: |
  The operation is idempotent and can be retried after a transient failure.)yml";

  pugi::xml_document doc;
  doc.load_string(kEnumXml);
  auto selected = doc.select_node(
      "//"
      "*[@id='namespacegoogle_1_1cloud_"
      "1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  YamlContext ctx;
  ctx.parent_id = "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c";
  ASSERT_TRUE(AppendIfEnumValue(yaml, ctx, selected.node()));

  auto const actual = std::string(yaml.c_str());
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Yaml, Enum) {
  auto constexpr kExpected = R"yml(items:
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
      The operation is not idempotent and should **not** be retried after a transient failure.)yml";

  pugi::xml_document doc;
  doc.load_string(kEnumXml);
  auto selected = doc.select_node(
      "//*[@id='"
      "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c"
      "']");
  ASSERT_TRUE(selected);
  YAML::Emitter yaml;
  yaml << YAML::BeginMap  //
       << YAML::Key << "items" << YAML::Value << YAML::BeginSeq;
  YamlContext ctx;
  ctx.parent_id = "test-only-parent-id";
  ASSERT_TRUE(AppendIfEnum(yaml, ctx, selected.node()));
  yaml << YAML::EndSeq << YAML::EndMap;

  auto const actual = std::string(yaml.c_str());
  EXPECT_EQ(actual, kExpected);
}

}  // namespace
}  // namespace docfx
