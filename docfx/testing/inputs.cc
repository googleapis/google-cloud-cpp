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

}  // namespace docfx_testing
