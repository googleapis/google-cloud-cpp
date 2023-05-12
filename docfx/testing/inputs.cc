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

#include "docfx/testing/inputs.h"

namespace docfx_testing {

std::string MockClass() {
  return R"xml(<?xml version='1.0' encoding='UTF-8' standalone='no'?>
    <doxygen xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="compound.xsd" version="1.9.5" xml:lang="en-US">
    <compounddef id="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection" kind="class" language="C++" prot="public">
        <compoundname>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</compoundname>
        <basecompoundref refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection" prot="public" virt="non-virtual">google::cloud::kms_inventory_v1::KeyDashboardServiceConnection</basecompoundref>
        <includes refid="mock__key__dashboard__connection_8h" local="no">google/cloud/kms/inventory/v1/mocks/mock_key_dashboard_connection.h</includes>
        <sectiondef kind="public-func">
        <memberdef kind="function" id="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection_1a2bf84b7b96702bc1622f0e6c9f0babc4" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
            <type></type>
            <definition>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection::MOCK_METHOD</definition>
            <argsstring>(Options, options,(),(override))</argsstring>
            <name>MOCK_METHOD</name>
            <qualifiedname>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection::MOCK_METHOD</qualifiedname>
            <param>
            <type><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound" external="/workspace/cmake-out/google/cloud/cloud.tag">Options</ref></type>
            </param>
            <param>
            <type><ref refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a922ac7ae75f6939947a07d843e863845" kindref="member">options</ref></type>
            </param>
            <param>
            <type>()</type>
            </param>
            <param>
            <type>(override)</type>
            </param>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="inventory/v1/mocks/mock_key_dashboard_connection.h" line="48" column="3"/>
        </memberdef>
        <memberdef kind="function" id="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection_1a789db998d71abf9016b64832d0c7a99e" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
            <type></type>
            <definition>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection::MOCK_METHOD</definition>
            <argsstring>(StreamRange&lt; google::cloud::kms::v1::CryptoKey &gt;, ListCryptoKeys,(google::cloud::kms::inventory::v1::ListCryptoKeysRequest request),(override))</argsstring>
            <name>MOCK_METHOD</name>
            <qualifiedname>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection::MOCK_METHOD</qualifiedname>
            <param>
            <type><ref refid="classgoogle_1_1cloud_1_1StreamRange" kindref="compound" external="/workspace/cmake-out/google/cloud/cloud.tag">StreamRange</ref>&lt; google::cloud::kms::v1::CryptoKey &gt;</type>
            </param>
            <param>
            <type><ref refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a2518e5014c3adbc16e83281bd2a596a8" kindref="member">ListCryptoKeys</ref></type>
            </param>
            <param>
            <type>(google::cloud::kms::inventory::v1::ListCryptoKeysRequest request)</type>
            </param>
            <param>
            <type>(override)</type>
            </param>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="inventory/v1/mocks/mock_key_dashboard_connection.h" line="50" column="3"/>
        </memberdef>
        <memberdef kind="function" id="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a922ac7ae75f6939947a07d843e863845" prot="public" static="no" const="no" explicit="no" inline="yes" virt="virtual">
            <type><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound" external="/workspace/cmake-out/google/cloud/cloud.tag">Options</ref></type>
            <definition>virtual Options google::cloud::kms_inventory_v1::KeyDashboardServiceConnection::options</definition>
            <argsstring>()</argsstring>
            <name>options</name>
            <qualifiedname>google::cloud::kms_inventory_v1::KeyDashboardServiceConnection::options</qualifiedname>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="inventory/v1/key_dashboard_connection.h" line="65" column="19" bodyfile="inventory/v1/key_dashboard_connection.h" bodystart="65" bodyend="65"/>
        </memberdef>
        <memberdef kind="function" id="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a2518e5014c3adbc16e83281bd2a596a8" prot="public" static="no" const="no" explicit="no" inline="no" virt="virtual">
            <type><ref refid="classgoogle_1_1cloud_1_1StreamRange" kindref="compound" external="/workspace/cmake-out/google/cloud/cloud.tag">StreamRange</ref>&lt; google::cloud::kms::v1::CryptoKey &gt;</type>
            <definition>virtual StreamRange&lt; google::cloud::kms::v1::CryptoKey &gt; google::cloud::kms_inventory_v1::KeyDashboardServiceConnection::ListCryptoKeys</definition>
            <argsstring>(google::cloud::kms::inventory::v1::ListCryptoKeysRequest request)</argsstring>
            <name>ListCryptoKeys</name>
            <qualifiedname>google::cloud::kms_inventory_v1::KeyDashboardServiceConnection::ListCryptoKeys</qualifiedname>
            <param>
            <type>google::cloud::kms::inventory::v1::ListCryptoKeysRequest</type>
            <declname>request</declname>
            </param>
            <briefdescription>
            </briefdescription>
            <detaileddescription>
            </detaileddescription>
            <inbodydescription>
            </inbodydescription>
            <location file="inventory/v1/key_dashboard_connection.h" line="67" column="23"/>
        </memberdef>
        </sectiondef>
        <briefdescription>
    <para>A class to mock <computeroutput>KeyDashboardServiceConnection</computeroutput>. </para>
        </briefdescription>
        <detaileddescription>
    <para>Application developers may want to test their code with simulated responses, including errors, from an object of type <computeroutput>KeyDashboardServiceClient</computeroutput>. To do so, construct an object of type <computeroutput>KeyDashboardServiceClient</computeroutput> with an instance of this class. Then use the Google Test framework functions to program the behavior of this mock.</para>
    <para><simplesect kind="see"><para><ulink url="https://googleapis.dev/cpp/google-cloud-bigquery/HEAD/bigquery-read-mock.html">This example</ulink> for how to test your application with GoogleTest. While the example showcases types from the BigQuery library, the underlying principles apply for any pair of <computeroutput>*Client</computeroutput> and <computeroutput>*Connection</computeroutput>. </para>
    </simplesect>
    </para>
        </detaileddescription>
        <inheritancegraph>
        <node id="2">
            <label>google::cloud::kms_inventory_v1::KeyDashboardServiceConnection</label>
            <link refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection"/>
        </node>
        <node id="1">
            <label>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</label>
            <link refid="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection"/>
            <childnode refid="2" relation="public-inheritance">
            </childnode>
        </node>
        </inheritancegraph>
        <collaborationgraph>
        <node id="2">
            <label>google::cloud::kms_inventory_v1::KeyDashboardServiceConnection</label>
            <link refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection"/>
        </node>
        <node id="1">
            <label>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</label>
            <link refid="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection"/>
            <childnode refid="2" relation="public-inheritance">
            </childnode>
        </node>
        </collaborationgraph>
        <location file="inventory/v1/mocks/mock_key_dashboard_connection.h" line="45" column="1" bodyfile="inventory/v1/mocks/mock_key_dashboard_connection.h" bodystart="46" bodyend="54"/>
        <listofallmembers>
        <member refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a2518e5014c3adbc16e83281bd2a596a8" prot="public" virt="virtual"><scope>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</scope><name>ListCryptoKeys</name></member>
        <member refid="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection_1a2bf84b7b96702bc1622f0e6c9f0babc4" prot="public" virt="non-virtual"><scope>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</scope><name>MOCK_METHOD</name></member>
        <member refid="classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1MockKeyDashboardServiceConnection_1a789db998d71abf9016b64832d0c7a99e" prot="public" virt="non-virtual"><scope>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</scope><name>MOCK_METHOD</name></member>
        <member refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a922ac7ae75f6939947a07d843e863845" prot="public" virt="virtual"><scope>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</scope><name>options</name></member>
        <member refid="classgoogle_1_1cloud_1_1kms__inventory__v1_1_1KeyDashboardServiceConnection_1a9f4e392a66326a1ed2fce8780c76c5e5" prot="public" virt="pure-virtual"><scope>google::cloud::kms_inventory_v1_mocks::MockKeyDashboardServiceConnection</scope><name>~KeyDashboardServiceConnection</name></member>
        </listofallmembers>
    </compounddef>
    </doxygen>)xml";
}

std::string MockClassId() {
  return "classgoogle_1_1cloud_1_1kms__inventory__v1__mocks_1_1"
         "MockKeyDashboardServiceConnection";
}

std::string MockedFunctionId() {
  return "classgoogle_1_1cloud_1_1kms__inventory__v1_1_1"
         "KeyDashboardServiceConnection_1a2518e5014c3adbc16e83281bd2a596a8";
}

std::string FunctionXml() {
  return R"xml(<?xml version="1.0" standalone="yes"?>
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
<para>a placeholder to match the Rep tparam for <computeroutput>duration</computeroutput> type, the semantics of this template parameter are documented in <computeroutput>std::chrono::duration&lt;&gt;</computeroutput> (in brief, the underlying arithmetic type used to store the number of ticks), for our purposes it is simply a formal parameter.</para>
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
<simplesect kind="return"><para>a future that becomes satisfied after <computeroutput>duration</computeroutput> time has elapsed. The result of the future is the time at which it expired, or an error <ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> if the timer did not run to expiration (e.g. it was cancelled).</para>
</simplesect>
</para>
        </detaileddescription>
        <inbodydescription>
        </inbodydescription>
        <location file="completion_queue.h" line="96" column="10" bodyfile="completion_queue.h" bodystart="96" bodyend="100"/>
      </memberdef>
    </doxygen>)xml";
}

std::string FunctionXmlId() {
  return "classgoogle_1_1cloud_1_1CompletionQueue_"
         "1a760d68ec606a03ab8cc80eea8bd965b3";
}

std::string StatusClassXml() {
  return R"""(<?xml version='1.0' encoding='UTF-8' standalone='no'?>
    <doxygen xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="compound.xsd" version="1.9.5" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle" kind="namespace" language="C++">
        <compoundname>google</compoundname>
        <innernamespace refid="namespacegoogle_1_1cloud">google::cloud</innernamespace>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud" kind="namespace" language="C++">
        <compoundname>google::cloud</compoundname>
        <innerclass refid="classgoogle_1_1cloud_1_1Status">google::cloud::Status</innerclass>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1Status" kind="class" language="C++" prot="public">
          <compoundname>google::cloud::Status</compoundname>
          <includes refid="status_8h" local="no">google/cloud/status.h</includes>
            <sectiondef kind="user-defined">
            <header>Copy construction and assignment.</header>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1aa3656155ad44d8bd75d92cd797123f4d" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
              <type/>
              <definition>google::cloud::Status::Status</definition>
              <argsstring>(Status const &amp;)</argsstring>
              <name>Status</name>
              <qualifiedname>google::cloud::Status::Status</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="305" column="3"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1a675be7e53ab9b27d69ba776a3c1ca7bf" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
              <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> &amp;</type>
              <definition>Status &amp; google::cloud::Status::operator=</definition>
              <argsstring>(Status const &amp;)</argsstring>
              <name>operator=</name>
              <qualifiedname>google::cloud::Status::operator=</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="306" column="10"/>
            </memberdef>
            </sectiondef>
            <sectiondef kind="user-defined">
            <header>Move construction and assignment.</header>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1afd186465a07fa176c10d437c1240f2de" prot="public" static="no" const="no" explicit="no" inline="no" noexcept="yes" virt="non-virtual">
              <type/>
              <definition>google::cloud::Status::Status</definition>
              <argsstring>(Status &amp;&amp;) noexcept</argsstring>
              <name>Status</name>
              <qualifiedname>google::cloud::Status::Status</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> &amp;&amp;</type>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="312" column="3"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1a23aaae701351564b3a17f47d5cb4e7cb" prot="public" static="no" const="no" explicit="no" inline="no" noexcept="yes" virt="non-virtual">
              <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> &amp;</type>
              <definition>Status &amp; google::cloud::Status::operator=</definition>
              <argsstring>(Status &amp;&amp;) noexcept</argsstring>
              <name>operator=</name>
              <qualifiedname>google::cloud::Status::operator=</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> &amp;&amp;</type>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="313" column="10"/>
            </memberdef>
            </sectiondef>
            <sectiondef kind="private-attrib">
            <memberdef kind="variable" id="classgoogle_1_1cloud_1_1Status_1a520fa011bc1cb95cda684a7b8ba701f3" prot="private" static="no" mutable="no">
              <type>std::unique_ptr&lt; Impl &gt;</type>
              <definition>std::unique_ptr&lt;Impl&gt; google::cloud::Status::impl_</definition>
              <argsstring/>
              <name>impl_</name>
              <qualifiedname>google::cloud::Status::impl_</qualifiedname>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="363" column="19" bodyfile="status.h" bodystart="363" bodyend="-1"/>
            </memberdef>
            </sectiondef>
            <sectiondef kind="public-func">
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1af3de0fb0dee8fb557e693195a812987f" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
              <type/>
              <definition>google::cloud::Status::Status</definition>
              <argsstring>()</argsstring>
              <name>Status</name>
              <qualifiedname>google::cloud::Status::Status</qualifiedname>
              <briefdescription>
      <para>Default constructor, initializes to <computeroutput><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847ae69fa9a656f76dd8a4d89f21992b2d3a" kindref="member">StatusCode::kOk</ref></computeroutput>. </para>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="298" column="3"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1a739165d43975222a55f064dd87db5e1f" prot="public" static="no" const="no" explicit="no" inline="no" virt="non-virtual">
              <type/>
              <definition>google::cloud::Status::~Status</definition>
              <argsstring>()</argsstring>
              <name>~Status</name>
              <qualifiedname>google::cloud::Status::~Status</qualifiedname>
              <briefdescription>
      <para>Destructor. </para>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="300" column="3"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1af927a89141bbcf10f0e02d789ebade94" prot="public" static="no" const="no" explicit="yes" inline="no" virt="non-virtual">
              <type/>
              <definition>google::cloud::Status::Status</definition>
              <argsstring>(StatusCode code, std::string message, ErrorInfo info={})</argsstring>
              <name>Status</name>
              <qualifiedname>google::cloud::Status::Status</qualifiedname>
              <param>
                <type><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847" kindref="member">StatusCode</ref></type>
                <declname>code</declname>
              </param>
              <param>
                <type>std::string</type>
                <declname>message</declname>
              </param>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1ErrorInfo" kindref="compound">ErrorInfo</ref></type>
                <declname>info</declname>
                <defval>{}</defval>
              </param>
              <briefdescription>
      <para>Construct from a status code, message and (optional) error info. </para>
              </briefdescription>
              <detaileddescription>
      <para><parameterlist kind="param"><parameteritem>
      <parameternamelist>
      <parametername>code</parametername>
      </parameternamelist>
      <parameterdescription>
      <para>the status code for the new <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput>. </para>
      </parameterdescription>
      </parameteritem>
      <parameteritem>
      <parameternamelist>
      <parametername>message</parametername>
      </parameternamelist>
      <parameterdescription>
      <para>the message for the new <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput>, ignored if <computeroutput>code</computeroutput> is <computeroutput><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847ae69fa9a656f76dd8a4d89f21992b2d3a" kindref="member">StatusCode::kOk</ref></computeroutput>. </para>
      </parameterdescription>
      </parameteritem>
      <parameteritem>
      <parameternamelist>
      <parametername>info</parametername>
      </parameternamelist>
      <parameterdescription>
      <para>the <computeroutput><ref refid="classgoogle_1_1cloud_1_1ErrorInfo" kindref="compound">ErrorInfo</ref></computeroutput> for the new <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput>, ignored if <computeroutput>code</computeroutput> is <computeroutput>SStatusCode::kOk</computeroutput>. </para>
      </parameterdescription>
      </parameteritem>
      </parameterlist>
      </para>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="325" column="12"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1a18952043ffe5a4f74911c8146e8bb504" prot="public" static="no" const="yes" explicit="no" inline="yes" virt="non-virtual">
              <type>bool</type>
              <definition>bool google::cloud::Status::ok</definition>
              <argsstring>() const</argsstring>
              <name>ok</name>
              <qualifiedname>google::cloud::Status::ok</qualifiedname>
              <briefdescription>
      <para>Returns true if the status code is <computeroutput><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847ae69fa9a656f76dd8a4d89f21992b2d3a" kindref="member">StatusCode::kOk</ref></computeroutput>. </para>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="328" column="8" bodyfile="status.h" bodystart="328" bodyend="328"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1ac18337d2ceb00ca007725d21f2c63f9f" prot="public" static="no" const="yes" explicit="no" inline="no" virt="non-virtual">
              <type><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847" kindref="member">StatusCode</ref></type>
              <definition>StatusCode google::cloud::Status::code</definition>
              <argsstring>() const</argsstring>
              <name>code</name>
              <qualifiedname>google::cloud::Status::code</qualifiedname>
              <briefdescription>
      <para>Returns the status code. </para>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="331" column="14"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1aaa8dea39008758d8494f29b12b92be02" prot="public" static="no" const="yes" explicit="no" inline="no" virt="non-virtual">
              <type>std::string const &amp;</type>
              <definition>std::string const  &amp; google::cloud::Status::message</definition>
              <argsstring>() const</argsstring>
              <name>message</name>
              <qualifiedname>google::cloud::Status::message</qualifiedname>
              <briefdescription>
      <para>Returns the message associated with the status. </para>
              </briefdescription>
              <detaileddescription>
      <para>This is always empty if <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status_1ac18337d2ceb00ca007725d21f2c63f9f" kindref="member">code()</ref></computeroutput> is <computeroutput><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847ae69fa9a656f76dd8a4d89f21992b2d3a" kindref="member">StatusCode::kOk</ref></computeroutput>. </para>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="338" column="21"/>
            </memberdef>
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1a172e846ab5623d78d49e2ed128f49583" prot="public" static="no" const="yes" explicit="no" inline="no" virt="non-virtual">
              <type><ref refid="classgoogle_1_1cloud_1_1ErrorInfo" kindref="compound">ErrorInfo</ref> const &amp;</type>
              <definition>ErrorInfo const  &amp; google::cloud::Status::error_info</definition>
              <argsstring>() const</argsstring>
              <name>error_info</name>
              <qualifiedname>google::cloud::Status::error_info</qualifiedname>
              <briefdescription>
      <para>Returns the additional error info associated with the status. </para>
              </briefdescription>
              <detaileddescription>
      <para>This is always a default-constructed error info if <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status_1ac18337d2ceb00ca007725d21f2c63f9f" kindref="member">code()</ref></computeroutput> is <computeroutput><ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847ae69fa9a656f76dd8a4d89f21992b2d3a" kindref="member">StatusCode::kOk</ref></computeroutput>. </para>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="346" column="19"/>
            </memberdef>
            </sectiondef>
            <sectiondef kind="friend">
            <memberdef kind="friend" id="classgoogle_1_1cloud_1_1Status_1a8c00daab4bca2eeb428f816fabf59492" prot="public" static="no" const="no" explicit="no" inline="yes" virt="non-virtual">
              <type>bool</type>
              <definition>bool operator==</definition>
              <argsstring>(Status const &amp;a, Status const &amp;b)</argsstring>
              <name>operator==</name>
              <qualifiedname>google::cloud::Status::operator==</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
                <declname>a</declname>
              </param>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
                <declname>b</declname>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="348" column="22" bodyfile="status.h" bodystart="348" bodyend="350"/>
            </memberdef>
            <memberdef kind="friend" id="classgoogle_1_1cloud_1_1Status_1a3624db9be409bca17dc8940db074ddff" prot="public" static="no" const="no" explicit="no" inline="yes" virt="non-virtual">
              <type>bool</type>
              <definition>bool operator!=</definition>
              <argsstring>(Status const &amp;a, Status const &amp;b)</argsstring>
              <name>operator!=</name>
              <qualifiedname>google::cloud::Status::operator!=</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
                <declname>a</declname>
              </param>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
                <declname>b</declname>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="351" column="22" bodyfile="status.h" bodystart="351" bodyend="353"/>
            </memberdef>
            </sectiondef>
            <sectiondef kind="private-static-func">
            <memberdef kind="function" id="classgoogle_1_1cloud_1_1Status_1afe065bc32bcd984148924422980477b0" prot="private" static="yes" const="no" explicit="no" inline="no" virt="non-virtual">
              <type>bool</type>
              <definition>static bool google::cloud::Status::Equals</definition>
              <argsstring>(Status const &amp;a, Status const &amp;b)</argsstring>
              <name>Equals</name>
              <qualifiedname>google::cloud::Status::Equals</qualifiedname>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
                <declname>a</declname>
              </param>
              <param>
                <type><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> const &amp;</type>
                <declname>b</declname>
              </param>
              <briefdescription>
              </briefdescription>
              <detaileddescription>
              </detaileddescription>
              <inbodydescription>
              </inbodydescription>
              <location file="status.h" line="356" column="15"/>
            </memberdef>
            </sectiondef>
          <briefdescription>
      <para>Represents success or an error with info about the error. </para>
          </briefdescription>
          <detaileddescription>
      <para>This class is typically used to indicate whether or not a function or other operation completed successfully. Success is indicated by an "OK" status. OK statuses will have <computeroutput>.<ref refid="classgoogle_1_1cloud_1_1Status_1ac18337d2ceb00ca007725d21f2c63f9f" kindref="member">code()</ref> == <ref refid="namespacegoogle_1_1cloud_1a90e17f75452470f0f3ee1a06ffe58847ae69fa9a656f76dd8a4d89f21992b2d3a" kindref="member">StatusCode::kOk</ref></computeroutput> and <computeroutput>.<ref refid="classgoogle_1_1cloud_1_1Status_1a18952043ffe5a4f74911c8146e8bb504" kindref="member">ok()</ref> == true</computeroutput>, with all other properties having empty values. All OK statuses are equal. Any non-OK <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput> is considered an error. Users can inspect the error using the member functions, or they can simply stream the <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput> object, and it will print itself in some human readable way (the streamed format may change over time and you should <emphasis>not</emphasis> depend on the specific format of a streamed <computeroutput><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref></computeroutput> object remaining unchanged).</para>
      <para>This is a regular value type that can be copied, moved, compared for equality, and streamed. </para>
          </detaileddescription>
          <location file="status.h" line="295" column="1" bodyfile="status.h" bodystart="295" bodyend="364"/>
          <listofallmembers>
            <member refid="classgoogle_1_1cloud_1_1Status_1ac18337d2ceb00ca007725d21f2c63f9f" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>code</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1afe065bc32bcd984148924422980477b0" prot="private" virt="non-virtual"><scope>google::cloud::Status</scope><name>Equals</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a172e846ab5623d78d49e2ed128f49583" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>error_info</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a520fa011bc1cb95cda684a7b8ba701f3" prot="private" virt="non-virtual"><scope>google::cloud::Status</scope><name>impl_</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1aaa8dea39008758d8494f29b12b92be02" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>message</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a18952043ffe5a4f74911c8146e8bb504" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>ok</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a3624db9be409bca17dc8940db074ddff" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>operator!=</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a675be7e53ab9b27d69ba776a3c1ca7bf" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>operator=</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a23aaae701351564b3a17f47d5cb4e7cb" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>operator=</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a8c00daab4bca2eeb428f816fabf59492" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>operator==</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1af3de0fb0dee8fb557e693195a812987f" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>Status</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1aa3656155ad44d8bd75d92cd797123f4d" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>Status</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1afd186465a07fa176c10d437c1240f2de" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>Status</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1af927a89141bbcf10f0e02d789ebade94" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>Status</name></member>
            <member refid="classgoogle_1_1cloud_1_1Status_1a739165d43975222a55f064dd87db5e1f" prot="public" virt="non-virtual"><scope>google::cloud::Status</scope><name>~Status</name></member>
          </listofallmembers>
        </compounddef>
      </doxygen>)""";
}

std::string StatusDefaultConstructorId() {
  return "classgoogle_1_1cloud_1_1Status_1af3de0fb0dee8fb557e693195a812987f";
}

std::string StatusCopyConstructorId() {
  return "classgoogle_1_1cloud_1_1Status_1aa3656155ad44d8bd75d92cd797123f4d";
}

std::string StatusMessageFunctionId() {
  return "classgoogle_1_1cloud_1_1Status_1aaa8dea39008758d8494f29b12b92be02";
}

std::string StatusOperatorEqualId() {
  return "classgoogle_1_1cloud_1_1Status_1a8c00daab4bca2eeb428f816fabf59492";
}

}  // namespace docfx_testing
