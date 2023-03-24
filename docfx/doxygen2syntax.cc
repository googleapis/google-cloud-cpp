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

#include "docfx/doxygen2syntax.h"
#include "docfx/doxygen_errors.h"
#include "docfx/yaml_context.h"
#include "docfx/yaml_emit.h"

namespace docfx {
namespace {

void AppendLocation(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node const& node, char const* file_attribute,
                    char const* line_attribute) {
  auto const name = std::string_view{node.child("name").child_value()};
  auto const location = node.child("location");
  if (name.empty() || !location) return;
  auto const line =
      std::string_view{location.attribute(line_attribute).as_string()};
  auto const file = std::string{location.attribute(file_attribute).as_string()};
  if (line.empty() || file.empty()) return;

  auto const repo =
      std::string_view{"https://github.com/googleapis/google-cloud-cpp/"};
  auto const path = ctx.library_root + file;
  auto const branch = std::string_view{"main"};
  yaml << YAML::Key << "source" << YAML::Value             //
       << YAML::BeginMap                                   //
       << YAML::Key << "id" << YAML::Value << name         //
       << YAML::Key << "path" << YAML::Value << path       //
       << YAML::Key << "startLine" << YAML::Value << line  //
       << YAML::Key << "remote" << YAML::Value             //
       << YAML::BeginMap                                   //
       << YAML::Key << "repo" << YAML::Value << repo       //
       << YAML::Key << "branch" << YAML::Value << branch   //
       << YAML::Key << "path" << YAML::Value << path       //
       << YAML::EndMap                                     // remote
       << YAML::EndMap                                     // source
      ;
}

}  // namespace

void AppendEnumSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node const& node) {
  yaml << YAML::Key << "syntax" << YAML::Value  //
       << YAML::BeginMap;
  AppendLocation(yaml, ctx, node, "file", "line");
  yaml << YAML::EndMap;
}

void AppendTypedefSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                         pugi::xml_node const& node) {
  yaml << YAML::Key << "syntax" << YAML::Value  //
       << YAML::BeginMap;
  AppendLocation(yaml, ctx, node, "file", "line");
  yaml << YAML::EndMap;
}

}  // namespace docfx
