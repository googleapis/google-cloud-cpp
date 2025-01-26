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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTERED_MAP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTERED_MAP_H

#include "google/cloud/bigtable/emulator/string_range_set.h"
#include <functional>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

template <typename StringKeyedMap>
class FilteredMap {
 public:
  FilteredMap(StringKeyedMap const& unfiltered,
              std::shared_ptr<StringRangeSet> filter)
      : unfiltered_(std::cref(unfiltered)), filter_(std::move(filter)) {}
  class const_iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = typename std::iterator_traits<
        typename StringKeyedMap::const_iterator>::value_type;
    using difference_type = typename std::iterator_traits<
        typename StringKeyedMap::const_iterator>::difference_type;
    using reference = value_type const&;
    using pointer = value_type*;

    const_iterator(
        FilteredMap const& parent,
        typename StringKeyedMap::const_iterator unfiltered_pos,
        std::set<StringRangeSet::Range,
                 StringRangeSet::RangeStartLess>::const_iterator filter_pos)
        : parent_(std::cref(parent)),
          unfiltered_pos_(std::move(unfiltered_pos)),
          filter_pos_(std::move(filter_pos)) {
      AdvanceToNextRange();
      EnsureIteratorValid();
    }

    const_iterator& operator++() {
      ++unfiltered_pos_;
      EnsureIteratorValid();
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator retval = *this;
      ++(*this);
      return retval;
    }

    bool operator==(const_iterator const& other) const {
      return unfiltered_pos_ == other.unfiltered_pos_;
    }

    bool operator!=(const_iterator const& other) const {
      return !(*this == other);
    }

    reference operator*() const { return *unfiltered_pos_; }

   private:
    void AdvanceToNextRange() {
      if (filter_pos_ == parent_.get().filter_->disjoint_ranges().end()) {
        // We've reached the end.
        unfiltered_pos_ = parent_.get().unfiltered_.get().end();
        return;
      }
      if (unfiltered_pos_ == parent_.get().unfiltered_.get().end()) {
        // unfiltered_pos_ is already pointing far enough.
        return;
      }
      if (!filter_pos_->IsBelowStart(unfiltered_pos_->first)) {
        // unfiltered_pos_ is already pointing far enough.
        return;
      }

      if (absl::holds_alternative<StringRangeSet::Range::Infinity>(
              filter_pos_->start())) {
        // Defensive programming - this should be dead code - it means we've got
        // a range which starts at infinity.
        unfiltered_pos_ = parent_.get().unfiltered_.get().end();
        return;
      }

      if (filter_pos_->start_closed()) {
        unfiltered_pos_ = parent_.get().unfiltered_.get().lower_bound(
            absl::get<std ::string>(filter_pos_->start()));
      } else {
        unfiltered_pos_ = parent_.get().unfiltered_.get().upper_bound(
            absl::get<std::string>(filter_pos_->start()));
      }
    }

    void EnsureIteratorValid() {
      // `unfiltered_pos_` may point to a row which is past the end of the range
      // pointed by filter_pos_. Make sure this only happens when the iteration
      // reaches its end.
      while (unfiltered_pos_ != parent_.get().unfiltered_.get().end() &&
             filter_pos_ != parent_.get().filter_->disjoint_ranges().end() &&
             filter_pos_->IsAboveEnd(unfiltered_pos_->first)) {
        ++filter_pos_;
        AdvanceToNextRange();
      }
      // This situation indicates that there are no rows which start after
      // current (as pointed by `filter_pos_`) range's start. Given that we're
      // traversing `filter_` in order, there will be no such rows for
      // following ranges, i.e. we've reached the end.
    }

    std::reference_wrapper<FilteredMap const> parent_;
    typename StringKeyedMap::const_iterator unfiltered_pos_;
    std::set<StringRangeSet::Range,
             StringRangeSet::RangeStartLess>::const_iterator filter_pos_;
  };

  const_iterator begin() const {
    return const_iterator(*this, unfiltered_.get().begin(),
                          filter_->disjoint_ranges().begin());
  }
  const_iterator end() const {
    return const_iterator(*this, unfiltered_.get().end(),
                          filter_->disjoint_ranges().end());
  }
 private:
  std::reference_wrapper<StringKeyedMap const> unfiltered_;
  std::shared_ptr<StringRangeSet> filter_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTERED_MAP_H
