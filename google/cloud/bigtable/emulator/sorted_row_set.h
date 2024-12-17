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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_SORTED_ROW_SET_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_SORTED_ROW_SET_H

#include <google/bigtable/v2/data.pb.h>
#include "google/cloud/bigtable/internal/row_range_helpers.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

class SortedRowSet {
 public:
  static StatusOr<SortedRowSet> Create(
      google::bigtable::v2::RowSet const& row_set);
  static SortedRowSet AllRows();

  void Insert(google::bigtable::v2::RowRange inserted_range);
  std::set<google::bigtable::v2::RowRange, internal::RowRangeHelpers::StartLess> const&
  disjoint_ranges() const {
    return disjoint_ranges_;
  };

 private:
  std::set<google::bigtable::v2::RowRange, internal::RowRangeHelpers::StartLess> disjoint_ranges_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_SORTED_ROW_SET_H
