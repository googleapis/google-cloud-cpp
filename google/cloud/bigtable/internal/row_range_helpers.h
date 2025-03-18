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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROW_RANGE_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROW_RANGE_HELPERS_H

#include "google/cloud/bigtable/row_key.h"
#include "google/cloud/bigtable/row_range.h"
#include <google/bigtable/v2/data.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class RowRangeHelpers {
 public:
  static google::bigtable::v2::RowRange Empty();
  static bool IsEmpty(google::bigtable::v2::RowRange const& row_range);
  static bool BelowStart(google::bigtable::v2::RowRange const& row_range,
                         RowKeyType const& key);
  static bool AboveEnd(google::bigtable::v2::RowRange const& row_range,
                       RowKeyType const& key);
  static std::pair<bool, google::bigtable::v2::RowRange> Intersect(
      google::bigtable::v2::RowRange const& lhs,
      google::bigtable::v2::RowRange const& rhs);
  /// Return true if @p key is in the range.
  template <typename T>
  static bool Contains(google::bigtable::v2::RowRange const& row_range,
                       T const& key) {
    return !BelowStart(row_range, key) && !AboveEnd(row_range, key);
  }
  static void SanitizeEmptyEndKeys(google::bigtable::v2::RowRange& row_range);

  /// A Functor describing the order on range starts.
  struct StartLess {
    bool operator()(google::bigtable::v2::RowRange const& left,
                    google::bigtable::v2::RowRange const& right) const;
  };

  /// A Functor describing the order on range ends.
  struct EndLess {
    bool operator()(google::bigtable::v2::RowRange const& left,
                    google::bigtable::v2::RowRange const& right) const;
  };
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ROW_RANGE_HELPERS_H
