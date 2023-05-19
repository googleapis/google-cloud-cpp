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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_YAML_CONTEXT_H
#define GOOGLE_CLOUD_CPP_DOCFX_YAML_CONTEXT_H

#include "docfx/config.h"
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace docfx {

struct YamlContext {
  std::string library_root = "google/cloud/";
  std::string parent_id;
  Config config;
  // Mocking functions (the `MOCK_METHOD()` elements), indexed by function name.
  std::unordered_map<std::string, std::string> mocking_functions;
  // Mocking functions (the `MOCK_METHOD()` elements), indexed by their id.
  std::unordered_map<std::string, std::string> mocking_functions_by_id;
  // Mocked functions (indexed by their id) pointing to the corresponding
  // `MOCK_METHOD()`'s id.
  std::unordered_map<std::string, std::string> mocked_ids;
  // Fallback brief and detailed description.
  std::string fallback_description_brief;
  std::string fallback_description_detailed;
};

/// Creates a new context to recurse over @p node
YamlContext NestedYamlContext(YamlContext const& ctx, pugi::xml_node node);

/// Returns true if a <memberdef> element should be skipped from the
/// children and references lists. We always skip mocked functions.
bool IsSkippedChild(YamlContext const& ctx, pugi::xml_node node);

/// If @p node is mocked, returns the mocking node. Otherwise return @p node.
pugi::xml_node MockingNode(YamlContext const& ctx, pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_YAML_CONTEXT_H
