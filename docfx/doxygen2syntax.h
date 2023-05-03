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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2SYNTAX_H
#define GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2SYNTAX_H

#include "docfx/yaml_context.h"
#include <pugixml.hpp>
#include <yaml-cpp/yaml.h>

namespace docfx {

// Generate the `syntax` element in a DocFX YAML.
//
// The DocFX YAML files contain a `syntax` element which includes:
// - Some textual representation of the element, e.g., the declaration for
//   a function.
// - Any template parameters for classes, structs, functions, etc.
// - The names, types, and description of any function parameters.
// - The return value for the function.
// - The type aliased by a typedef or using definition.
// - The value of enums (if applicable).

// Generate the `syntax.content` element for an enum.
std::string EnumSyntaxContent(pugi::xml_node node);

// Generate the `syntax.content` element for a typedef.
std::string TypedefSyntaxContent(pugi::xml_node node);

// Generate the `syntax.content` element for a friend.
std::string FriendSyntaxContent(pugi::xml_node node);

// Generate the `syntax.content` element for a variable.
std::string VariableSyntaxContent(pugi::xml_node node);

// Generate the `syntax.content` element for a function.
std::string FunctionSyntaxContent(pugi::xml_node node,
                                  std::string_view prefix = {});

// Generate the `syntax.content` element for a class.
std::string ClassSyntaxContent(pugi::xml_node node,
                               std::string_view prefix = {});

// Generate the `syntax.content` element for a struct.
std::string StructSyntaxContent(pugi::xml_node node,
                                std::string_view prefix = {});

// Generate the `syntax.content` element for a namespace.
std::string NamespaceSyntaxContent(pugi::xml_node node);

// Generate the `syntax` element for an enum.
void AppendEnumSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node node);

// Generate the `syntax` element for an typedef.
void AppendTypedefSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                         pugi::xml_node node);

// Generate the `syntax` element for a friend.
void AppendFriendSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node node);

// Generate the `syntax` element for an variable.
void AppendVariableSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                          pugi::xml_node node);

// Generate the `syntax` element for a function.
void AppendFunctionSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                          pugi::xml_node node);

// Generate the `syntax` element for a class.
void AppendClassSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node);

// Generate the `syntax` element for a struct.
void AppendStructSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node node);

// Generate the `syntax` element for a namespace.
void AppendNamespaceSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                           pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN2SYNTAX_H
