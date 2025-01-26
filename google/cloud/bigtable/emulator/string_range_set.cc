// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/emulator/string_range_set.h"
#include "google/cloud/bigtable/internal/google_bytes_traits.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

int CompareRangeValues(StringRangeSet::Range::Value const& lhs,
                       StringRangeSet::Range::Value const& rhs) {
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(lhs)) {
    return absl::holds_alternative<StringRangeSet::Range::Infinity>(rhs) ? 0 : 1;
  }
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(rhs)) {
    return -1;
  }
  return internal::CompareRowKey(absl::get<std::string>(lhs),
                                 absl::get<std::string>(rhs));
}

bool ConsecutiveRowKeys(StringRangeSet::Range::Value const& lhs,
                        StringRangeSet::Range::Value const& rhs) {
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(lhs) ||
      absl::holds_alternative<StringRangeSet::Range::Infinity>(rhs)) {
    return false;
  }
  return internal::ConsecutiveRowKeys(absl::get<std::string>(lhs),
                                      absl::get<std::string>(rhs));
}

bool HasOverlap(StringRangeSet::Range const& lhs,
                StringRangeSet::Range const& rhs) {
  auto const start_cmp = CompareRangeValues(lhs.start(), rhs.start());
  StringRangeSet::Range const& intersect_start =
      (start_cmp == 0) ? (lhs.start_open() ? lhs : rhs)
                       : ((start_cmp > 0) ? lhs : rhs);
  auto const end_cmp = CompareRangeValues(lhs.end(), rhs.end());
  StringRangeSet::Range const& intersect_end = (end_cmp == 0)
                                                 ? (lhs.end_open() ? lhs : rhs)
                                                 : ((end_cmp < 0) ? lhs : rhs);
  return !StringRangeSet::Range::IsEmpty(
      intersect_start.start(), intersect_start.start_open(),
      intersect_end.end(), intersect_end.end_open());
}

bool DisjointAndSortedRangesAdjacent(StringRangeSet::Range const& lhs,
                                     StringRangeSet::Range const& rhs) {
  assert(!HasOverlap(lhs, rhs));
  assert(StringRangeSet::RangeStartLess()(lhs, rhs));
  if (lhs.end_closed() && rhs.start_open() && lhs.end() == rhs.start()) {
      return true;
  }
  if (lhs.end_open() && rhs.start_closed() && lhs.end() == rhs.start()) {
    return true;
  }
  // FIXME - ConsecutiveRowKeys should somehow take into account the allowed
  // length of the strings.
  if (lhs.end_closed() && rhs.start_closed() &&
      ConsecutiveRowKeys(lhs.end(), rhs.start())) {
    return true;
  }
  return false;
}

}  // anonymous namespace

StringRangeSet::Range::Range(Value start, bool start_open, Value end,
                           bool end_open)
    : start_(std::move(start)),
      start_open_(start_open),
      end_(std::move(end)),
      end_open_(end_open) {
  assert(!RangeValueLess()(end, start));
  assert(!absl::holds_alternative<StringRangeSet::Range::Infinity>(start) ||
         start_open_);
  assert(!absl::holds_alternative<StringRangeSet::Range::Infinity>(end) ||
         end_open_);
  assert(!absl::holds_alternative<StringRangeSet::Range::Infinity>(start) ||
         StringRangeSet::Range::IsEmpty(start_, start_open_, end_, end_open_));
}

void StringRangeSet::Range::set_start(Value start, bool start_open) {
  start_ = std::move(start);
  start_open_ = start_open;
}

void StringRangeSet::Range::set_end(Value end, bool end_open) {
  end_ = std::move(end);
  end_open_ = end_open;
}

bool StringRangeSet::Range::IsBelowStart(Value const &value) const {
  auto const cmp = CompareRangeValues(start_, value);
  if (cmp != 0) {
    return cmp < 0;
  }
  return start_open_;
}

bool StringRangeSet::Range::IsEmpty(StringRangeSet::Range::Value const& start,
                                    bool start_open,
                                    StringRangeSet::Range::Value const& end,
                                    bool end_open) {
  auto const res_cmp = CompareRangeValues(start, end);
  if (res_cmp > 0) {
    return true;
  }
  if (res_cmp == 0) {
    return start_open || end_open;
  }
  if (start_open && end_open) {
    // FIXME - ConsecutiveRowKeys should somehow take into account the allowed
    // length of the strings.
    return ConsecutiveRowKeys(start, end);
  }
  return false;
}


bool StringRangeSet::Range::IsAboveEnd(Value const &value) const {
  auto const cmp = CompareRangeValues(value, end_);
  if (cmp != 0) {
    return cmp > 0;
  }
  return end_open_;
}

bool StringRangeSet::RangeValueLess::operator()(Range::Value const& lhs,
                                              Range::Value const& rhs) const {
  return CompareRangeValues(lhs, rhs) < 0;
}

bool StringRangeSet::RangeStartLess::operator()(Range const& lhs,
                                              Range const& rhs) const {
  auto res = CompareRangeValues(lhs.start(), rhs.start());
  if (res == 0) {
    return lhs.start_closed() && rhs.start_open();
  }
  return res < 0;
}

bool StringRangeSet::RangeEndLess::operator()(Range const& lhs,
                                            Range const& rhs) const {
  auto res = CompareRangeValues(lhs.end(), rhs.end());
  if (res == 0) {
    return lhs.end_open() && rhs.end_closed();
  }
  return res < 0;
}

StringRangeSet StringRangeSet::All() {
  StringRangeSet res;
  res.Insert(Range("", false, StringRangeSet::Range::Infinity{}, true));
  return res;
}

StringRangeSet StringRangeSet::Empty() {
  return StringRangeSet{};
}

void StringRangeSet::Insert(StringRangeSet::Range inserted_range) {
  // Remove all ranges which either have an overlap with `inserted_range` or are
  // adjacent to it. Then add `inserted_range` with `start` and `end`
  // adjusted to cover what the removed ranges used to cover.

  auto first_to_remove = disjoint_ranges_.upper_bound(inserted_range);
  // `*first_to_remove` starts strictly after `inserted_range`'s start.
  // The previous range is the first to have a chance for an overlap - it is the
  // last one, which starts at or before `inserted_range` start.
  if (first_to_remove != disjoint_ranges_.begin() &&
      HasOverlap(*std::prev(first_to_remove), inserted_range)) {
    std::advance(first_to_remove, -1);
  }
  // The range preceeding `first_to_remove` for sure has no overlap with
  // `inserted_range` but it may be adjacent to it. In that case we should also
  // remove it.
  if (first_to_remove != disjoint_ranges_.begin() &&
      DisjointAndSortedRangesAdjacent(*std::prev(first_to_remove),
                                      inserted_range)) {
    std::advance(first_to_remove, -1);
  }
  if (first_to_remove != disjoint_ranges_.end()) {
    if (RangeStartLess()(*first_to_remove, inserted_range)) {
      inserted_range.set_start(std::move(first_to_remove)->start(),
                               first_to_remove->start_open());
    }
    do {
      if (RangeEndLess()(inserted_range, *first_to_remove)) {
        inserted_range.set_end(std::move(first_to_remove)->end(),
                                first_to_remove->end_open());
      }
      disjoint_ranges_.erase(first_to_remove++);
    } while (
        first_to_remove != disjoint_ranges_.end() &&
        (HasOverlap(*first_to_remove, inserted_range) ||
         DisjointAndSortedRangesAdjacent(inserted_range, *first_to_remove)));
  }
  disjoint_ranges_.insert(std::move(inserted_range));
}

bool operator==(StringRangeSet::Range::Value const& lhs,
                StringRangeSet::Range::Value const& rhs) {
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(lhs)) {
    return absl::holds_alternative<StringRangeSet::Range::Infinity>(rhs);
  }
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(rhs)) {
    return false;
  }
  return absl::get<std::string>(lhs) == absl::get<std::string>(rhs);
}

std::ostream& operator<<(std::ostream& os,
                         StringRangeSet::Range::Value const& value) {
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(value)) {
    os << "inf";
  }
  os << absl::holds_alternative<std::string>(value);
  return os;
}

bool operator==(StringRangeSet::Range const& lhs,
                StringRangeSet::Range const& rhs) {
  return lhs.start() == rhs.start() && lhs.start_open() == rhs.start_open() &&
         lhs.end() == rhs.end() && lhs.end_open() == rhs.end_open();
}

std::ostream& operator<<(std::ostream& os,
                         StringRangeSet::Range const& range) {
  os << (range.start_closed() ? "[" : "(") << range.start() << ","
     << range.end() << (range.end_closed() ? "]" : ")");
  return os;
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
