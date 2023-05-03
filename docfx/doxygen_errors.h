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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN_ERRORS_H
#define GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN_ERRORS_H

#include <pugixml.hpp>
#include <string_view>

namespace docfx {

/// Throws an exception indicating an expected attribute is missing.
[[noreturn]] void MissingAttribute(std::string_view where,
                                   std::string_view name, pugi::xml_node node);

/// Throws an exception indicating an expected element is missing.
[[noreturn]] void MissingElement(std::string_view where, std::string_view name,
                                 pugi::xml_node node);

/// Throws an exception indicating child node with an unknown type was found.
[[noreturn]] void UnknownChildType(std::string_view where,
                                   pugi::xml_node child);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_DOXYGEN_ERRORS_H
