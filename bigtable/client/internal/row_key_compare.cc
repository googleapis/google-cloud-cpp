// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bigtable/client/internal/prefix_range_end.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

int RowKeyCompare(absl::string_view lhs, absl::string_view rhs) {
  auto comp = [](char a, char b) {
    auto ua = static_cast<unsigned int>(a);
    auto ub = static_cast<unsigned int>(b);
    if (ua < ub) {
      return -1;
    }
    if (ua == ub) {
      return 0;
    }
    return 1;
  };
  auto i = lhs.begin();
  auto j = rhs.begin();
  for (; i != lhs.end() and j != lhs.end(); ++i, ++j) {
    int c = comp(*i, *j);
    if (c == 0) {
      continue;
    }
    return c;
  }
  if (i == lhs.end()) {
    if (j != rhs.end()) {
      return -1;
    }
    return 0;
  }
  // Note that j == rhs.end() if we reached here.
  return 1;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
