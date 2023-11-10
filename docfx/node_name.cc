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

#include "docfx/node_name.h"
#include "docfx/doxygen_errors.h"
#include "docfx/function_classifiers.h"
#include "docfx/linked_text_type.h"
#include <sstream>
#include <string_view>

namespace docfx {
namespace {

std::string UnqualifiedClassName(pugi::xml_node node) {
  auto name = std::string_view{node.child_value("compoundname")};
  auto const p = name.rfind("::");
  if (p == std::string_view::npos) return std::string{name};
  return std::string{name.substr(p + 2)};
}

std::string_view RemoveTypenamePrefix(std::string_view tname) {
  auto const prefix = std::string_view{"typename "};
  if (tname.rfind(prefix) != 0) return tname;
  return tname.substr(prefix.size());
}

// Template classes have a `<templateparamlist>` child. This child has a list
// of `<param>` children, each one containing a `<type>` child with the template
// parameter name. But they appear as `typename T` instead of `T`.
std::string ClassName(pugi::xml_node node) {
  auto name = UnqualifiedClassName(node);
  auto tparams = node.child("templateparamlist").children("param");
  if (tparams.empty()) return name;
  auto sep = std::string_view{"<"};
  for (auto const& t : tparams) {
    name += sep;
    name += RemoveTypenamePrefix(std::string_view{t.child_value("type")});
    sep = ", ";
  }
  name += ">";
  return name;
}

// Functions can be overloaded. Their names need to include the parameter types
// or the person reading the documentation cannot navigate effectively. Member
// functions also need any cv-qualifiers.
std::string FunctionName(pugi::xml_node node) {
  std::ostringstream os;
  if (std::string_view{node.attribute("static").as_string()} == "yes") {
    os << "static ";
  }
  if (std::string_view{node.attribute("virt").as_string()} == "virtual") {
    os << "virtual ";
  }

  os << node.child_value("name") << "(";
  auto params = node.select_nodes("param");
  auto sep = std::string_view{};
  for (auto const& i : params) {
    os << sep << LinkedTextType(i.node().child("type"));
    sep = ", ";
  }
  os << ")";
  if (std::string_view{node.attribute("const").as_string()} == "yes") {
    os << " const";
  }
  auto const refqual = std::string_view{node.attribute("refqual").as_string()};
  if (refqual == "rvalue") os << " &&";
  if (refqual == "lvalue") os << " &";
  return std::move(os).str();
}

}  // namespace

std::string NodeName(pugi::xml_node node) {
  auto const element = std::string_view{node.name()};
  if (element == "compounddef") {
    auto kind = std::string_view{node.attribute("kind").as_string()};
    if (kind == "class" || kind == "struct") {
      return ClassName(node);
    }
    return node.child_value("compoundname");
  }
  if (element == "memberdef" || element == "member") {
    if (IsFunction(node)) return FunctionName(node);
    return node.child_value("name");
  }
  if (element == "enumvalue") {
    return node.child_value("name");
  }
  std::ostringstream os;
  os << "Unknown doxygen element " << __func__ << "(): node=";
  node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
  throw std::runtime_error(std::move(os).str());
}

}  // namespace docfx
