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

#include "bigtable/client/row_range.h"

#include <cstdlib>
#include <iostream>
#include <thread>

#include <absl/memory/memory.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

const std::string RowRange::EMPTY_KEY = "\377\377\377";;
// Any range [x, x).
RowRange RowRange::empty() {
  return RowRange(RowRange::EMPTY_KEY, RowRange::EMPTY_KEY);
}

RowRange RowRange::infinite_range() {
  return RowRange("", "");
}

// Default one: [begin, end).
RowRange RowRange::right_open(std::string begin, std::string end) {
  return RowRange(begin, end);
}

// (begin, end).
RowRange RowRange::open_interval(std::string begin, std::string end) {
  return RowRange(Successor(begin), end);
}

// [begin, end].
RowRange RowRange::closed_interval(std::string begin, std::string end) {
  return RowRange(begin, Successor(end));
}

// (begin, end].
RowRange RowRange::left_open(std::string begin, std::string end) {
  return RowRange(Successor(begin), Successor(end));
}

RowRange RowRange::prefix_range(std::string prefix) {
  return RowRange(prefix, Successor(prefix));
}
  // An alias for right_open().
RowRange RowRange::range(std::string begin, std::string end) {
  return right_open(begin, end);
}

std::string RowRange::Successor(std::string& key) {
  if (key == "") {
    return "";
  }
  int position=key.length()-1;
  for ( ; position >= 0 && key[position] == '\377'; position--) continue;

  if (position<0) {
    return "";
  }
  const char last_char = key[position]+1;
  std::string result = key.substr(0, position);
  result += last_char;
  return result;
}

bool RowRange::contains(std::string_view row) const {
  return (start_ <= row) && (row < limit_);
}

bool RowRange::operator==(RowRange const& rhs) const {
  return ((this->IsEmpty() && rhs.IsEmpty()) ||
          (this->start_ == rhs.start_ && this->limit_ == rhs.limit_));
}

// The intersection of two ranges (useful for retries).
RowRange RowRange::intersect(RowRange const& lhs, RowRange const& rhs) {
  std::string result_start, result_limit;

  result_start = std::max(lhs.GetStart(), rhs.GetStart());
  result_limit = std::min(lhs.GetLimit(), rhs.GetLimit());

  // Avoid making  result->limit_ smaller than result->start_
  if (result_limit < result_start) {
    result_limit = result_start;
  }
  return RowRange(result_start, result_limit);
}

bool RowRange::IsEmpty() const {
  return ((this->start_ != "" && this->limit_ != "") &&
          this->start_ == this->limit_);
}

std::string RowRange::debug_string() const {
  std::string result = "[ " + this->start_ + " .. " + this->limit_ + " )";
  return result;
}

std::ostream& operator<<(std::ostream& out, RowRange const& rhs) {
  return out << rhs.debug_string();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
