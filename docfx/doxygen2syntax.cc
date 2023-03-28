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
#include "docfx/yaml_emit.h"

namespace docfx {
namespace {

void AppendLocation(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node const& node, char const* name_attribute) {
  auto const name = std::string_view{node.child(name_attribute).child_value()};
  auto const location = node.child("location");
  if (name.empty() || !location) return;
  auto const line = std::string_view{location.attribute("line").as_string()};
  auto const file = std::string{location.attribute("file").as_string()};
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
       << YAML::EndMap;                                    // source
}

// A `linkedTextType` is defined as below. It is basically a sequence of
// references (links) mixed with plain text. We ignore the references in the
// formatting of the syntax content.
//
// clang-format off
//   <xsd:complexType name="linkedTextType" mixed="true">
//     <xsd:sequence>
//     <xsd:element name="ref" type="refTextType" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//   </xsd:complexType>
// ... ..
//   <xsd:complexType name="refTextType">
//     <xsd:simpleContent>
//       <xsd:extension base="xsd:string">
//        <xsd:attribute name="refid" type="xsd:string" />
//        <xsd:attribute name="kindref" type="DoxRefKind" />
//        <xsd:attribute name="external" type="xsd:string" use="optional"/>
//        <xsd:attribute name="tooltip" type="xsd:string" use="optional"/>
//       </xsd:extension>
//     </xsd:simpleContent>
//   </xsd:complexType>
// clang-format on
std::string LinkedTextType(pugi::xml_node const& node) {
  std::ostringstream os;
  for (auto const& child : node) {
    auto const name = std::string_view{child.name()};
    if (name == "ref") {
      os << child.child_value();
    } else {
      os << child.value();
    }
  }
  return std::move(os).str();
}

}  // namespace

std::string EnumSyntaxContent(pugi::xml_node const& node) {
  std::ostringstream os;
  auto const strong = std::string_view{node.attribute("strong").as_string()};
  os << "enum " << (strong == "yes" ? "class " : "")
     << node.child_value("qualifiedname") << " {\n";
  for (auto const& child : node) {
    if (std::string_view{child.name()} != "enumvalue") continue;
    os << "  " << child.child_value("name") << ",\n";
  }
  os << "};";
  return std::move(os).str();
}

std::string TypedefSyntaxContent(pugi::xml_node const& node) {
  std::ostringstream os;
  os << "using " << node.child_value("qualifiedname") << " =\n"  //
     << "  " << LinkedTextType(node.child("type")) << ";";
  return std::move(os).str();
}

std::string FunctionSyntaxContent(pugi::xml_node const& node) {
  std::ostringstream os;
  auto templateparamlist = node.child("templateparamlist");
  if (templateparamlist) {
    os << "template <";
    auto sep = std::string_view{"\n    "};
    for (auto const& param : templateparamlist) {
      if (std::string_view{param.name()} != "param") {
        UnknownChildType(__func__, param);
      }
      os << sep << LinkedTextType(param.child("type"));
      auto const defval = param.child("defval");
      if (defval) {
        os << " = " << LinkedTextType(defval);
      }
      sep = std::string_view{",\n    "};
    }
    os << ">\n";
  }
  os << LinkedTextType(node.child("type")) << "\n"
     << node.child_value("qualifiedname") << " (";
  auto sep = std::string_view();
  auto params = node.select_nodes("param");
  if (!params.empty()) {
    for (auto const& i : params) {
      os << "\n    " << LinkedTextType(i.node().child("type")) << " "
         << i.node().child_value("declname");
    }
    sep = std::string_view("\n  ");
  }
  os << sep << ")";
  return std::move(os).str();
}

std::string ClassSyntaxContent(pugi::xml_node const& node) {
  std::ostringstream os;
  os << "// Found in #include <" << node.child_value("includes") << ">\n"
     << "class " << node.child_value("compoundname") << " { ... };";
  return std::move(os).str();
}

void AppendEnumSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node const& node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << EnumSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendTypedefSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                         pugi::xml_node const& node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << TypedefSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendFunctionSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                          pugi::xml_node const& node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << FunctionSyntaxContent(node);
  yaml << YAML::Key << "returns" << YAML::Value                //
       << YAML::BeginMap                                       //
       << YAML::Key << "var_type" << YAML::Value               //
       << YAML::Literal << LinkedTextType(node.child("type"))  //
       << YAML::EndMap;
  auto params = node.select_nodes("param");
  if (!params.empty()) {
    yaml << YAML::Key << "parameters" << YAML::Value  //
         << YAML::BeginSeq;
    for (auto const& i : params) {
      auto declname = std::string{i.node().child("declname").child_value()};
      yaml << YAML::BeginMap                                           //
           << YAML::Key << "id" << YAML::Value << declname             //
           << YAML::Key << "var_type" << YAML::Value                   //
           << YAML::Literal << LinkedTextType(i.node().child("type"))  //
           << YAML::EndMap;
    }
    yaml << YAML::EndSeq;
  }
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

// Generate the `syntax` element for a class.
void AppendClassSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node const& node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << ClassSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "compoundname");
  yaml << YAML::EndMap;
}

}  // namespace docfx
