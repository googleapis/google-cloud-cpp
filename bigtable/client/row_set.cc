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

#include "bigtable/client/row_set.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace btproto = ::google::bigtable::v2;

void RowSet::Intersect(bigtable::RowRange const& range) {
  btproto::RowSet result;
  for (auto const& key : row_set_.row_keys()) {
    if (range.Contains(key)) {
      *result.add_row_keys() = key;
    }
  }
  for (auto const& r : row_set_.row_ranges()) {
    auto i = range.Intersect(RowRange(r));
    if (std::get<0>(i)) {
      *result.add_row_ranges() = std::get<1>(i).as_proto_move();
    }
  }
  row_set_.Swap(&result);
}
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
