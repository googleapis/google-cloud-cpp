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

#include "docfx/linked_text_type.h"
#include <sstream>
#include <string_view>

namespace docfx {

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
std::string LinkedTextType(pugi::xml_node node) {
  std::ostringstream os;
  for (auto const child : node) {
    auto const name = std::string_view{child.name()};
    os << (name == "ref" ? child.child_value() : child.value());
  }
  return std::move(os).str();
}

}  // namespace docfx
