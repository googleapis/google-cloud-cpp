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

#include "google/cloud/bigtable/row_set.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
RowSet RowSet::Intersect(bigtable::RowRange const& range) const {
  // Special case: "all rows", return the argument range.
  if (row_set_.row_keys().empty() and row_set_.row_ranges().empty()) {
    return RowSet(range);
  }
  // Normal case: find the intersection with
  // row keys and row ranges in the RowSet.
  RowSet result;
  for (auto const& key : row_set_.row_keys()) {
    if (range.Contains(key)) {
      *result.row_set_.add_row_keys() = key;
    }
  }
  for (auto const& r : row_set_.row_ranges()) {
    auto i = range.Intersect(RowRange(r));
    if (std::get<0>(i)) {
      *result.row_set_.add_row_ranges() = std::get<1>(i).as_proto_move();
    }
  }
  // Another special case: a RowSet() with no entries
  // means "all rows", but we want "no rows".
  if (result.row_set_.row_keys().empty() and
      result.row_set_.row_ranges().empty()) {
    return RowSet(bigtable::RowRange::Empty());
  }
  return result;
}

bool RowSet::IsEmpty() const {
  if (row_set_.row_keys_size() > 0) {
    return false;
  }
  for (auto const& r : row_set_.row_ranges()) {
    if (not RowRange(r).IsEmpty()) {
      return false;
    }
  }
  // We are left with only empty ranges (empty) or nothing at all
  // (meaning "all rows").
  return row_set_.row_ranges_size() > 0;
}
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
