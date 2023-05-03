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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_PUBLIC_DOCS_H
#define GOOGLE_CLOUD_CPP_DOCFX_PUBLIC_DOCS_H

#include "docfx/config.h"
#include <pugixml.hpp>

namespace docfx {

// Determine if a node is part of the public documentation.
//
// Many nodes are not part of the public documentation, for example, private
// member variables, private functions, or any names in the `*internal*`
// namespaces. This helper allows us to short circuit the recursion over the
// doxygen structure when an element is not needed for the public docs.
bool IncludeInPublicDocuments(Config const& cfg, pugi::xml_node node);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_PUBLIC_DOCS_H
