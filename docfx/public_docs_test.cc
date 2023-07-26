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

#include "docfx/public_docs.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

TEST(PublicDocs, Basic) {
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
      <compounddef id="deprecated" kind="page"></compounddef>
      <compounddef id="not-deprecated" kind="page"></compounddef>
      <compounddef id="namespacegoogle" kind="namespace"></compounddef>
      <compounddef id="namespacegoogle_1_1cloud" kind="namespace"></compounddef>
      <memberdef kind="function" id="classgoogle_1_1cloud_1_1AsyncOperation_1a94e0b5e72b871d6f9cabf588dbb00343" prot="public" static="no" const="no" explicit="no" inline="no" virt="virtual">
        <type/>
        <name>~AsyncOperation</name>
        <qualifiedname>google::cloud::AsyncOperation::~AsyncOperation</qualifiedname>
      </memberdef>
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
      {"deprecated", false},
      {"not-deprecated", true},
      {"namespacegoogle", false},
      {"namespacegoogle_1_1cloud", false},
      {"classgoogle_1_1cloud_1_1AsyncOperation_"
       "1a94e0b5e72b871d6f9cabf588dbb00343",
       false},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    auto const cfg = Config{"unused", "kms", "unused"};
    EXPECT_EQ(test.expected, IncludeInPublicDocuments(cfg, selected.node()));
  }

  TestCase cloud_cases[] = {
      {"namespacegoogle", false},
      {"namespacegoogle_1_1cloud", true},
  };
  for (auto const& test : cloud_cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    auto const cfg = Config{"unused", "cloud", "unused"};
    EXPECT_EQ(test.expected, IncludeInPublicDocuments(cfg, selected.node()));
  }
}

}  // namespace
}  // namespace docfx
