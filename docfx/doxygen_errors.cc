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

#include "docfx/doxygen_errors.h"
#include <sstream>
#include <stdexcept>

namespace docfx {

[[noreturn]] void MissingAttribute(std::string_view where,
                                   std::string_view name, pugi::xml_node node) {
  std::ostringstream os;
  os << "Missing attribute <" << name << "> in " << where << "(): node=";
  node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
  throw std::runtime_error(std::move(os).str());
}

[[noreturn]] void MissingElement(std::string_view where, std::string_view name,
                                 pugi::xml_node node) {
  std::ostringstream os;
  os << "Missing element <" << name << " in " << where << "(): node=";
  node.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
  throw std::runtime_error(std::move(os).str());
}

[[noreturn]] void UnknownChildType(std::string_view where,
                                   pugi::xml_node child) {
  std::ostringstream os;
  os << "Unknown child in " << where << "(): node=";
  child.print(os, /*indent=*/"", /*flags=*/pugi::format_raw);
  throw std::runtime_error(std::move(os).str());
}

}  // namespace docfx
