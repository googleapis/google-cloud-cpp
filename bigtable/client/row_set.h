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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_SET_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_SET_H_

#include "bigtable/client/row_range.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Represent (possibly non-continuous) set of row keys.
 *
 * Cloud Bigtable can scan non-continuous sets of rows, these sets can include
 * a mix of specific row keys and ranges as defined by `bigtable::RowRange`.
 */
class RowSet {
public:
  /// Create an empty set.
  RowSet() {}

  RowSet(RowSet&& rhs) noexcept = default;
  RowSet& operator=(RowSet&& rhs) noexcept = default;
  RowSet(RowSet const& rhs) = default;
  RowSet& operator=(RowSet const& rhs) = default;

  /// Add @p range to the set.
  void Append(RowRange range) {
    *row_set_.add_row_ranges() = range.as_proto_move();
  }

  /// Add @p row_key to the set.
  void Append(std::string row_key) {
    *row_set_.add_row_keys() = std::move(row_key);
  }

  /// Add @p row_key to the set.
  void Append(absl::string_view row_key) {
    *row_set_.add_row_keys() = static_cast<std::string>(row_key);
  }

  /// Add @p row_key to the set.
  void Append(char const* row_key) {
    Append(absl::string_view(row_key));
  }

  ::google::bigtable::v2::RowSet as_proto() const { return row_set_; }
  ::google::bigtable::v2::RowSet as_proto_move() { return std::move(row_set_); }

 private:
  ::google::bigtable::v2::RowSet row_set_;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_SET_H_
