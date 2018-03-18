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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_RANGE_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_RANGE_H_

#include "bigtable/client/version.h"

#include <google/bigtable/v2/data.pb.h>

#include <chrono>

#include "bigtable/client/internal/prefix_range_end.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interfaces to create row key ranges.
 *
 * Example:
 * @code
 * // Create a range for the keys starting with the given prefix.
 * auto range = bigtable::RowRange("foo/");
 * @endcode
 */
class RowRange {
 public:
  explicit RowRange(::google::bigtable::v2::RowRange rhs)
      : row_range_(std::move(rhs)) {}

  RowRange(RowRange&& rhs) noexcept = default;
  RowRange& operator=(RowRange&& rhs) noexcept = default;
  RowRange(RowRange const& rhs) = default;
  RowRange& operator=(RowRange const& rhs) = default;

  /// Return the infinite range, i.e., a range including all possible keys.
  static RowRange InfiniteRange() { return RowRange(); }

  /// Return the range starting at @p begin (included), with no upper limit.
  static RowRange StartingAt(std::string begin) {
    RowRange result;
    result.row_range_.set_start_key_closed(std::move(begin));
    return result;
  }

  /// Return the range ending at @p end (included), with no lower limit.
  static RowRange EndingAt(std::string end) {
    RowRange result;
    result.row_range_.set_end_key_closed(std::move(end));
    return result;
  }

  /// Return an empty range.
  static RowRange Empty() {
    RowRange result;
    result.row_range_.set_start_key_open("");
    result.row_range_.set_end_key_open("");
    return result;
  }

  /// Return the range representing the interval [@p begin, @p end).
  static RowRange Range(std::string begin, std::string end) {
    return RightOpen(std::move(begin), std::move(end));
  }

  /// Return a range that contains all the keys starting with @p prefix.
  static RowRange Prefix(std::string prefix) {
    auto end = internal::PrefixRangeEnd(prefix);
    return RightOpen(std::move(prefix), std::move(end));
  }

  //@{
  /// @name Less common, yet sometimes useful, ranges.
  /// Return a range representing the interval [@p begin, @p end).
  static RowRange RightOpen(std::string begin, std::string end) {
    RowRange result;
    if (not begin.empty()) {
      result.row_range_.set_start_key_closed(std::move(begin));
    }
    if (not end.empty()) {
      result.row_range_.set_end_key_open(std::move(end));
    }
    return result;
  }

  /// Return a range representing the interval (@p begin, @p end].
  static RowRange LeftOpen(std::string begin, std::string end) {
    RowRange result;
    if (not begin.empty()) {
      result.row_range_.set_start_key_open(std::move(begin));
    }
    if (not end.empty()) {
      result.row_range_.set_end_key_closed(std::move(end));
    }
    return result;
  }

  /// Return a range representing the interval (@p begin, @p end).
  static RowRange Open(std::string begin, std::string end) {
    RowRange result;
    if (not begin.empty()) {
      result.row_range_.set_start_key_open(std::move(begin));
    }
    if (not end.empty()) {
      result.row_range_.set_end_key_open(std::move(end));
    }
    return result;
  }

  /// Return a range representing the interval [@p begin, @p end].
  static RowRange Closed(std::string begin, std::string end) {
    RowRange result;
    if (not begin.empty()) {
      result.row_range_.set_start_key_closed(std::move(begin));
    }
    if (not end.empty()) {
      result.row_range_.set_end_key_closed(std::move(end));
    }
    return result;
  }
  //@}

  /**
   * Return true if the range is empty.
   *
   * Note that some ranges (such as `["", ""]`) are not empty but only include
   * invalid row keys.
   */
  bool IsEmpty() const;

  /// Return true if @p key is in the range.
  bool Contains(std::string const& key) const;

  /**
   * Compute the intersection against another RowRange.
   *
   * @return a 2-tuple, the first element is a boolean, with value `true` if
   *     there is some intersection, the second element is the intersection.
   *     If there is no intersection the first element is `false` and the second
   *     element has a valid, but unspecified value.
   */
  std::pair<bool, RowRange> Intersect(RowRange const& range) const;

  bool operator==(RowRange const& rhs) const;
  bool operator!=(RowRange const& rhs) const { return !(*this == rhs); }

  /// Return the filter expression as a protobuf.
  ::google::bigtable::v2::RowRange as_proto() const { return row_range_; }

  /// Move out the underlying protobuf value.
  ::google::bigtable::v2::RowRange as_proto_move() {
    return std::move(row_range_);
  }

  friend std::ostream& operator<<(std::ostream& os, RowRange const& x);

 private:
  /// Private to avoid mistaken creation of uninitialized ranges.
  RowRange() {}

  /// Return true if @p key is below the start.
  bool BelowStart(std::string const& key) const;

  /// Return true if @p key is above the end.
  bool AboveEnd(std::string const& key) const;

 private:
  ::google::bigtable::v2::RowRange row_range_;
};

/// Print a human-readable representation of the range, mostly for testing.
std::ostream& operator<<(std::ostream& os, RowRange const& x);
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_RANGE_H_
