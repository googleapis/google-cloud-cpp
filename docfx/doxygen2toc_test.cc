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
          <title>The Page Title: unused in this test</title>
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
      </compounddef>
    </doxygen>)xml";

  auto constexpr kExpected = R"""(### YamlMime:TableOfContent
name: common
items:
  - name: common-error-handling
    href: common-error-handling.md
  - name: README
    href: indexpage.md
  - uid: group__terminate
    name: group__terminate
)""";

  pugi::xml_document doc;
  ASSERT_TRUE(doc.load_string(kXml));
  auto const config = Config{"test-only-no-input-file", "common", "4.2"};
  auto const actual = Doxygen2Toc(config, doc);

  EXPECT_EQ(kExpected, actual);
}

}  // namespace
}  // namespace docfx
