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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2YAML_H
#define GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2YAML_H

#include "docfx/toc_entry.h"
#include "docfx/yaml_context.h"
#include <pugixml.hpp>
#include <yaml-cpp/yaml.h>
#include <string>

namespace docfx {

// Generate the YAML file contents for `<compounddef>` nodes representing C++
// types.
std::string Compound2Yaml(Config const& cfg, pugi::xml_node node);

// Close the preamble elements required by DocFx and return the file contents.
std::string EndDocFxYaml(YAML::Emitter& yaml);

// Create a YAML entry for an enum value.
bool AppendIfEnumValue(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node);

// Create a YAML entry for an enum definition.
bool AppendIfEnum(YAML::Emitter& yaml, YamlContext const& ctx,
                  pugi::xml_node node);

// Create a YAML entry for a typedef definition.
bool AppendIfTypedef(YAML::Emitter& yaml, YamlContext const& ctx,
                     pugi::xml_node node);

// Create a YAML entry for a friend definition.
bool AppendIfFriend(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node node);

// Create a YAML entry for a variable definition.
bool AppendIfVariable(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node node);

// Create a YAML entry for a function declaration.
bool AppendIfFunction(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node node);

// Create YAML entries for a <sectiondef> and its children.
bool AppendIfSectionDef(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node node);

// Create YAML entries for a namespace and its children.
bool AppendIfNamespace(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node);

// Create YAML entries for a class and its children.
bool AppendIfClass(YAML::Emitter& yaml, YamlContext const& ctx,
                   pugi::xml_node node);

// Create YAML entries for a struct and its children.
bool AppendIfStruct(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2YAML_H
