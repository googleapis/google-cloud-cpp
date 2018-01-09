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

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace btproto = ::google::bigtable::v2;

bool RowRange::IsEmpty() const {
  std::string unused;
  // We do not want to copy the strings unnecessarily, so initialize a reference
  // pointing to *_key_closed() or *_key_open(), as needed.
  std::string const* start = &unused;
  bool start_open = false;
  switch (row_range_.start_key_case()) {
    case btproto::RowRange::kStartKeyClosed:
      start = &row_range_.start_key_closed();
      break;
    case btproto::RowRange::kStartKeyOpen:
      start = &row_range_.start_key_open();
      start_open = true;
      break;
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
  }
  // We need to initialize this to something to make g++ happy, but it cannot
  // be a value that is discarded in all switch() cases to make Clang happy.
  std::string const* end = &row_range_.end_key_closed();
  bool end_open = false;
  switch (row_range_.end_key_case()) {
    case btproto::RowRange::kEndKeyClosed:
      // Already initialized.
      break;
    case btproto::RowRange::kEndKeyOpen:
      end = &row_range_.end_key_open();
      end_open = true;
      break;
    case btproto::RowRange::END_KEY_NOT_SET:
      // A range ending at +infinity is never empty.
      return false;
  }

  // Compare the strings as byte vectors (careful with unsigned chars).
  int cmp = start->compare(*end);
  if (cmp == 0) {
    return start_open or end_open;
  }
  return cmp > 0;
}

bool RowRange::Contains(absl::string_view key) const {
  return not BelowStart(key) and not AboveEnd(key);
}

bool RowRange::BelowStart(absl::string_view key) const {
  switch (row_range_.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed:
      return key < row_range_.start_key_closed();
    case btproto::RowRange::kStartKeyOpen:
      return key <= row_range_.start_key_open();
  }
  return false;
}

bool RowRange::AboveEnd(absl::string_view key) const {
  switch (row_range_.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed:
      return key > row_range_.end_key_closed();
    case btproto::RowRange::kEndKeyOpen:
      return key >= row_range_.end_key_open();
  }
  return false;
}

std::pair<bool, RowRange> RowRange::Intersect(RowRange const& range) const {
  if (range.IsEmpty()) {
    return std::make_pair(false, RowRange::Empty());
  }
  std::string empty;

  // The algorithm is simple.  The two ranges have no intersection only if
  // @p range is completely below this range or complete above this range.

  // First check if the start of @p range is below *this.
  std::string const* start = &empty;
  bool start_closed = true;
  switch (range.row_range_.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed:
      if (AboveEnd(range.row_range_.start_key_closed())) {
        return std::make_pair(false, Empty());
      }
      start = &range.row_range_.start_key_closed();
      break;
    case btproto::RowRange::kStartKeyOpen:
      if (AboveEnd(range.row_range_.start_key_open())) {
        return std::make_pair(false, Empty());
      }
      start = &range.row_range_.start_key_open();
      start_closed = false;
      break;
  }

  // Then check if the end limit of @p range is below *this.
  std::string const* end = &empty;
  bool end_closed = true;
  switch (range.row_range_.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed:
      if (BelowStart(range.row_range_.end_key_closed())) {
        return std::make_pair(false, Empty());
      }
      end = &range.row_range_.end_key_closed();
      break;
    case btproto::RowRange::kEndKeyOpen:
      if (BelowStart(range.row_range_.end_key_open())) {
        return std::make_pair(false, Empty());
      }
      end = &range.row_range_.end_key_open();
      end_closed = false;
      break;
  }

  // If we hit this point there is some intersection.  Start with the current
  // range and clip it as needed.
  RowRange intersection(*this);
  // If *start is inside, then the intersection must start at *start, and it is
  // a closed interval on the left.  Note that otherwise the existing start is
  // the right place for the intersection.
  if (intersection.Contains(*start)) {
    if (start_closed) {
      intersection.row_range_.set_start_key_closed(*start);
    } else {
      intersection.row_range_.set_start_key_open(*start);
    }
  }
  // Do the analogous thing for *end.
  if (intersection.Contains(*end)) {
    if (end_closed) {
      intersection.row_range_.set_end_key_closed(*end);
    } else {
      intersection.row_range_.set_end_key_open(*end);
    }
  }

  bool is_empty = intersection.IsEmpty();
  return std::make_pair(not is_empty, std::move(intersection));
}

bool RowRange::operator==(RowRange const& rhs) const {
  if (row_range_.start_key_case() != rhs.row_range_.start_key_case()) {
    return false;
  }
  switch (row_range_.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed:
      if (row_range_.start_key_closed() != rhs.row_range_.start_key_closed()) {
        return false;
      }
    case btproto::RowRange::kStartKeyOpen:
      if (row_range_.start_key_open() != rhs.row_range_.start_key_open()) {
        return false;
      }
  }

  if (row_range_.end_key_case() != rhs.row_range_.end_key_case()) {
    return false;
  }
  switch (row_range_.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed:
      if (row_range_.end_key_closed() != rhs.row_range_.end_key_closed()) {
        return false;
      }
    case btproto::RowRange::kEndKeyOpen:
      if (row_range_.end_key_open() != rhs.row_range_.end_key_open()) {
        return false;
      }
  }

  return true;
}

std::ostream& operator<<(std::ostream& os, RowRange const& x) {
  switch (x.row_range_.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      os << "['', ";
      break;
    case btproto::RowRange::kStartKeyClosed:
      os << "['" << x.row_range_.start_key_closed() << "', ";
      break;
    case btproto::RowRange::kStartKeyOpen:
      os << "('" << x.row_range_.start_key_open() << "', ";
  }

  switch (x.row_range_.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      os << "'')";
      break;
    case btproto::RowRange::kEndKeyClosed:
      os << "'" << x.row_range_.end_key_closed() << "']";
      break;
    case btproto::RowRange::kEndKeyOpen:
      os << "'" << x.row_range_.end_key_open() << "')";
      break;
  }
  return os;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
