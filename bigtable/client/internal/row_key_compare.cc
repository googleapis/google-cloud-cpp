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
  auto len = std::min(lhs.size(), rhs.size());
  int cmp = std::memcmp(lhs.data(), rhs.data(), len);
  if (cmp != 0) {
    return cmp;
  }
  if (len == lhs.size()) {
    if (len < rhs.size()) {
      return -1;
    }
    // Note that len cannot be > than rhs.size().
    return 0;
  }
  // Note that in this case len == rhs.size() > lhs.size().
  return 1;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
