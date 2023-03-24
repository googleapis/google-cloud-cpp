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

#include "docfx/doxygen2yaml.h"
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen_errors.h"
#include "docfx/yaml_context.h"
#include "docfx/yaml_emit.h"
#include <sstream>
#include <string_view>

namespace docfx {
namespace {

auto kind(pugi::xml_node const& node) {
  return std::string_view{node.attribute("kind").as_string()};
}

std::string Summary(pugi::xml_node const& node) {
  std::ostringstream os;
  MarkdownContext ctx;
  ctx.paragraph_start = "";
  auto brief = node.child("briefdescription");
  auto detailed = node.child("detaileddescription");
  if (!brief.empty()) {
    AppendIfBriefDescription(os, ctx, brief);
    ctx = MarkdownContext{};
  }
  if (!detailed.empty()) AppendIfDetailedDescription(os, ctx, detailed);
  return std::move(os).str();
}

}  // namespace

void StartDocFxYaml(YAML::Emitter& yaml) {
  yaml << YAML::BeginMap << YAML::Key << "items" << YAML::Value
       << YAML::BeginSeq;
}

std::string EndDocFxYaml(YAML::Emitter& yaml) {
  yaml << YAML::EndSeq << YAML::EndMap;
  auto result = std::string{"### YamlMime:UniversalReference\n"};
  result += yaml.c_str();
  result += "\n";
  return result;
}

bool AppendIfEnumValue(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node const& node) {
  if (std::string_view{node.name()} != "enumvalue") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const summary = Summary(node);

  yaml << YAML::BeginMap                                               //
       << YAML::Key << "uid" << YAML::Value << id                      //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name  //
       << YAML::Key << "id" << YAML::Value << id                       //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id        //
       << YAML::Key << "type" << YAML::Value << "enumvalue"            //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  return true;
}

bool AppendIfEnum(YAML::Emitter& yaml, YamlContext const& ctx,
                  pugi::xml_node const& node) {
  if (kind(node) != "enum") return false;
  if (node.attribute("id").empty()) MissingAttribute(__func__, "id", node);
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name       //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << full_name                         //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "enum"                      //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  auto nested = ctx;
  nested.parent_id = id;
  for (auto const& child : node) {
    if (AppendIfEnumValue(yaml, nested, child)) continue;
  }
  return true;
}

bool AppendIfTypedef(YAML::Emitter& yaml, YamlContext const& ctx,
                     pugi::xml_node const& node) {
  if (kind(node) != "typedef") return false;
  auto const id = std::string_view{node.attribute("id").as_string()};
  auto const name = std::string_view{node.child("name").child_value()};
  auto const full_name =
      std::string_view{node.child("qualifiedname").child_value()};
  yaml << YAML::BeginMap                                                    //
       << YAML::Key << "uid" << YAML::Value << id                           //
       << YAML::Key << "name" << YAML::Value << YAML::Literal << name       //
       << YAML::Key << "fullName"                                           //
       << YAML::Value << YAML::Literal << full_name                         //
       << YAML::Key << "id" << YAML::Value << id                            //
       << YAML::Key << "parent" << YAML::Value << ctx.parent_id             //
       << YAML::Key << "type" << YAML::Value << "typedef"                   //
       << YAML::Key << "langs" << YAML::BeginSeq << "cpp" << YAML::EndSeq;  //
  auto const summary = Summary(node);
  if (!summary.empty()) {
    yaml << YAML::Key << "summary" << YAML::Value << YAML::Literal << summary;
  }
  yaml << YAML::EndMap;
  return true;
}

}  // namespace docfx
