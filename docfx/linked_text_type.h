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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_LINKED_TEXT_TYPE_H
#define GOOGLE_CLOUD_CPP_DOCFX_LINKED_TEXT_TYPE_H

#include <pugixml.hpp>
#include <string>

namespace docfx {

/**
 * Returns an element of type `linkedTextType` as a simple string.
 *
 * Doxygen nodes of `linkedTextType` appear in many contexts. We often need to
 * format them as simple strings as they appear in the name of documented
 * elements, including function prototypes and template parameters.
 */
std::string LinkedTextType(pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_LINKED_TEXT_TYPE_H
