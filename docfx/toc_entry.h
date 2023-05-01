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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_TOC_ENTRY_H
#define GOOGLE_CLOUD_CPP_DOCFX_TOC_ENTRY_H

#include <pugixml.hpp>
#include <iosfwd>
#include <list>
#include <map>
#include <memory>
#include <string>

namespace docfx {

struct TocEntry;
using TocItems = std::list<std::shared_ptr<TocEntry>>;

/**
 * An entry in the Table of Contents.
 *
 * The table of contents is a hierarchical data structure. Each node contains
 * a name, an optional set of attributes and then a list of nodes.
 *
 * The attributes are optional, but the following values are common:
 * - `href`: the name of a file that the node links to.
 * - `uid`: the uid of the documented element.
 */
struct TocEntry {
  std::string name;
  std::map<std::string, std::string> attr;
  TocItems items;
};

/// Compare two ToC entries, used in unit tests.
bool operator==(TocEntry const& lhs, TocEntry const& rhs);

/// Compare two ToC entries, used in unit tests.
inline bool operator!=(TocEntry const& lhs, TocEntry const& rhs) {
  return !(lhs == rhs);
}

/// Print out the tree, mostly for troubleshooting.
std::ostream& operator<<(std::ostream& os, TocEntry const& entry);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_TOC_ENTRY_H
