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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_NODE_NAME_H
#define GOOGLE_CLOUD_CPP_DOCFX_NODE_NAME_H

#include <pugixml.hpp>
#include <string>

namespace docfx {

/**
 * Returns the name of @p node for the documentation.
 *
 * We need to consistently name documentation nodes in the table of contents and
 * the DocFX yaml files. The name also depends on the node type, for example:
 * - We want fully qualified names for namespaces
 * - We want unqualified names for functions, classes, structs, etc.
 * - We want function names to include any parameter types, to distinguish
 *   overloads
 * - We want template classes to include the type parameters
 *
 * It seems better to use a single function to keep this knowledge.
 */
std::string NodeName(pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_NODE_NAME_H
