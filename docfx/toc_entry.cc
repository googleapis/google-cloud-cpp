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

#include "docfx/toc_entry.h"
#include <algorithm>
#include <iostream>

namespace docfx {

bool operator==(TocEntry const& lhs, TocEntry const& rhs) {
  return lhs.name == rhs.name && lhs.attr == rhs.attr &&
         std::equal(lhs.items.begin(), lhs.items.end(),  //
                    rhs.items.begin(), rhs.items.end(),  //
                    [](auto const& a, auto const& b) { return *a == *b; });
}

std::ostream& operator<<(std::ostream& os, TocEntry const& entry) {
  os << "{name=" << entry.name << ", attr={";
  auto sep = std::string_view{", attr={"};
  for (auto const& [key, value] : entry.attr) {
    os << sep << key << "=" << value;
    sep = ", ";
  }
  if (!entry.attr.empty()) os << "}";
  sep = ", items=[";
  for (auto const& item : entry.items) {
    os << sep << *item;
    sep = ", ";
  }
  if (!entry.items.empty()) os << "]";
  return os << "}";
}

}  // namespace docfx
