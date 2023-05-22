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

#include "docfx/doxygen_pages.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

TEST(DoxygenPages, CommonPage) {
  auto constexpr kXml =
      R"xml(<?xml version="1.0" standalone="yes"?><doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="indexpage" kind="page">
        <compoundname>index</compoundname>
        <title>Common Components for the Google Cloud C++ Client Libraries</title>
        <briefdescription>
        </briefdescription>
        <detaileddescription>
          <sect1 id="index_1autotoc_md0">
            <title>Overview</title>
            <para>This library contains common components shared by all the Google Cloud C++ Client Libraries. Including:</para>
            <para><itemizedlist>
              <listitem><para><ref refid="classgoogle_1_1cloud_1_1Credentials" kindref="compound">Credentials</ref> are used to configure authentication in the client libraries. See <ref refid="group__guac" kindref="compound">Authentication Components</ref> for more details on authentication.</para>
              </listitem><listitem><para><ref refid="classgoogle_1_1cloud_1_1Options" kindref="compound">Options</ref> are used to override the client library default configuration. See <ref refid="group__options" kindref="compound">Client Library Configuration</ref> for more details on library configuration.</para>
              </listitem><listitem><para><ref refid="classgoogle_1_1cloud_1_1Status" kindref="compound">Status</ref> error codes and details from an operation.</para>
              </listitem><listitem><para><ref refid="classgoogle_1_1cloud_1_1StatusOr" kindref="compound">StatusOr&lt;T&gt;</ref> returns a value on success and a <computeroutput>Status</computeroutput> on error.</para>
              </listitem><listitem><para><ref refid="classgoogle_1_1cloud_1_1future" kindref="compound">future&lt;T&gt;</ref> and <ref refid="classgoogle_1_1cloud_1_1promise" kindref="compound">promise&lt;T&gt;</ref> futures (a holder that will receive a value asynchronously) and promises (the counterpart of a future, where values are stored asynchronously). They satisfy the API for <computeroutput>std::future</computeroutput> and <computeroutput>std::promise</computeroutput>, and add support for callbacks and cancellation.</para>
              </listitem></itemizedlist>
            </para>
            <para>
              <simplesect kind="note"><para>Say something noteworthy.</para></simplesect>
              <simplesect kind="remark"><para>Say something remarkable.</para></simplesect>
              <simplesect kind="warning"><para>The symbols in the <computeroutput>google::cloud::testing_util</computeroutput> namespace are implementation details and subject to change and/or removal without notice.</para>
              </simplesect>
              <simplesect kind="attention"><para>The symbols in the <computeroutput>google::cloud::internal</computeroutput> namespace are implementation details and subject to change and/or removal without notice.</para>
              </simplesect>
            </para>
            <sect2 id="index_1autotoc_md1">
              <title>More information</title>
              <para><itemizedlist>
              <listitem><para><ref refid="common-error-handling" kindref="compound">Error Handling</ref> for more details about how the libraries report run-time errors and how you can handle them.</para>
              </listitem></itemizedlist>
              </para>
            </sect2>
          </sect1>
        </detaileddescription>
        <location file="doc/common-main.dox"/>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"md(---
uid: indexpage
---

# Common Components for the Google Cloud C++ Client Libraries


## Overview

This library contains common components shared by all the Google Cloud C++ Client Libraries. Including:


- [Credentials](xref:classgoogle_1_1cloud_1_1Credentials) are used to configure authentication in the client libraries. See [Authentication Components](xref:group__guac) for more details on authentication.
- [Options](xref:classgoogle_1_1cloud_1_1Options) are used to override the client library default configuration. See [Client Library Configuration](xref:group__options) for more details on library configuration.
- [Status](xref:classgoogle_1_1cloud_1_1Status) error codes and details from an operation.
- [StatusOr<T>](xref:classgoogle_1_1cloud_1_1StatusOr) returns a value on success and a `Status` on error.
- [future<T>](xref:classgoogle_1_1cloud_1_1future) and [promise<T>](xref:classgoogle_1_1cloud_1_1promise) futures (a holder that will receive a value asynchronously) and promises (the counterpart of a future, where values are stored asynchronously). They satisfy the API for `std::future` and `std::promise`, and add support for callbacks and cancellation.



<aside class="note"><b>Note:</b>
Say something noteworthy.
</aside>

<aside class="note"><b>Remark:</b>
Say something remarkable.
</aside>

<aside class="warning"><b>Warning:</b>
The symbols in the `google::cloud::testing_util` namespace are implementation details and subject to change and/or removal without notice.
</aside>

<aside class="caution"><b>Attention:</b>
The symbols in the `google::cloud::internal` namespace are implementation details and subject to change and/or removal without notice.
</aside>

### More information


- [Error Handling](xref:common-error-handling) for more details about how the libraries report run-time errors and how you can handle them.
)md";

  pugi::xml_document doc;
  doc.load_string(kXml);
  auto selected = doc.select_node("//*[@id='indexpage']");
  ASSERT_TRUE(selected);
  auto const actual = Page2Markdown(selected.node());
  EXPECT_EQ(kExpected, actual);
}

}  // namespace
}  // namespace docfx
