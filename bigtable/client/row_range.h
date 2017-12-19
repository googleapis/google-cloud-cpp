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

#ifndef BIGTABLE_CLIENT_DATA_H_
#define BIGTABLE_CLIENT_DATA_H_

#include <string>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

class RowRange {
 public:
  RowRange(std::string begin, std::string end)
      : start_(begin), limit_(end) {}

  // Create different types of ranges.
  static RowRange infinite_range();
  // An empty range.
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

  // The intersection of two ranges (useful for retries).
  static RowRange intersect(RowRange const& lhs, RowRange const& rhs);

  std::string debug_string() const;
  std::string GetStart() const { return start_;}
  std::string GetLimit() const { return limit_;}
 private:
  std::string start_;
  std::string limit_;

  bool IsEmpty() const;
  static const std::string EMPTY_KEY;
  static std::string Successor(std::string& prefix);
};

std::ostream& operator<<(std::ostream&, RowRange const&);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_DATA_H_
