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

#include "docfx/node_name.h"
#include "docfx/testing/inputs.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

TEST(NodeName, Namespace) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud" kind="namespace" language="C++">
           <compoundname>google::cloud</compoundname>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="namespacegoogle_1_1cloud_1_1mocks" kind="namespace" language="C++">
            <compoundname>google::cloud::mocks</compoundname>
        </compounddef>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"namespacegoogle_1_1cloud", "google::cloud"},
      {"namespacegoogle_1_1cloud_1_1mocks", "google::cloud::mocks"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, NodeName(selected.node()));
  }
}

TEST(NodeName, Class) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1Status" kind="class" language="C++" prot="public">
            <compoundname>google::cloud::Status</compoundname>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1StatusOr" kind="class" language="C++" prot="public" final="yes">
            <compoundname>google::cloud::StatusOr</compoundname>
            <templateparamlist>
            <param><type>typename T</type></param>
            </templateparamlist>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="structgoogle_1_1cloud_1_1LoggingComponentsOption" kind="struct" language="C++" prot="public">
            <compoundname>google::cloud::LoggingComponentsOption</compoundname>
        </compounddef>
        <compounddef xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="classgoogle_1_1cloud_1_1future_3_01void_01_4" kind="class" language="C++" prot="public" final="yes">
            <compoundname>google::cloud::future&lt; void &gt;</compoundname>
        </compounddef>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"classgoogle_1_1cloud_1_1Status", "Status"},
      {"classgoogle_1_1cloud_1_1StatusOr", "StatusOr<T>"},
      {"structgoogle_1_1cloud_1_1LoggingComponentsOption",
       "LoggingComponentsOption"},
      {"classgoogle_1_1cloud_1_1future_3_01void_01_4", "future< void >"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, NodeName(selected.node()));
  }
}

TEST(NodeName, Enum) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <memberdef kind="enum" id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c" prot="public" static="no" strong="yes">
            <type/>
            <name>Idempotency</name>
            <qualifiedname>google::cloud::Idempotency</qualifiedname>
        </memberdef>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9c",
       "Idempotency"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, NodeName(selected.node()));
  }
}

TEST(NodeName, EnumValue) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
        <enumvalue id="namespacegoogle_1_1cloud_1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190" prot="public">
          <name>kNonIdempotent</name>
        </enumvalue>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"namespacegoogle_1_1cloud_"
       "1a7d65fd569564712b7cfe652613f30d9cae75d33e94f2dc4028d4d67bdaab75190",
       "kNonIdempotent"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, NodeName(selected.node()));
  }
}

TEST(NodeName, Typedef) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
      <memberdef kind="typedef" id="namespacegoogle_1_1cloud_1a7a08fee311943ff399218e534ee86287" prot="public" static="no">
        <type>::google::cloud::internal::BackoffPolicy</type>
        <definition>using google::cloud::BackoffPolicy = typedef ::google::cloud::internal::BackoffPolicy</definition>
        <argsstring/>
        <name>BackoffPolicy</name>
        <qualifiedname>google::cloud::BackoffPolicy</qualifiedname>
      </memberdef>
      <memberdef kind="typedef" id="structgoogle_1_1cloud_1_1UserAgentProductsOption_1acbbd25eda33665932bf5561aae9682e3" prot="public" static="no">
        <type>std::vector&lt; std::string &gt;</type>
        <definition>using google::cloud::UserAgentProductsOption::Type =  std::vector&lt;std::string&gt;</definition>
        <argsstring/>
        <name>Type</name>
        <qualifiedname>google::cloud::UserAgentProductsOption::Type</qualifiedname>
      </memberdef>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"namespacegoogle_1_1cloud_1a7a08fee311943ff399218e534ee86287",
       "BackoffPolicy"},
      {"structgoogle_1_1cloud_1_1UserAgentProductsOption_"
       "1acbbd25eda33665932bf5561aae9682e3",
       "Type"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, NodeName(selected.node()));
  }
}

TEST(NodeName, Functions) {
  pugi::xml_document doc;
  doc.load_string(docfx_testing::StatusClassXml().c_str());

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {docfx_testing::StatusDefaultConstructorId(), "Status()"},
      {docfx_testing::StatusCopyConstructorId(), "Status(Status const &)"},
      {docfx_testing::StatusMessageFunctionId(), "message() const"},
      {docfx_testing::StatusOperatorEqualId(),
       "operator==(Status const &, Status const &)"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, NodeName(selected.node()));
  }
}

}  // namespace
}  // namespace docfx
