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

#include "google/cloud/bigtable/emulator/range_set.h"
#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include <google/bigtable/v2/data.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace detail {

int CompareRangeValues(StringRangeSet::Range::Value const& lhs,
                       StringRangeSet::Range::Value const& rhs) {
  if (absl::holds_alternative<StringRangeSet::Range::Infinity>(lhs)) {
    return absl::holds_alternative<StringRangeSet::Range::Infinity>(rhs) ? 0
                                                                         : 1;
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

bool HasOverlap(TimestampRangeSet::Range const& lhs,
                TimestampRangeSet::Range const& rhs) {
  TimestampRangeSet::Range::Value overlap_start =
      std::max(lhs.start(), rhs.start());
  TimestampRangeSet::Range::Value overlap_end =
      TimestampRangeSet::Range::EndLess()(lhs, rhs) ? lhs.end() : rhs.end();
  return !TimestampRangeSet::Range::IsEmpty(overlap_start, overlap_end);
}

bool DisjointAndSortedRangesAdjacent(StringRangeSet::Range const& lhs,
                                     StringRangeSet::Range const& rhs) {
  assert(!HasOverlap(lhs, rhs));
  assert(StringRangeSet::Range::StartLess()(lhs, rhs));
  if (lhs.end_closed() && rhs.start_open() && lhs.end() == rhs.start()) {
      return true;
  }
  if (lhs.end_open() && rhs.start_closed() && lhs.end() == rhs.start()) {
    return true;
  }
  // FIXME - ConsecutiveRowKeys should somehow take into account the allowed
  // length of the strings.
  if (lhs.end_closed() && rhs.start_closed() &&
      detail::ConsecutiveRowKeys(lhs.end(), rhs.start())) {
    return true;
  }
  return false;
}

bool DisjointAndSortedRangesAdjacent(TimestampRangeSet::Range const& lhs,
                                     TimestampRangeSet::Range const& rhs) {
  assert(!HasOverlap(lhs, rhs));
  assert(TimestampRangeSet::Range::StartLess()(lhs, rhs));
  return lhs.end() == rhs.start();
}

template <typename RangeSetType, typename RangeType>
void RangeSetInsertImpl(RangeSetType& disjoint_ranges,
                            RangeType inserted_range) {
  // Remove all ranges which either have an overlap with `inserted_range` or are
  // adjacent to it. Then add `inserted_range` with `start` and `end`
  // adjusted to cover what the removed ranges used to cover.

  auto first_to_remove = disjoint_ranges.upper_bound(inserted_range);
  // `*first_to_remove` starts strictly after `inserted_range`'s start.
  // The previous range is the first to have a chance for an overlap - it is the
  // last one, which starts at or before `inserted_range` start.
  if (first_to_remove != disjoint_ranges.begin() &&
      detail::HasOverlap(*std::prev(first_to_remove), inserted_range)) {
    std::advance(first_to_remove, -1);
  }
  // The range preceeding `first_to_remove` for sure has no overlap with
  // `inserted_range` but it may be adjacent to it. In that case we should also
  // remove it.
  if (first_to_remove != disjoint_ranges.begin() &&
      detail::DisjointAndSortedRangesAdjacent(*std::prev(first_to_remove),
                                              inserted_range)) {
    std::advance(first_to_remove, -1);
  }
  if (first_to_remove != disjoint_ranges.end()) {
    if (typename RangeType::StartLess()(*first_to_remove, inserted_range)) {
      inserted_range.set_start(*first_to_remove);
    }
    do {
      if (typename RangeType::EndLess()(inserted_range, *first_to_remove)) {
        inserted_range.set_end(*first_to_remove);
      }
      disjoint_ranges.erase(first_to_remove++);
    } while (first_to_remove != disjoint_ranges.end() &&
             (detail::HasOverlap(*first_to_remove, inserted_range) ||
              detail::DisjointAndSortedRangesAdjacent(inserted_range,
                                                      *first_to_remove)));
  }
  disjoint_ranges.insert(std::move(inserted_range));
}

}  // namespace detail

StringRangeSet::Range::Range(Value start, bool start_open, Value end,
                           bool end_open)
    : start_(std::move(start)),
      start_open_(start_open),
      end_(std::move(end)),
      end_open_(end_open) {
  assert(!Range::ValueLess()(end, start));
  assert(!absl::holds_alternative<StringRangeSet::Range::Infinity>(start) ||
         !start_open_);
  assert(!absl::holds_alternative<StringRangeSet::Range::Infinity>(end) ||
         !end_open_);
  assert(!absl::holds_alternative<StringRangeSet::Range::Infinity>(start) ||
         absl::holds_alternative<StringRangeSet::Range::Infinity>(end));
}

StatusOr<StringRangeSet::Range> StringRangeSet::Range::FromRowRange(
    google::bigtable::v2::RowRange const& row_range) {
  StringRangeSet::Range::Value start;
  bool start_open;
  if (row_range.has_start_key_open() && !row_range.start_key_open().empty()) {
    start = StringRangeSet::Range::Value(row_range.start_key_open());
    start_open = true;
  } else if (row_range.has_start_key_closed() &&
             !row_range.start_key_closed().empty()) {
    start = StringRangeSet::Range::Value(row_range.start_key_closed());
    start_open = false;
  } else {
    start = StringRangeSet::Range::Value("");
    start_open = false;
  }
  StringRangeSet::Range::Value end;
  bool end_open;
  if (row_range.has_end_key_open() && !row_range.end_key_open().empty()) {
    end = StringRangeSet::Range::Value(row_range.end_key_open());
    end_open = true;
  } else if (row_range.has_end_key_closed() &&
             !row_range.end_key_closed().empty()) {
    end = StringRangeSet::Range::Value(row_range.end_key_closed());
    end_open = false;
  } else {
    end = StringRangeSet::Range::Value(StringRangeSet::Range::Infinity{});
    end_open = false;
  }
  if (StringRangeSet::Range::ValueLess()(end, start)) {
    return InvalidArgumentError(
        "reversed `row_range`",
        GCP_ERROR_INFO().WithMetadata("row_range", row_range.DebugString()));
  }
  return StringRangeSet::Range(std::move(start), start_open, std::move(end),
                               end_open);
}

StatusOr<StringRangeSet::Range> StringRangeSet::Range::FromValueRange(
    google::bigtable::v2::ValueRange const& value_range) {
  StringRangeSet::Range::Value start;
  bool start_open;
  if (value_range.has_start_value_open() &&
      !value_range.start_value_open().empty()) {
    start = StringRangeSet::Range::Value(value_range.start_value_open());
    start_open = true;
  } else if (value_range.has_start_value_closed() &&
             !value_range.start_value_closed().empty()) {
    start = StringRangeSet::Range::Value(value_range.start_value_closed());
    start_open = false;
  } else {
    start = StringRangeSet::Range::Value("");
    start_open = false;
  }
  StringRangeSet::Range::Value end;
  bool end_open;
  if (value_range.has_end_value_open() &&
      !value_range.end_value_open().empty()) {
    end = StringRangeSet::Range::Value(value_range.end_value_open());
    end_open = true;
  } else if (value_range.has_end_value_closed() &&
             !value_range.end_value_closed().empty()) {
    end = StringRangeSet::Range::Value(value_range.end_value_closed());
    end_open = false;
  } else {
    end = StringRangeSet::Range::Value(StringRangeSet::Range::Infinity{});
    end_open = false;
  }
  if (StringRangeSet::Range::ValueLess()(end, start)) {
    return InvalidArgumentError("reversed `value_range`",
                                GCP_ERROR_INFO().WithMetadata(
                                    "value_range", value_range.DebugString()));
  }
  return StringRangeSet::Range(std::move(start), start_open, std::move(end),
                               end_open);
}

StatusOr<StringRangeSet::Range> StringRangeSet::Range::FromColumnRange(
    google::bigtable::v2::ColumnRange const& column_range) {
  StringRangeSet::Range::Value start;
  bool start_open;
  if (column_range.has_start_qualifier_open() &&
      !column_range.start_qualifier_open().empty()) {
    start = StringRangeSet::Range::Value(column_range.start_qualifier_open());
    start_open = true;
  } else if (column_range.has_start_qualifier_closed() &&
             !column_range.start_qualifier_closed().empty()) {
    start = StringRangeSet::Range::Value(column_range.start_qualifier_closed());
    start_open = false;
  } else {
    start = StringRangeSet::Range::Value("");
    start_open = false;
  }
  StringRangeSet::Range::Value end;
  bool end_open;
  if (column_range.has_end_qualifier_open() &&
      !column_range.end_qualifier_open().empty()) {
    end = StringRangeSet::Range::Value(column_range.end_qualifier_open());
    end_open = true;
  } else if (column_range.has_end_qualifier_closed() &&
             !column_range.end_qualifier_closed().empty()) {
    end = StringRangeSet::Range::Value(column_range.end_qualifier_closed());
    end_open = false;
  } else {
    end = StringRangeSet::Range::Value(StringRangeSet::Range::Infinity{});
    end_open = false;
  }
  if (StringRangeSet::Range::ValueLess()(end, start)) {
    return InvalidArgumentError(
        "reversed `column_range`",
        GCP_ERROR_INFO().WithMetadata("column_range",
                                      column_range.DebugString()));
  }
  return StringRangeSet::Range(std::move(start), start_open, std::move(end),
                               end_open);
}

void StringRangeSet::Range::set_start(Range const& source) {
  start_ = source.start();
  start_open_ = source.start_open();
}

void StringRangeSet::Range::set_end(Range const& source) {
  end_ = source.end();
  end_open_ = source.end_open();
}

bool StringRangeSet::Range::IsBelowStart(Value const &value) const {
  auto const cmp = detail::CompareRangeValues(value, start_);
  if (cmp != 0) {
    return cmp < 0;
  }
  return start_open_;
}

bool StringRangeSet::Range::IsEmpty(StringRangeSet::Range::Value const& start,
                                    bool start_open,
                                    StringRangeSet::Range::Value const& end,
                                    bool end_open) {
  auto const res_cmp = detail::CompareRangeValues(start, end);
  if (res_cmp > 0) {
    return true;
  }
  if (res_cmp == 0) {
    return start_open || end_open ||
           absl::holds_alternative<StringRangeSet::Range::Infinity>(start);
  }
  if (start_open && end_open) {
    // FIXME - ConsecutiveRowKeys should somehow take into account the allowed
    // length of the strings.
    return detail::ConsecutiveRowKeys(start, end);
  }
  return false;
}

bool StringRangeSet::Range::IsAboveEnd(Value const &value) const {
  auto const cmp = detail::CompareRangeValues(value, end_);
  if (cmp != 0) {
    return cmp > 0;
  }
  return end_open_;
}

bool StringRangeSet::Range::IsWithin(Value const &value) const {
  return !IsAboveEnd(value) && !IsBelowStart(value);
}

bool StringRangeSet::Range::IsEmpty() const {
  return Range::IsEmpty(start_, start_open_, end_, end_open_);
}

bool StringRangeSet::Range::ValueLess::operator()(Range::Value const& lhs,
                                              Range::Value const& rhs) const {
  return detail::CompareRangeValues(lhs, rhs) < 0;
}

bool StringRangeSet::Range::StartLess::operator()(Range const& lhs,
                                              Range const& rhs) const {
  auto res = detail::CompareRangeValues(lhs.start(), rhs.start());
  if (res == 0) {
    return lhs.start_closed() && rhs.start_open();
  }
  return res < 0;
}

bool StringRangeSet::Range::EndLess::operator()(Range const& lhs,
                                            Range const& rhs) const {
  auto res = detail::CompareRangeValues(lhs.end(), rhs.end());
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
  detail::RangeSetInsertImpl(disjoint_ranges_, std::move(inserted_range));
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
    return os;
  }
  os << absl::get<std::string>(value);
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

TimestampRangeSet::Range::Range(Value start, Value end)
    : start_(std::move(start)), end_(std::move(end)) {
  assert(end == std::chrono::milliseconds::zero() || start <= end);
}

StatusOr<TimestampRangeSet::Range> TimestampRangeSet::Range::FromTimestampRange(
        google::bigtable::v2::TimestampRange const& timestamp_range) {
  auto start = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::microseconds(timestamp_range.start_timestamp_micros()));
  auto end = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::microseconds(timestamp_range.end_timestamp_micros()));
  if (end != std::chrono::milliseconds::zero() && start > end) {
    return InvalidArgumentError(
        "reversed `timestamp_range`",
        GCP_ERROR_INFO().WithMetadata("timestamp_range",
                                      timestamp_range.DebugString()));
  }
  return Range(start, end);
}

bool TimestampRangeSet::Range::IsAboveEnd(Value value) const {
  return end_ != std::chrono::milliseconds::zero() && value >= end_;
}

bool TimestampRangeSet::Range::IsWithin(Value value) const {
  return !IsAboveEnd(value) && !IsBelowStart(value);
}

bool TimestampRangeSet::Range::IsEmpty(TimestampRangeSet::Range::Value start,
                                       TimestampRangeSet::Range::Value end) {
  if (end == std::chrono::milliseconds::zero()) {
    return false;
  }
  return start >= end;
}

bool TimestampRangeSet::Range::StartLess::operator()(Range const& lhs,
                                              Range const& rhs) const {
  return lhs.start() < rhs.start();
}

bool TimestampRangeSet::Range::EndLess::operator()(Range const& lhs,
                                            Range const& rhs) const {
  if (lhs.end() == std::chrono::milliseconds::zero()) {
    return false;
  }
  if (rhs.end() == std::chrono::milliseconds::zero()) {
    return true;
  }
  return lhs.end() < rhs.end();
}

TimestampRangeSet TimestampRangeSet::All() {
  TimestampRangeSet res;
  res.Insert(Range(std::chrono::milliseconds(0), std::chrono::milliseconds(0)));
  return res;
}

TimestampRangeSet TimestampRangeSet::Empty() {
  return TimestampRangeSet{};
}

void TimestampRangeSet::Insert(TimestampRangeSet::Range inserted_range) {
  detail::RangeSetInsertImpl(disjoint_ranges_, std::move(inserted_range));
}

bool operator==(TimestampRangeSet::Range const& lhs,
                TimestampRangeSet::Range const& rhs) {
  return lhs.start() == rhs.start() && lhs.end() == rhs.end();
}

std::ostream& operator<<(std::ostream& os,
                         TimestampRangeSet::Range const& range) {
  os << "[" << range.start().count() << "ms,";
  if (range.end() == std::chrono::milliseconds::zero()) {
    os << "inf";
  } else {
    os << range.end().count() << "ms";
  }
  os << ")";
  return os;
}


}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
