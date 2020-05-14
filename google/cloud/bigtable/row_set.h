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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_SET_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_SET_H

#include "google/cloud/bigtable/internal/conjunction.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/bigtable/version.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Represent a (possibly non-continuous) set of row keys.
 *
 * Cloud Bigtable can scan non-continuous sets of rows, these sets can include
 * a mix of specific row keys and ranges as defined by `bigtable::RowRange`.
 */
class RowSet {
 public:
  /// Create an empty set.
  RowSet() = default;

  RowSet(RowSet&&) noexcept = default;
  RowSet& operator=(RowSet&&) noexcept = default;
  RowSet(RowSet const&) = default;
  RowSet& operator=(RowSet const&) = default;

  template <typename... Arg>
  RowSet(Arg&&... a) {  // NOLINT(google-explicit-constructor)
    AppendAll(std::forward<Arg&&>(a)...);
  }

  /// Add @p range to the set.
  void Append(RowRange range) {
    *row_set_.add_row_ranges() = std::move(range).as_proto();
  }

  /**
   * Add @p row_key to the set, minimize copies when possible.
   */
  template <typename T>
  void Append(T&& row_key) {
    *row_set_.add_row_keys() = std::forward<T>(row_key);
  }

  /**
   * Modify this object to contain the ranges and keys inside @p range.
   *
   * This function removes any rowkeys outside @p range, it removes any row
   * ranges that do not insersect with @p range, and keeps only the intersection
   * for those ranges that do intersect @p range.
   */
  RowSet Intersect(bigtable::RowRange const& range) const;

  /**
   * Returns true if the set is empty.
   *
   * A row set is empty iff passing it to a ReadRows call would never
   * cause it to return rows. This is true if the set consists of only
   * empty ranges.
   *
   * Note that a default constructed RowSet is not empty, since it
   * matches all rows in the table.
   */
  bool IsEmpty() const;

  ::google::bigtable::v2::RowSet const& as_proto() const& { return row_set_; }
  ::google::bigtable::v2::RowSet&& as_proto() && { return std::move(row_set_); }

 private:
  /// Append the arguments to the rowset.
  template <typename H, typename... Tail>
  void AppendAll(H&& head, Tail&&... a) {
    // We cannot use the initializer list expression here because the types
    // may be all different.
    Append(std::forward<H>(head));
    AppendAll(std::forward<Tail>(a)...);
  }

  /// Terminate the recursion.
  void AppendAll() {}

 private:
  ::google::bigtable::v2::RowSet row_set_;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_SET_H
