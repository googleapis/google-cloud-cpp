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
#include "docfx/doxygen2markdown.h"
#include "docfx/doxygen_errors.h"
#include "docfx/function_classifiers.h"
#include "docfx/linked_text_type.h"
#include "docfx/yaml_emit.h"
#include <iostream>

namespace docfx {
namespace {

void AppendLocation(YAML::Emitter& yaml, YamlContext const& ctx,
                    pugi::xml_node node, char const* name_attribute) {
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

std::string HtmlEscape(std::string_view text) {
  std::string clean;
  for (auto c : text) {
    if (c == '<') {
      clean += "&lt;";
    } else if (c == '>') {
      clean += "&gt;";
    } else {
      clean += c;
    }
  }
  return clean;
}

void TemplateParamListSyntaxContent(std::ostream& os, pugi::xml_node node) {
  auto templateparamlist = node.child("templateparamlist");
  if (!templateparamlist) return;
  os << "template <";
  auto sep = std::string_view{"\n    "};
  for (auto const param : templateparamlist) {
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

std::string ReturnDescription(YamlContext const& /*ctx*/, pugi::xml_node node) {
  // The return description, if present, is in a `<simplesect>` node that is
  // part of the *function* description.
  auto selected = node.select_node(".//simplesect[@kind='return']");
  if (!selected) return {};
  std::ostringstream os;
  MarkdownContext mdctx;
  mdctx.paragraph_start = "";
  AppendDescriptionType(os, mdctx, selected.node());
  return std::move(os).str();
}

// We need to search the parameters in the `<parameterlist>` element. The type
// of this element is defined as below. Note that this is basically a sequence
// of "parameter items". The "parameter items" contain the description, and
// may contain a *list* of parameter names.
//
// clang-format off
//   <xsd:complexType name="docParamListType">
//     <xsd:sequence>
//       <xsd:element name="parameteritem" type="docParamListItem" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//     <xsd:attribute name="kind" type="DoxParamListKind" />
//   </xsd:complexType>
//   <xsd:complexType name="docParamListItem">
//     <xsd:sequence>
//       <xsd:element name="parameternamelist" type="docParamNameList" minOccurs="0" maxOccurs="unbounded" />
//       <xsd:element name="parameterdescription" type="descriptionType" />
//     </xsd:sequence>
//   </xsd:complexType>
//   <xsd:complexType name="docParamNameList">
//     <xsd:sequence>
//       <xsd:element name="parametertype" type="docParamType" minOccurs="0" maxOccurs="unbounded" />
//       <xsd:element name="parametername" type="docParamName" minOccurs="0" maxOccurs="unbounded" />
//     </xsd:sequence>
//   </xsd:complexType>
// clang-format on
bool ParameterItemMatchesName(std::string_view parameter_name,
                              pugi::xml_node item) {
  for (auto const list : item.children("parameternamelist")) {
    for (auto const name : list.children("parametername")) {
      if (std::string_view{name.child_value()} == parameter_name) return true;
    }
  }
  return false;
}

std::string ParameterItemDescription(pugi::xml_node parameteritem) {
  std::ostringstream os;
  MarkdownContext mdctx;
  mdctx.paragraph_start = "";
  AppendDescriptionType(os, mdctx, parameteritem.child("parameterdescription"));
  return std::move(os).str();
}

std::string ParameterDescription(YamlContext const& /*ctx*/,
                                 pugi::xml_node node,
                                 std::string_view parameter_name) {
  // The parameter description, if present, is in a `<simplesect>` node that is
  // part of the *function* description.
  auto selected = node.select_node(".//parameterlist[@kind='param']");
  if (!selected) return {};
  for (auto const item : selected.node()) {
    if (!ParameterItemMatchesName(parameter_name, item)) continue;
    return ParameterItemDescription(item);
  }
  return {};
}

std::string TemplateParameterDescription(YamlContext const& /*ctx*/,
                                         pugi::xml_node node,
                                         std::string_view type) {
  auto const prefix = std::string_view{"typename "};
  if (type.substr(0, prefix.size()) == prefix) {
    type = type.substr(prefix.size());
  }
  // The parameter description, if present, is in a `<simplesect>` node that is
  // part of the *function* description.
  auto selected = node.select_node(".//parameterlist[@kind='templateparam']");
  if (!selected) return {};
  for (auto const item : selected.node()) {
    if (!ParameterItemMatchesName(type, item)) continue;
    return ParameterItemDescription(item);
  }
  return {};
}

}  // namespace

std::string EnumSyntaxContent(pugi::xml_node node) {
  std::ostringstream os;
  auto const strong = std::string_view{node.attribute("strong").as_string()};
  os << "enum " << (strong == "yes" ? "class " : "")
     << node.child_value("qualifiedname") << " {\n";
  for (auto const child : node) {
    if (std::string_view{child.name()} != "enumvalue") continue;
    os << "  " << child.child_value("name") << ",\n";
  }
  os << "};";
  return std::move(os).str();
}

std::string TypedefSyntaxContent(pugi::xml_node node) {
  std::ostringstream os;
  os << "using " << node.child_value("qualifiedname") << " =\n"  //
     << "  " << LinkedTextType(node.child("type")) << ";";
  return std::move(os).str();
}

std::string VariableSyntaxContent(pugi::xml_node node) {
  std::ostringstream os;
  os << LinkedTextType(node.child("type")) << " " << node.child_value("name")
     << ";";
  return std::move(os).str();
}

std::string FriendSyntaxContent(pugi::xml_node node) {
  auto type = std::string_view{node.child_value("type")};
  if (IsFunction(node)) return FunctionSyntaxContent(node, "friend ");
  std::ostringstream os;
  TemplateParamListSyntaxContent(os, node);
  os << "friend " << type << " " << node.child_value("qualifiedname") << ";";
  return std::move(os).str();
}

std::string FunctionSyntaxContent(pugi::xml_node node,
                                  std::string_view prefix) {
  std::ostringstream os;
  TemplateParamListSyntaxContent(os, node);
  os << prefix;
  auto const rettype = LinkedTextType(node.child("type"));
  if (!rettype.empty()) os << rettype << "\n";
  os << node.child_value("qualifiedname") << " (";
  auto sep = std::string_view();
  auto params = node.select_nodes("param");
  if (!params.empty()) {
    sep = "\n    ";
    for (auto const& i : params) {
      os << sep << LinkedTextType(i.node().child("type"));
      auto declname = std::string_view{i.node().child_value("declname")};
      if (!declname.empty()) os << " " << declname;
      sep = ",\n    ";
    }
    sep = "\n  ";
  }
  os << sep << ")";
  return std::move(os).str();
}

std::string ClassSyntaxContent(pugi::xml_node node, std::string_view prefix) {
  // struct vs class
  auto const* const kind = node.attribute("kind").as_string();
  // If the `node` is a  '<compounddef>' element, the name of the documented
  // entity is stored in '<compoundname>'.  Sometimes classes and structs appear
  // in `<memberdef>` nodes, in that case the name is stored in the
  // `<qualifiedname>`.
  auto name = std::string_view{node.name()};
  auto const* const entity_name = name == "compounddef"
                                      ? node.child_value("compoundname")
                                      : node.child_value("qualifiedname");
  std::ostringstream os;
  os << "// Found in #include <" << node.child_value("includes") << ">\n";
  TemplateParamListSyntaxContent(os, node);
  os << prefix << kind << " " << entity_name << " { ... };";
  return std::move(os).str();
}

std::string StructSyntaxContent(pugi::xml_node node, std::string_view prefix) {
  return ClassSyntaxContent(node, prefix);
}

std::string NamespaceSyntaxContent(pugi::xml_node node) {
  std::ostringstream os;
  os << "namespace " << node.child_value("compoundname") << " { ... };";
  return std::move(os).str();
}

void AppendEnumSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                      pugi::xml_node node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << EnumSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendTypedefSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                         pugi::xml_node node) {
  auto const aliasof =
      "<code>" + HtmlEscape(LinkedTextType(node.child("type"))) + "</code>";
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << TypedefSyntaxContent(node)                               //
       << YAML::Key << "aliasof" << YAML::Value << YAML::Literal << aliasof;
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendFriendSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << FriendSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendVariableSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                          pugi::xml_node node) {
  auto full_name = std::string{node.child("qualifiedname").child_value()};
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << VariableSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendFunctionSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                          pugi::xml_node node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << FunctionSyntaxContent(node);
  auto const rettype = LinkedTextType(node.child("type"));
  if (!rettype.empty()) {
    // The `return` element accepts either a string for `type` or a sequence of
    // strings. If `type` is a string then it must be UID pointing to another
    // element in the documentation. That does not work in C++ where many
    // functions return primitive types and Doxygen does not create links for
    // the return type. So we create a sequence with a single element.
    yaml << YAML::Key << "return" << YAML::Value << YAML::BeginMap  //
         << YAML::Key << "type" << YAML::Value << YAML::BeginSeq    //
         << YAML::DoubleQuoted << rettype << YAML::EndSeq;
    auto description = ReturnDescription(ctx, node);
    if (!description.empty()) {
      yaml << YAML::Key << "description" << YAML::Value << YAML::Literal
           << description;
    }
    yaml << YAML::EndMap;
  }
  auto params = node.select_nodes("param");
  auto tparams = node.child("templateparamlist").children("param");
  if (!params.empty() || !tparams.empty()) {
    yaml << YAML::Key << "parameters" << YAML::Value  //
         << YAML::BeginSeq;
    for (auto const& i : params) {
      auto declname = std::string{i.node().child("declname").child_value()};
      yaml << YAML::BeginMap                                //
           << YAML::Key << "id" << YAML::Value << declname  //
           << YAML::Key << "var_type" << YAML::Value        //
           << YAML::DoubleQuoted
           << HtmlEscape(LinkedTextType(i.node().child("type")));
      auto description = ParameterDescription(ctx, node, declname);
      if (!description.empty()) {
        yaml << YAML::Key << "description" << YAML::Value << YAML::Literal
             << description;
      }
      yaml << YAML::EndMap;
    }
    // Generate the template parameters as normal parameters, as there does not
    // seem to be any other way to document them.
    for (auto const& i : tparams) {
      auto type = std::string{i.child("type").child_value()};
      yaml << YAML::BeginMap  //
           << YAML::Key << "id" << YAML::Value << type;
      auto description = TemplateParameterDescription(ctx, node, type);
      if (!description.empty()) {
        yaml << YAML::Key << "description" << YAML::Value << YAML::Literal
             << description;
      }
      yaml << YAML::EndMap;
    }
    yaml << YAML::EndSeq;
  }
  auto exceptions = node.select_node(".//parameterlist[@kind='exception']");
  if (!!exceptions && !exceptions.node().children().empty()) {
    yaml << YAML::Key << "exceptions" << YAML::Value << YAML::BeginSeq;
    for (auto const item : exceptions.node()) {
      auto const description = ParameterItemDescription(item);
      for (auto const name : item.child("parameternamelist")) {
        auto exception_type = LinkedTextType(name);
        yaml << YAML::BeginMap                             //
             << YAML::Key << "var_type" << YAML::Value     //
             << YAML::DoubleQuoted << exception_type       //
             << YAML::Key << "description" << YAML::Value  //
             << YAML::Literal << description               //
             << YAML::EndMap;
      }
    }
    yaml << YAML::EndSeq;
  }
  AppendLocation(yaml, ctx, node, "name");
  yaml << YAML::EndMap;
}

void AppendClassSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                       pugi::xml_node node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << ClassSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "compoundname");
  yaml << YAML::EndMap;
}

void AppendStructSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                        pugi::xml_node node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << StructSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "compoundname");
  yaml << YAML::EndMap;
}

void AppendNamespaceSyntax(YAML::Emitter& yaml, YamlContext const& ctx,
                           pugi::xml_node node) {
  yaml << YAML::Key << "syntax" << YAML::Value                     //
       << YAML::BeginMap                                           //
       << YAML::Key << "contents" << YAML::Value << YAML::Literal  //
       << NamespaceSyntaxContent(node);
  AppendLocation(yaml, ctx, node, "compoundname");
  yaml << YAML::EndMap;
}

}  // namespace docfx
