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

#include "google/cloud/bigtable/emulator/sorted_row_set.h"
#include "google/cloud/bigtable/internal/row_range_helpers.h"
#include "google/cloud/bigtable/row_range.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

namespace btproto = google::bigtable::v2;

bool HasOverlap(btproto::RowRange const& lhs, btproto::RowRange const& rhs) {
  return internal::RowRangeHelpers::Intersect(lhs, rhs).first;
}

bool DisjointRangesAdjacent(btproto::RowRange const& left,
                            btproto::RowRange const& right) {
  assert(internal::RowRangeHelpers::StartLess()(left, right));
  if (left.has_end_key_closed() &&
      right.has_start_key_open() &&
      left.end_key_closed() == right.start_key_open()) {
    return true;
  }
  if (left.has_end_key_open() &&
      right.has_start_key_closed() &&
      left.end_key_open() == right.start_key_closed()) {
    return true;
  }
  if (left.has_end_key_closed() &&
      right.has_start_key_closed() &&
      internal::ConsecutiveRowKeys(left.end_key_closed(),
                                   right.start_key_closed())) {
    return true;
  }
  return false;
}

}  // anonymous namespace

StatusOr<SortedRowSet> SortedRowSet::Create(
    google::bigtable::v2::RowSet const& row_set) {
  SortedRowSet res;
  for (auto const& row_key : row_set.row_keys()) {
    if (row_key.empty()) {
      return InvalidArgumentError(
          "`row_key` empty",
          GCP_ERROR_INFO().WithMetadata("row_set", row_set.DebugString()));
    }
    btproto::RowRange to_insert;
    to_insert.set_start_key_closed(row_key);
    to_insert.set_end_key_closed(row_key);
    res.Insert(std::move(to_insert));
  }
  for (auto const& row_range : row_set.row_ranges()) {
    btproto::RowRange to_insert(row_range);
    internal::RowRangeHelpers::SanitizeEmptyEndKeys(to_insert);
    if (internal::RowRangeHelpers::IsEmpty(to_insert)) {
      continue;
    }
    res.Insert(row_range);
  }
  return res;
}

SortedRowSet SortedRowSet::AllRows() {
  SortedRowSet res;
  res.Insert(btproto::RowRange());
  return res;
}

void SortedRowSet::Insert(btproto::RowRange inserted_range) {
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
      DisjointRangesAdjacent(*std::prev(first_to_remove), inserted_range)) {
    std::advance(first_to_remove, -1);
  }
  if (first_to_remove != disjoint_ranges_.end()) {
    if (internal::RowRangeHelpers::StartLess()(*first_to_remove,
                                               inserted_range)) {
      *inserted_range.mutable_start_key_open() =
          first_to_remove->start_key_open();
      *inserted_range.mutable_start_key_closed() =
          first_to_remove->start_key_closed();
    }
    do {
      if (internal::RowRangeHelpers::EndLess()(inserted_range,
                                               *first_to_remove)) {
        *inserted_range.mutable_end_key_open() =
            first_to_remove->end_key_open();
        *inserted_range.mutable_end_key_closed() =
            first_to_remove->end_key_closed();
      }
      disjoint_ranges_.erase(first_to_remove++);
    } while (first_to_remove != disjoint_ranges_.end() &&
             (HasOverlap(*first_to_remove, inserted_range) ||
              DisjointRangesAdjacent(inserted_range, *first_to_remove)));
  }
  disjoint_ranges_.insert(std::move(inserted_range));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
