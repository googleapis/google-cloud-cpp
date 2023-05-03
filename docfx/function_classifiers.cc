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
#include <algorithm>
#include <string_view>

namespace docfx {

bool IsOperator(pugi::xml_node node) {
  auto const name = std::string_view{node.child("name").child_value()};
  return name.find("operator") != std::string_view::npos;
}

bool IsConstructor(pugi::xml_node node) {
  auto is_empty = [](pugi::xml_node child) {
    auto const name = std::string_view{child.name()};
    if (name == "ref") return std::string_view{child.child_value()}.empty();
    return std::string_view{child.value()}.empty();
  };
  auto type = node.child("type");
  return std::all_of(type.begin(), type.end(), is_empty);
}

bool IsFunction(pugi::xml_node node) {
  auto const kind = std::string_view{node.attribute("kind").as_string()};
  if (kind == "function") return true;
  if (kind != "friend") return false;
  // Not all friends are functions, the `<type>` element can be used to
  // determine if a friend is a struct or class.
  auto const type = std::string_view{node.child_value("type")};
  return type != "struct" && type != "class";
}

bool IsPlainFunction(pugi::xml_node node) {
  return IsFunction(node) && !IsConstructor(node) && !IsOperator(node);
}

}  // namespace docfx
