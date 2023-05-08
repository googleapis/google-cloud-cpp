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

#include "docfx/doxygen2references.h"
#include "docfx/testing/inputs.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

using ::testing::UnorderedElementsAre;

TEST(Doxygen2ReferencesTest, Class) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
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
      <location file="status.h" line="161" column="1" bodyfile="status.h" bodystart="161" bodyend="169"/>
      <listofallmembers>
        <member refid="classgoogle_1_1cloud_1_1RuntimeStatusError_1aac6b78160cce6468696ce77eb1276a95" prot="public" virt="non-virtual"><scope>google::cloud::RuntimeStatusError</scope><name>RuntimeStatusError</name></member>
        <member refid="classgoogle_1_1cloud_1_1RuntimeStatusError_1ac30dbdb272a62aee4eb8f9bf45966c7e" prot="public" virt="non-virtual"><scope>google::cloud::RuntimeStatusError</scope><name>status</name></member>
        <member refid="classgoogle_1_1cloud_1_1RuntimeStatusError_1a85bebd1a98468aff6b7f5fe54f7b4241" prot="private" virt="non-virtual"><scope>google::cloud::RuntimeStatusError</scope><name>status_</name></member>
      </listofallmembers>
        </compounddef>
    </doxygen>)xml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected =
      doc.select_node("//*[@id='classgoogle_1_1cloud_1_1RuntimeStatusError']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  auto references = ExtractReferences(ctx, selected.node());
  EXPECT_THAT(
      references,
      UnorderedElementsAre(
          Reference("classgoogle_1_1cloud_1_1RuntimeStatusError",
                    "google::cloud::RuntimeStatusError"),
          Reference("classgoogle_1_1cloud_1_1RuntimeStatusError_"
                    "1aac6b78160cce6468696ce77eb1276a95",
                    "google::cloud::RuntimeStatusError::RuntimeStatusError"),
          Reference("classgoogle_1_1cloud_1_1RuntimeStatusError_"
                    "1ac30dbdb272a62aee4eb8f9bf45966c7e",
                    "google::cloud::RuntimeStatusError::status")));
}

TEST(Doxygen2ReferencesTest, Namespace) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud" kind="namespace" language="C++">
        <compoundname>google::cloud</compoundname>
        <innerclass refid="structgoogle_1_1cloud_1_1AccessTokenLifetimeOption" prot="public">google::cloud::AccessTokenLifetimeOption</innerclass>
        <innerclass refid="classgoogle_1_1cloud_1_1AsyncOperation" prot="public">google::cloud::AsyncOperation</innerclass>
        <innernamespace refid="namespacegoogle_1_1cloud_1_1mocks">google::cloud::mocks</innernamespace>
        <memberdef kind="enum" id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c" prot="public" static="no" strong="yes">
            <name>Idempotency</name>
            <qualifiedname>google::cloud::Idempotency</qualifiedname>
        </memberdef>
      </compounddef>
    </doxygen>)xml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='namespacegoogle_1_1cloud']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  ctx.config.library = "cloud";
  auto references = ExtractReferences(ctx, selected.node());
  EXPECT_THAT(
      references,
      UnorderedElementsAre(
          Reference("namespacegoogle_1_1cloud", "google::cloud"),
          Reference("structgoogle_1_1cloud_1_1AccessTokenLifetimeOption",
                    "google::cloud::AccessTokenLifetimeOption"),
          Reference("classgoogle_1_1cloud_1_1AsyncOperation",
                    "google::cloud::AsyncOperation"),
          Reference(
              "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c",
              "google::cloud::Idempotency")));
}

TEST(Doxygen2ReferencesTest, Enum) {
  auto constexpr kXml = R"xml(xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="enum" id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c" prot="public" static="no" strong="yes">
        <type/>
        <name>Idempotency</name>
        <qualifiedname>google::cloud::Idempotency</qualifiedname>
        <enumvalue id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa" prot="public">
          <name>kIdempotent</name>
          <briefdescription>
<para>The operation is idempotent and can be retried after a transient failure. </para>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
        </enumvalue>
        <enumvalue id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190" prot="public">
          <name>kNonIdempotent</name>
          <briefdescription>
<para>The operation is not idempotent and should <bold>not</bold> be retried after a transient failure. </para>
          </briefdescription>
          <detaileddescription>
          </detaileddescription>
        </enumvalue>
        <briefdescription>
<para>Whether a request is <ulink url="https://en.wikipedia.org/wiki/Idempotence">idempotent</ulink>. </para>
        </briefdescription>
        <detaileddescription>
<para>When a RPC fails with a retryable error, the <computeroutput>google-cloud-cpp</computeroutput> client libraries automatically retry the RPC <bold>if</bold> the RPC is <ulink url="https://en.wikipedia.org/wiki/Idempotence">idempotent</ulink>. For each service, the library define a policy that determines whether a given request is idempotent. In many cases this can be determined statically, for example, read-only operations are always idempotent. In some cases, the contents of the request may need to be examined to determine if the operation is idempotent. For example, performing operations with pre-conditions, such that the pre-conditions change when the operation succeed, is typically idempotent.</para>
<para>Applications may override the default idempotency policy, though we anticipate that this would be needed only in very rare circumstances. A few examples include:</para>
<para><itemizedlist>
<listitem><para>In some services deleting "the most recent" entry may be idempotent if the system has been configured to keep no history or versions, as the deletion may succeed only once. In contrast, deleting "the most recent entry" is <bold>not</bold> idempotent if the system keeps multiple versions. Google Cloud Storage or Bigtable can be configured either way.</para>
</listitem><listitem><para>In some applications, creating a duplicate entry may be acceptable as the system will deduplicate them later. In such systems it may be preferable to retry the operation even though it is not idempotent. </para>
</listitem></itemizedlist>
</para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="idempotency.h" line="55" column="1" bodyfile="idempotency.h" bodystart="55" bodyend="61"/>
      </memberdef>
    </doxygen>)xml";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node(
      "//*[@id='namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c']");
  ASSERT_TRUE(selected);
  YamlContext ctx;
  auto references = ExtractReferences(ctx, selected.node());
  EXPECT_THAT(
      references,
      UnorderedElementsAre(
          Reference(
              "namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c",
              "google::cloud::Idempotency"),
          Reference("namespacegoogle_1_1cloud_"
                    "1a7d65fd569564712b7cfe652613f30d9caf8bb1d"
                    "9c7cccc450ecd06167c7422bfa",
                    "kIdempotent"),
          Reference("namespacegoogle_1_1cloud_"
                    "1a7d65fd569564712b7cfe652613f30d9cae75d33"
                    "e94f2dc4028d4d67bdaab75190",
                    "kNonIdempotent")));
}

TEST(Doxygen2ReferencesTest, MockedFunctions) {
  pugi::xml_document doc;
  doc.load_string(docfx_testing::MockClass().c_str());
  auto const filter = "//*[@id='" + docfx_testing::MockClassId() + "']";
  auto const selected = doc.select_node(filter.c_str());
  ASSERT_TRUE(selected);

  YamlContext parent;
  parent.config.library = "kms";
  auto const references = ExtractReferences(parent, selected.node());
  EXPECT_THAT(
      references,
      UnorderedElementsAre(
          Reference("classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_"
                    "1MockKeyDashboardServiceConnection",
                    "google::cloud::kms_inventory_v1_mocks::"
                    "MockKeyDashboardServiceConnection"),
          Reference("classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_"
                    "1MockKeyDashboardServiceConnection_"
                    "1a2bf84b7b96702bc1622f0e6c9f0babc4",
                    "google::cloud::kms_inventory_v1_mocks::"
                    "MockKeyDashboardServiceConnection::options"),
          Reference("classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_"
                    "1MockKeyDashboardServiceConnection_"
                    "1a789db998d71abf9016b64832d0c7a99e",
                    "google::cloud::kms_inventory_v1_mocks::"
                    "MockKeyDashboardServiceConnection::ListCryptoKeys")));
}

}  // namespace
}  // namespace docfx
