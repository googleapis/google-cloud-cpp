// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/row_range_helpers.h"
#include "google/cloud/bigtable/internal/google_bytes_traits.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace btproto = ::google::bigtable::v2;

btproto::RowRange RowRangeHelpers::Empty() {
  btproto::RowRange result;
  // Return an open interval that contains no key, using "\0" for the end key.
  // We can't use "", because when appearing as the end it means 'infinity'.
  result.set_start_key_open("");
  result.set_end_key_open(std::string("\0", 1));
  return result;
}

bool RowRangeHelpers::IsEmpty(btproto::RowRange const& row_range) {
  RowKeyType unused;
  // We do not want to copy the strings unnecessarily, so initialize a reference
  // pointing to *_key_closed() or *_key_open(), as needed.
  auto const* start = &unused;
  bool start_open = false;
  switch (row_range.start_key_case()) {
    case btproto::RowRange::kStartKeyClosed:
      start = &row_range.start_key_closed();
      break;
    case btproto::RowRange::kStartKeyOpen:
      start = &row_range.start_key_open();
      start_open = true;
      break;
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
  }
  // We need to initialize this to something to make g++ happy, but it cannot
  // be a value that is discarded in all switch() cases to make Clang happy.
  auto const* end = &row_range.end_key_closed();
  bool end_open = false;
  switch (row_range.end_key_case()) {
    case btproto::RowRange::kEndKeyClosed:
      // Already initialized.
      break;
    case btproto::RowRange::kEndKeyOpen:
      end = &row_range.end_key_open();
      end_open = true;
      break;
    case btproto::RowRange::END_KEY_NOT_SET:
      // A range ending at +infinity is never empty.
      return false;
  }

  // Special case of an open interval of two consecutive strings.
  if (start_open && end_open && internal::ConsecutiveRowKeys(*start, *end)) {
    return true;
  }

  // Compare the strings as byte vectors (careful with unsigned chars).
  int cmp = internal::CompareRowKey(*start, *end);
  if (cmp == 0) {
    return start_open || end_open;
  }
  return cmp > 0;
}

bool RowRangeHelpers::BelowStart(btproto::RowRange const& row_range,
                                 RowKeyType const& key) {
  switch (row_range.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed:
      return key < row_range.start_key_closed();
    case btproto::RowRange::kStartKeyOpen:
      return key <= row_range.start_key_open();
  }
  return false;
}

bool RowRangeHelpers::AboveEnd(btproto::RowRange const& row_range,
                               RowKeyType const& key) {
  switch (row_range.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed:
      return key > row_range.end_key_closed();
    case btproto::RowRange::kEndKeyOpen:
      return key >= row_range.end_key_open();
  }
  return false;
}

std::pair<bool, btproto::RowRange> RowRangeHelpers::Intersect(
    btproto::RowRange const& lhs, btproto::RowRange const& rhs) {
  if (IsEmpty(rhs)) {
    return std::make_pair(false, Empty());
  }
  std::string empty;

  // The algorithm is simple: start with lhs as a the resulting range. Update
  // both endpoints based on the value of @p range.  If the resulting range is
  // empty there is no intersection.
  btproto::RowRange intersection(lhs);

  switch (rhs.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed: {
      auto const& start = rhs.start_key_closed();
      // If `range` starts above the current range then there is no
      // intersection.
      if (AboveEnd(intersection, start)) {
        return std::make_pair(false, Empty());
      }
      // If `start` is inside the intersection (as computed so far), then the
      // intersection must start at `start`, and it would be closed if `range`
      // is closed at the start.
      if (Contains(intersection, start)) {
        intersection.set_start_key_closed(start);
      }
      break;
    }
    case btproto::RowRange::kStartKeyOpen: {
      // The case where `range` is open on the start point is analogous.
      auto const& start = rhs.start_key_open();
      if (AboveEnd(intersection, start)) {
        return std::make_pair(false, Empty());
      }
      if (Contains(intersection, start)) {
        intersection.set_start_key_open(start);
      }
    } break;
  }

  // Then check if the end limit of @p range is below *this.
  switch (rhs.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed: {
      // If `range` ends before the start of the intersection there is no
      // intersection and we can return immediately.
      auto const& end = rhs.end_key_closed();
      if (BelowStart(intersection, end)) {
        return std::make_pair(false, Empty());
      }
      // If `end` is inside the intersection as computed so far, then the
      // intersection must end at `end` and it is closed if `range` is closed
      // at the end.
      if (Contains(intersection, end)) {
        intersection.set_end_key_closed(end);
      }
    } break;
    case btproto::RowRange::kEndKeyOpen: {
      // Do the analogous thing for `end` being a open endpoint.
      auto const& end = rhs.end_key_open();
      if (BelowStart(intersection, end)) {
        return std::make_pair(false, Empty());
      }
      if (Contains(intersection, end)) {
        intersection.set_end_key_open(end);
      }
    } break;
  }

  bool is_empty = IsEmpty(intersection);
  return std::make_pair(!is_empty, std::move(intersection));
}

void RowRangeHelpers::SanitizeEmptyEndKeys(
    google::bigtable::v2::RowRange& row_range) {
  // The service treats an empty end key as end of table. Some of our
  // intersection logic does not, though. So we are best off sanitizing the
  // input, by clearing the end key if it is empty.
  if (row_range.has_end_key_closed()) {
    if (IsEmptyRowKey(row_range.end_key_closed())) {
      row_range.clear_end_key_closed();
    }
  }
  if (row_range.has_end_key_open()) {
    if (IsEmptyRowKey(row_range.end_key_open())) {
      row_range.clear_end_key_open();
    }
  }
}

bool RowRangeHelpers::StartLess::operator()(
    btproto::RowRange const& left, btproto::RowRange const& right) const {
  if (!left.has_start_key_open() && !left.has_start_key_closed()) {
    // left is empty
    return right.has_start_key_open() || right.has_start_key_closed();
  }
  // left is non-empty
  if (!right.has_start_key_open() && !right.has_start_key_closed()) {
    return false;
  }
  // both are non-empty
  auto const& left_start = left.has_start_key_closed() ? left.start_key_closed()
                                                       : left.start_key_open();
  auto const& right_start = right.has_start_key_closed()
                                ? right.start_key_closed()
                                : right.start_key_open();

  auto cmp = internal::CompareRowKey(left_start, right_start);
  if (cmp != 0) {
    return cmp < 0;
  }
  // same row key in both
  return left.has_start_key_closed() && right.has_start_key_open();
}

bool RowRangeHelpers::EndLess::operator()(
    btproto::RowRange const& left, btproto::RowRange const& right) const {
  if (!right.has_end_key_open() && !right.has_end_key_closed()) {
    // right is infinite
    return left.has_end_key_open() || left.has_end_key_closed();
  }
  // right is finite
  if (!left.has_end_key_open() && !left.has_end_key_closed()) {
    return false;
  }
  // both are finite
  auto const& left_end =
      left.has_end_key_closed() ? left.end_key_closed() : left.end_key_open();
  auto const& right_end = right.has_end_key_closed() ? right.end_key_closed()
                                                     : right.end_key_open();

  auto cmp = internal::CompareRowKey(left_end, right_end);
  if (cmp != 0) {
    return cmp < 0;
  }
  // same row key in both
  return left.has_end_key_open() && right.has_end_key_closed();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
