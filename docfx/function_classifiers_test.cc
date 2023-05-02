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

#include "docfx/function_classifiers.h"
#include "docfx/testing/inputs.h"
#include <gmock/gmock.h>

namespace docfx {
namespace {

TEST(FunctionClassifiers, IsConstructor) {
  pugi::xml_document doc;
  doc.load_string(docfx_testing::StatusClassXml().c_str());

  pugi::xpath_variable_set vars;
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//memberdef[@id = string($id)]", &vars);

  vars.set("id", docfx_testing::StatusDefaultConstructorId().c_str());
  auto ctor = doc.select_node(query);
  ASSERT_TRUE(ctor);
  EXPECT_TRUE(IsConstructor(ctor.node()));

  vars.set("id", docfx_testing::StatusMessageFunctionId().c_str());
  auto msg = doc.select_node(query);
  ASSERT_TRUE(msg);
  EXPECT_FALSE(IsConstructor(msg.node()));
}

TEST(FunctionClassifiers, IsOperator) {
  pugi::xml_document doc;
  doc.load_string(docfx_testing::StatusClassXml().c_str());

  pugi::xpath_variable_set vars;
  vars.add("id", pugi::xpath_type_string);
  auto query = pugi::xpath_query("//memberdef[@id = string($id)]", &vars);

  vars.set("id", docfx_testing::StatusOperatorEqualId().c_str());
  auto eq = doc.select_node(query);
  ASSERT_TRUE(eq);
  EXPECT_TRUE(IsOperator(eq.node()));

  vars.set("id", docfx_testing::StatusDefaultConstructorId().c_str());
  auto ctor = doc.select_node(query);
  ASSERT_TRUE(ctor);
  EXPECT_FALSE(IsOperator(ctor.node()));

  vars.set("id", docfx_testing::StatusMessageFunctionId().c_str());
  auto msg = doc.select_node(query);
  ASSERT_TRUE(msg);
  EXPECT_FALSE(IsConstructor(msg.node()));
}

}  // namespace
}  // namespace docfx
