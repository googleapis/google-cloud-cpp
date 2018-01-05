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

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
