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

#include <iosfwd>
#include <string>

namespace docfx {

/// An entry in table of contents
struct TocEntry {
  std::string filename;
  std::string name;
};

inline bool operator==(TocEntry const& lhs, TocEntry const& rhs) {
  return lhs.filename == rhs.filename && lhs.name == rhs.name;
}

inline bool operator!=(TocEntry const& lhs, TocEntry const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, TocEntry const& entry);

}  // namespace docfx

#endif  // GOOGLE_CLOUD_CPP_DOCFX_TOC_ENTRY_H
