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

#include "google/cloud/bigtable/row_range.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace btproto = ::google::bigtable::v2;

namespace {

/// Returns true iff a < b and there is no string c such that a < c < b.
inline bool Consecutive(std::string const& a, std::string const& b) {
  // The only way for two strings to be consecutive is for the
  // second to be equal to the first with an appended zero char.
  if (b.length() != a.length() + 1) {
    return false;
  }
  if (b.back() != '\0') {
    return false;
  }
  return b.compare(0, a.length(), a) == 0;
}

}  // anonymous namespace

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

  // Special case of an open interval of two consecutive strings.
  if (start_open && end_open && Consecutive(*start, *end)) {
    return true;
  }

  // Compare the strings as byte vectors (careful with unsigned chars).
  int cmp = start->compare(*end);
  if (cmp == 0) {
    return start_open || end_open;
  }
  return cmp > 0;
}

bool RowRange::Contains(std::string const& key) const {
  return !BelowStart(key) && !AboveEnd(key);
}

bool RowRange::BelowStart(std::string const& key) const {
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

bool RowRange::AboveEnd(std::string const& key) const {
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

  // The algorithm is simple: start with *this as a the resulting range.  Update
  // both endpoints based on the value of @p range.  If the resulting range is
  // empty there is no intersection.
  RowRange intersection(*this);

  switch (range.row_range_.start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed: {
      auto const& start = range.row_range_.start_key_closed();
      // If `range` starts above the current range then there is no
      // intersection.
      if (intersection.AboveEnd(start)) {
        return std::make_pair(false, Empty());
      }
      // If `start` is inside the intersection (as computed so far), then the
      // intersection must start at `start`, and it would be closed if `range`
      // is closed at the start.
      if (intersection.Contains(start)) {
        intersection.row_range_.set_start_key_closed(start);
      }
    } break;
    case btproto::RowRange::kStartKeyOpen: {
      // The case where `range` is open on the start point is analogous.
      auto const& start = range.row_range_.start_key_open();
      if (intersection.AboveEnd(start)) {
        return std::make_pair(false, Empty());
      }
      if (intersection.Contains(start)) {
        intersection.row_range_.set_start_key_open(start);
      }
    } break;
  }

  // Then check if the end limit of @p range is below *this.
  switch (range.row_range_.end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed: {
      // If `range` ends before the start of the intersection there is no
      // intersection and we can return immediately.
      auto const& end = range.row_range_.end_key_closed();
      if (intersection.BelowStart(end)) {
        return std::make_pair(false, Empty());
      }
      // If `end` is inside the intersection as computed so far, then the
      // intersection must end at `end` and it is closed if `range` is closed
      // at the end.
      if (intersection.Contains(end)) {
        intersection.row_range_.set_end_key_closed(end);
      }
    } break;
    case btproto::RowRange::kEndKeyOpen: {
      // Do the analogous thing for `end` being a open endpoint.
      auto const& end = range.row_range_.end_key_open();
      if (intersection.BelowStart(end)) {
        return std::make_pair(false, Empty());
      }
      if (intersection.Contains(end)) {
        intersection.row_range_.set_end_key_open(end);
      }
    } break;
  }

  bool is_empty = intersection.IsEmpty();
  return std::make_pair(!is_empty, std::move(intersection));
}

bool operator==(RowRange const& lhs, RowRange const& rhs) {
  if (lhs.as_proto().start_key_case() != rhs.as_proto().start_key_case()) {
    return false;
  }
  switch (lhs.as_proto().start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      break;
    case btproto::RowRange::kStartKeyClosed:
      if (lhs.as_proto().start_key_closed() !=
          rhs.as_proto().start_key_closed()) {
        return false;
      }
      break;
    case btproto::RowRange::kStartKeyOpen:
      if (lhs.as_proto().start_key_open() != rhs.as_proto().start_key_open()) {
        return false;
      }
      break;
  }

  if (lhs.as_proto().end_key_case() != rhs.as_proto().end_key_case()) {
    return false;
  }
  switch (lhs.as_proto().end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      break;
    case btproto::RowRange::kEndKeyClosed:
      if (lhs.as_proto().end_key_closed() != rhs.as_proto().end_key_closed()) {
        return false;
      }
      break;
    case btproto::RowRange::kEndKeyOpen:
      if (lhs.as_proto().end_key_open() != rhs.as_proto().end_key_open()) {
        return false;
      }
      break;
  }

  return true;
}

std::ostream& operator<<(std::ostream& os, RowRange const& x) {
  switch (x.as_proto().start_key_case()) {
    case btproto::RowRange::START_KEY_NOT_SET:
      os << "['', ";
      break;
    case btproto::RowRange::kStartKeyClosed:
      os << "['" << x.as_proto().start_key_closed() << "', ";
      break;
    case btproto::RowRange::kStartKeyOpen:
      os << "('" << x.as_proto().start_key_open() << "', ";
  }

  switch (x.as_proto().end_key_case()) {
    case btproto::RowRange::END_KEY_NOT_SET:
      os << "'')";
      break;
    case btproto::RowRange::kEndKeyClosed:
      os << "'" << x.as_proto().end_key_closed() << "']";
      break;
    case btproto::RowRange::kEndKeyOpen:
      os << "'" << x.as_proto().end_key_open() << "')";
      break;
  }
  return os;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
