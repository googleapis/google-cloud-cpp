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

#include "docfx/doxygen2toc.h"
#include "docfx/testing/inputs.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

TEST(Doxygen2Toc, Simple) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="common-error-handling" kind="page">
          <compoundname>common-error-handling</compoundname>
          <title>Error Handling</title>
          <briefdescription><para>An overview of error handling in the Google Cloud C++ client libraries.</para>
          </briefdescription>
          <detaileddescription><para>More details about error handling.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="indexpage" kind="page">
          <compoundname>index</compoundname>
          <title>The Page Title</title>
          <briefdescription><para>Some brief description.</para>
          </briefdescription>
          <detaileddescription><para>More details about the index.</para></detaileddescription>
        </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle" kind="namespace" language="C++">
        <compoundname>google</compoundname>
        <innernamespace refid="namespacegoogle_1_1cloud">google::cloud</innernamespace>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud" kind="namespace" language="C++">
        <compoundname>google::cloud</compoundname>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacestd" kind="namespace" language="Unknown">
        <compoundname>std</compoundname>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__terminate" kind="group">
        <compoundname>terminate</compoundname>
        <title>Terminate Group Title</title>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"""(### YamlMime:TableOfContent
name: "cloud.google.com/cpp/cloud"
items:
  - name: "The Page Title"
    href: index.md
    uid: indexpage
  - name: "In-Depth Topics"
    items:
      - name: "Error Handling"
        href: common-error-handling.md
        uid: common-error-handling
  - name: "Modules"
    items:
      - name: "Terminate Group Title"
        href: group__terminate.yml
        uid: group__terminate
  - name: "Namespaces"
    items:
      - name: "google::cloud"
        items:
          - name: "Overview"
            uid: namespacegoogle_1_1cloud
)""";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(kXml));
  auto const config = Config{"test-only-no-input-file", "cloud", "4.2"};
  auto const actual = Doxygen2Toc(config, doc);

  EXPECT_EQ(kExpected, actual);
}

TEST(Doxygen2Toc, PagesToc) {
  auto constexpr kXml =
      R"xml(<?xml version="1.0" standalone="yes"?><doxygen version="1.9.1" xml:lang="en-US">
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="common-error-handling" kind="page">
          <compoundname>common-error-handling</compoundname>
          <title>Error Handling</title>
          <briefdescription><para>An overview of error handling in the Google Cloud C++ client libraries.</para>
          </briefdescription>
          <detaileddescription><para>More details about error handling.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="indexpage" kind="page">
          <compoundname>index</compoundname>
          <title>The Page Title</title>
          <briefdescription><para>Some brief description.</para>
          </briefdescription>
          <detaileddescription><para>More details about the index.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="deprecated" kind="page">
          <compoundname>deprecated</compoundname>
          <title>Deprecated List</title>
          <briefdescription><para>Some brief description.</para>
          </briefdescription>
          <detaileddescription><para>More details about the index.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="secretmanager_v1_1_1SecretManagerServiceClient-endpoint-snippet" kind="page">
          <compoundname>secretmanager_v1::SecretManagerServiceClient-endpoint-snippet</compoundname>
          <title>Override secretmanager_v1::SecretManagerServiceClient Endpoint Configuration</title>
          <briefdescription><para>Some brief description.</para>
          </briefdescription>
          <detaileddescription><para>More details about the snippet.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="secretmanager_v1_1_1SecretManagerServiceClient-service-account-snippet" kind="page">
          <compoundname>secretmanager_v1::SecretManagerServiceClient-service-account-snippet</compoundname>
          <title>Override secretmanager_v1::SecretManagerServiceClient Authentication Default</title>
          <briefdescription><para>Some brief description.</para>
          </briefdescription>
          <detaileddescription><para>More details about the snippet.</para></detaileddescription>
        </compounddef>
      </doxygen>)xml";

  auto constexpr kExpected = R"""(### YamlMime:TableOfContent
name: "cloud.google.com/cpp/cloud"
items:
  - name: "The Page Title"
    href: index.md
    uid: indexpage
  - name: "In-Depth Topics"
    items:
      - name: "Error Handling"
        href: common-error-handling.md
        uid: common-error-handling
)""";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(kXml));
  auto const actual = Doxygen2Toc(Config{"unused", "cloud", ""}, doc);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Toc, GroupsToc) {
  auto constexpr kXml =
      R"xml(<?xml version="1.0" standalone="yes"?><doxygen version="1.9.1" xml:lang="en-US">
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__g1" kind="group">
          <compoundname>g1</compoundname>
          <title>Group 1</title>
          <briefdescription><para>The description for Group 1.</para>
          </briefdescription>
          <detaileddescription><para>More details about Group 1.</para></detaileddescription>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="group__g2" kind="group">
          <compoundname>g2</compoundname>
          <title>Group 2</title>
          <briefdescription><para>The description for Group 2.</para>
          </briefdescription>
          <detaileddescription><para>More details about Group 2.</para></detaileddescription>
        </compounddef>
      </doxygen>)xml";

  auto constexpr kExpected = R"""(### YamlMime:TableOfContent
name: "cloud.google.com/cpp/cloud"
items:
  - name: "Modules"
    items:
      - name: "Group 1"
        href: group__g1.yml
        uid: group__g1
      - name: "Group 2"
        href: group__g2.yml
        uid: group__g2
)""";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(kXml));
  auto const config = Config{"test-only-no-input-file", "cloud", "4.2"};
  auto const actual = Doxygen2Toc(config, doc);

  EXPECT_EQ(kExpected, actual);
}

TEST(Doxygen2Toc, CompoundToc) {
  auto constexpr kDocXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle" kind="namespace" language="C++">
        <compoundname>google</compoundname>
        <innernamespace refid="namespacegoogle_1_1cloud">google::cloud</innernamespace>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud" kind="namespace" language="C++">
        <compoundname>google::cloud</compoundname>
        <innerclass refid="classgoogle_1_1cloud_1_1future">google::cloud::future</innerclass>
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
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacestd" kind="namespace" language="Unknown">
        <compoundname>std</compoundname>
      </compounddef>
      <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1future" kind="class" language="C++" prot="public" final="yes">
        <compoundname>google::cloud::future</compoundname>
        <basecompoundref prot="private" virt="non-virtual">internal::future_base&lt; T &gt;</basecompoundref>
        <includes refid="future__generic_8h" local="no">google/cloud/future_generic.h</includes>
        <templateparamlist>
          <param>
            <type>typename T</type>
          </param>
        </templateparamlist>
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"""(### YamlMime:TableOfContent
name: "cloud.google.com/cpp/cloud"
items:
  - name: "Namespaces"
    items:
      - name: "google::cloud"
        items:
          - name: "Overview"
            uid: namespacegoogle_1_1cloud
          - name: "Classes"
            items:
              - name: "future<T>"
                items:
                  - name: "Overview"
                    uid: classgoogle_1_1cloud_1_1future
          - name: "Enums"
            items:
              - name: "Idempotency"
                items:
                  - name: "Overview"
                    uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c
                  - name: "kIdempotent"
                    uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9caf8bb1d9c7cccc450ecd06167c7422bfa
                  - name: "kNonIdempotent"
                    uid: namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190
)""";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(kDocXml));
  auto const actual = Doxygen2Toc(Config{"unused", "cloud", ""}, doc);
  EXPECT_EQ(actual, kExpected);
}

TEST(Doxygen2Toc, ClassToc) {
  auto constexpr kExpected = R"""(### YamlMime:TableOfContent
name: "cloud.google.com/cpp/cloud"
items:
  - name: "Namespaces"
    items:
      - name: "google::cloud"
        items:
          - name: "Overview"
            uid: namespacegoogle_1_1cloud
          - name: "Classes"
            items:
              - name: "Status"
                items:
                  - name: "Overview"
                    uid: classgoogle_1_1cloud_1_1Status
                  - name: "Constructors"
                    items:
                      - name: "Status(Status const &)"
                        uid: classgoogle_1_1cloud_1_1Status_1aa3656155ad44d8bd75d92cd797123f4d
                      - name: "Status(Status &&)"
                        uid: classgoogle_1_1cloud_1_1Status_1afd186465a07fa176c10d437c1240f2de
                      - name: "Status()"
                        uid: classgoogle_1_1cloud_1_1Status_1af3de0fb0dee8fb557e693195a812987f
                      - name: "Status(StatusCode, std::string, ErrorInfo)"
                        uid: classgoogle_1_1cloud_1_1Status_1af927a89141bbcf10f0e02d789ebade94
                  - name: "Operators"
                    items:
                      - name: "operator=(Status const &)"
                        uid: classgoogle_1_1cloud_1_1Status_1a675be7e53ab9b27d69ba776a3c1ca7bf
                      - name: "operator=(Status &&)"
                        uid: classgoogle_1_1cloud_1_1Status_1a23aaae701351564b3a17f47d5cb4e7cb
                      - name: "operator==(Status const &, Status const &)"
                        uid: classgoogle_1_1cloud_1_1Status_1a8c00daab4bca2eeb428f816fabf59492
                      - name: "operator!=(Status const &, Status const &)"
                        uid: classgoogle_1_1cloud_1_1Status_1a3624db9be409bca17dc8940db074ddff
                  - name: "Functions"
                    items:
                      - name: "ok() const"
                        uid: classgoogle_1_1cloud_1_1Status_1a18952043ffe5a4f74911c8146e8bb504
                      - name: "code() const"
                        uid: classgoogle_1_1cloud_1_1Status_1ac18337d2ceb00ca007725d21f2c63f9f
                      - name: "message() const"
                        uid: classgoogle_1_1cloud_1_1Status_1aaa8dea39008758d8494f29b12b92be02
                      - name: "error_info() const"
                        uid: classgoogle_1_1cloud_1_1Status_1a172e846ab5623d78d49e2ed128f49583
)""";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(docfx_testing::StatusClassXml().c_str()));
  auto const actual = Doxygen2Toc(Config{"unused", "cloud", ""}, doc);
  EXPECT_EQ(actual, kExpected);
}

}  // namespace
}  // namespace docfx
