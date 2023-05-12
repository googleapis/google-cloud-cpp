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

#include "docfx/linked_text_type.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

TEST(LinkedTextType, Basic) {
  auto constexpr kXml = R"xml(<?xml version="1.0" standalone="yes"?>
    <doxygen version="1.9.1" xml:lang="en-US">
    <type id="001"><ref refid="classgoogle_1_1cloud_1_1ErrorInfo" kindref="compound">ErrorInfo</ref> const &amp;</type>
    <type id="002">std::string</type>
    </doxygen>)xml";
  pugi::xml_document doc;
  doc.load_string(kXml);

  struct TestCase {
    std::string id;
    std::string expected;
  } const cases[] = {
      {"001", "ErrorInfo const &"},
      {"002", "std::string"},
  };

  auto vars = pugi::xpath_variable_set();
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//*[@id = string($id)]", &vars);
  for (auto const& test : cases) {
    SCOPED_TRACE("Running with id=" + test.id);
    vars.set("id", test.id.c_str());
    auto selected = doc.select_node(query);
    ASSERT_TRUE(selected);
    EXPECT_EQ(test.expected, LinkedTextType(selected.node()));
  }
}

}  // namespace
}  // namespace docfx
