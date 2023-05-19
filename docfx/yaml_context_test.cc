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

#include "docfx/yaml_context.h"
#include "docfx/testing/inputs.h"
#include <gmock/gmock.h>
#include <pugixml.hpp>

namespace docfx {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(YamlContextTest, MockedFunctions) {
  pugi::xml_document doc;
  doc.load_string(docfx_testing::MockClass().c_str());
  auto const filter = "//*[@id='" + docfx_testing::MockClassId() + "']";
  auto selected = doc.select_node(filter.c_str());
  ASSERT_TRUE(selected);

  YamlContext parent;
  parent.config.library = "kms";
  auto const actual = NestedYamlContext(parent, selected.node());
  EXPECT_THAT(
      actual.mocking_functions,
      UnorderedElementsAre(Pair("options",
                                "classgoogle_1_1cloud_1_1kms__inventory__v1__"
                                "mocks_1_1MockKeyDashboardServiceConnection_"
                                "1a2bf84b7b96702bc1622f0e6c9f0babc4"),
                           Pair("ListCryptoKeys",
                                "classgoogle_1_1cloud_1_1kms__inventory__v1__"
                                "mocks_1_1MockKeyDashboardServiceConnection_"
                                "1a789db998d71abf9016b64832d0c7a99e")));
  EXPECT_THAT(
      actual.mocking_functions_by_id,
      UnorderedElementsAre(Pair("classgoogle_1_1cloud_1_1kms__inventory__v1__"
                                "mocks_1_1MockKeyDashboardServiceConnection_"
                                "1a2bf84b7b96702bc1622f0e6c9f0babc4",
                                "options"),
                           Pair("classgoogle_1_1cloud_1_1kms__inventory__v1__"
                                "mocks_1_1MockKeyDashboardServiceConnection_"
                                "1a789db998d71abf9016b64832d0c7a99e",
                                "ListCryptoKeys")));
  EXPECT_THAT(
      actual.mocked_ids,
      UnorderedElementsAre(Pair("classgoogle_1_1cloud_1_1kms__inventory__v1_1_"
                                "1KeyDashboardServiceConnection_"
                                "1a922ac7ae75f6939947a07d843e863845",
                                "classgoogle_1_1cloud_1_1kms__inventory__v1__"
                                "mocks_1_1MockKeyDashboardServiceConnection_"
                                "1a2bf84b7b96702bc1622f0e6c9f0babc4"),
                           Pair("classgoogle_1_1cloud_1_1kms__inventory__v1_1_"
                                "1KeyDashboardServiceConnection_"
                                "1a2518e5014c3adbc16e83281bd2a596a8",
                                "classgoogle_1_1cloud_1_1kms__inventory__v1__"
                                "mocks_1_1MockKeyDashboardServiceConnection_"
                                "1a789db998d71abf9016b64832d0c7a99e")));
}

}  // namespace
}  // namespace docfx
