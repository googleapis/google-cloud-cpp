// Copyright 2017 Google LLC
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

#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/bigtable/internal/row_range_helpers.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace btproto = ::google::bigtable::v2;

RowRange::RowRange(::google::bigtable::v2::RowRange rhs)
    : row_range_(std::move(rhs)) {
  internal::RowRangeHelpers::SanitizeEmptyEndKeys(row_range_);
}

RowRange RowRange::Empty() {
  return RowRange(internal::RowRangeHelpers::Empty());
}

bool RowRange::IsEmpty() const {
  return internal::RowRangeHelpers::IsEmpty(row_range_);
}

bool RowRange::BelowStart(RowKeyType const& key) const {
  return internal::RowRangeHelpers::BelowStart(row_range_, key);
}

bool RowRange::AboveEnd(RowKeyType const& key) const {
  return internal::RowRangeHelpers::AboveEnd(row_range_, key);
}

std::pair<bool, RowRange> RowRange::Intersect(RowRange const& range) const {
  auto res = internal::RowRangeHelpers::Intersect(row_range_, range.row_range_);
  return std::make_pair(res.first, RowRange(std::move(res.second)));
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
