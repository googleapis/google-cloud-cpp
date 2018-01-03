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

#ifndef BIGTABLE_CLIENT_ROW_RANGE_H_
#define BIGTABLE_CLIENT_ROW_RANGE_H_

#include <iostream>
#include <string>

#include "bigtable/client/version.h"

#include <google/bigtable/v2/data.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class RowRange {
 public:
  RowRange(RowRange&& rhs) noexcept = default;
  RowRange& operator=(RowRange&& rhs) noexcept = default;
  RowRange(RowRange const& rhs) = default;
  RowRange& operator=(RowRange const& rhs) = default;
  RowRange() = default;
  RowRange(std::string begin, std::string end);

  // Create different types of ranges.
  // A range with 'end' not set; 'start' might be either set or not set.
  static RowRange infinite_range();

  // An empty range: both start and end are set, and they are the same,
  // i.e., start = end.
  static RowRange empty();

  // The typical C++ range [begin,end).
  static RowRange right_open(std::string begin, std::string end);

  // An open interval (begin, end).
  static RowRange open_interval(std::string begin, std::string end);

  // A closed interval [begin, end].
  static RowRange closed_interval(std::string begin, std::string end);

  // A half-open interval (begin, end].
  static RowRange left_open(std::string begin, std::string end);

  // All the rows with keys starting with 'prefix'.
  static RowRange prefix_range(std::string prefix);

  // An alias for right_open()
  static RowRange range(std::string begin, std::string end);

  // Some accessors.
  bool contains(std::string_view row) const;
  bool operator==(RowRange const&) const;
  bool operator!=(RowRange const& rhs) const { return !(*this == rhs); }
  std::string debug_string() const;
  // The intersection of two ranges (useful for retries).
  static RowRange intersect(RowRange const& lhs, RowRange const& rhs);

  // Return the RowRange expression as a protobuf.
  // TODO() consider a "move" operation too.
  google::bigtable::v2::RowRange as_proto() const { return range_; }

 private:
  google::bigtable::v2::RowRange range_;

  static const std::string SAMPLE_KEY;

  bool IsEmpty() const;
  static std::string Successor(std::string const& prefix);

  enum class KeyType { NOT_SET, OPEN, CLOSED };
  enum class PositionType { START, END };

  // Wrapper/Accessor methods over RowRange ProtoBuf.
  KeyType GetStartKeyType() const;
  KeyType GetEndKeyType() const;
  std::string GetStartKey() const;
  std::string GetEndKey() const;

  // A helper method for range intersection.
  // Set the result point (key) and type given values (key and type)
  // from two ranges.
  static void FixKeyPointAndTypeIntersect(PositionType position,
                                          std::string& result_key,
                                          KeyType& result_type,
                                          std::string rhs_key,
                                          KeyType rhs_type);

  static void SetProtoBufRangePoint(PositionType position, RowRange& row_range,
                                    std::string point_key, KeyType point_type);
};

std::ostream& operator<<(std::ostream&, RowRange const&);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_ROW_RANGE_H_
