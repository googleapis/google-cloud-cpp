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

#include "google/cloud/bigtable/emulator/range_set.h"
#include <re2/re2.h>
#include <functional>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

/**
 * A map view filtering elements by whether their keys fall into a range set.
 *
 * Objects of this type provide a lightweight wrapper around `std::map`, which
 * provides a iterator, which will skip over unwanted elements.
 *
 * This class is not very generic. It should be thought of as a crude way of
 * deduplicating code.
 *
 * The unfiltered elements' keys should fall into a given range set - either
 * `StringRangeSet` or by `TimestampRangeSet`.
 *
 * @tparam Map the type of the map to be wrapped, an instantiation of `std::map`
 * @tparam PermittedRanges the type of the filter, either `StringRangeSet` or
 *   `TimestampRangeSet`
 */
template <typename Map, typename PermittedRanges>
class RangeFilteredMapView {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  class const_iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type =
        typename std::iterator_traits<typename Map::const_iterator>::value_type;
    using difference_type = typename std::iterator_traits<
        typename Map::const_iterator>::difference_type;
    using reference = value_type const&;
    using pointer = value_type const*;

    const_iterator(
        RangeFilteredMapView const& parent,
        typename Map::const_iterator unfiltered_pos,
        typename std::set<typename PermittedRanges::Range,
                          typename PermittedRanges::Range::StartLess>::
            const_iterator filter_pos)
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
    pointer operator->() const { return &*unfiltered_pos_; }

   private:
    // Adjust `unfiltered_pos_` after we transition to a different range.
    void AdvanceToNextRange() {
      if (filter_pos_ == parent_.get().filter_.get().disjoint_ranges().end()) {
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

      if (filter_pos_->start_closed()) {
        unfiltered_pos_ = parent_.get().unfiltered_.get().lower_bound(
            filter_pos_->start_finite());
      } else {
        unfiltered_pos_ = parent_.get().unfiltered_.get().upper_bound(
            filter_pos_->start_finite());
      }
    }

    // After `unfiltered_pos_` was increased, make sure it's within a valid
    // range.
    void EnsureIteratorValid() {
      // `unfiltered_pos_` may point to a row which is past the end of the range
      // pointed by filter_pos_. Make sure this only happens when the iteration
      // reaches its end.
      while (unfiltered_pos_ != parent_.get().unfiltered_.get().end() &&
             filter_pos_ !=
                 parent_.get().filter_.get().disjoint_ranges().end() &&
             filter_pos_->IsAboveEnd(unfiltered_pos_->first)) {
        ++filter_pos_;
        AdvanceToNextRange();
      }
    }

    std::reference_wrapper<RangeFilteredMapView const> parent_;
    typename Map::const_iterator unfiltered_pos_;
    typename std::set<
        typename PermittedRanges::Range,
        typename PermittedRanges::Range::StartLess>::const_iterator filter_pos_;
  };

  /**
   * Create a new object.
   *
   * Objects of this class store references to arguments passed in the
   * constructor. The user is responsible for making sure that the referenced
   * objects continue to exist throughout the lifetime of this object. They
   * should also not change.
   *
   * @unfiltered the map whose elements need to be filtered.
   * @filter the range set which dictates which ranges should remain unfiltered.
   */
  RangeFilteredMapView(Map const& unfiltered, PermittedRanges const& filter)
      : unfiltered_(std::cref(unfiltered)), filter_(std::cref(filter)) {}

  const_iterator begin() const {
    return const_iterator(*this, unfiltered_.get().begin(),
                          filter_.get().disjoint_ranges().begin());
  }
  const_iterator end() const {
    return const_iterator(*this, unfiltered_.get().end(),
                          filter_.get().disjoint_ranges().end());
  }

 private:
  std::reference_wrapper<Map const> unfiltered_;
  std::reference_wrapper<PermittedRanges const> filter_;
};

/**
 * A map view filtering elements by whether their keys match a regex.
 *
 * Objects of this type provide a lightweight wrapper around `std::map`, which
 * provides a iterator, which will skip over unwanted elements.
 *
 * This class is not very generic. It should be thought of as a crude way of
 * deduplicating code.
 *
 * Elements whose keys match all regexes are not filtered out.
 *
 * @tparam Map the type of the map to be wrapped, an instantiation of `std::map`
 */
template <typename Map>
class RegexFiteredMapView {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  class const_iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type =
        typename std::iterator_traits<typename Map::const_iterator>::value_type;
    using difference_type = typename std::iterator_traits<
        typename Map::const_iterator>::difference_type;
    using reference = value_type const&;
    using pointer = value_type const*;

    const_iterator(RegexFiteredMapView const& parent,
                   typename Map::const_iterator unfiltered_pos)
        : parent_(std::cref(parent)),
          unfiltered_pos_(std::move(unfiltered_pos)) {
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
    pointer operator->() const { return &*unfiltered_pos_; }

   private:
    // Make sure that `unfiltered_pos_` points to an unfiltered elem or end().
    void EnsureIteratorValid() {
      for (; unfiltered_pos_ != parent_.get().unfiltered_.end() &&
             std::any_of(parent_.get().filters_.get().begin(),
                         parent_.get().filters_.get().end(),
                         [&](std::shared_ptr<re2::RE2 const> const& filter) {
                           return !re2::RE2::PartialMatch(
                               unfiltered_pos_->first, *filter);
                         });
           ++unfiltered_pos_) {
      }
    }

    std::reference_wrapper<RegexFiteredMapView const> parent_;
    typename Map::const_iterator unfiltered_pos_;
  };

  /**
   * Create a new object.
   *
   * Objects of this class store references to arguments passed in the
   * constructor. The user is responsible for making sure that the referenced
   * objects continue to exist throughout the lifetime of this object. They
   * should also not change.
   *
   * @unfiltered the map whose elements need to be filtered.
   * @filters the regexes which element's keys have match to not be filtered
   *   out.
   */
  RegexFiteredMapView(
      Map unfiltered,
      std::vector<std::shared_ptr<re2::RE2 const>> const& filters)
      : unfiltered_(std::move(unfiltered)), filters_(std::cref(filters)) {}

  const_iterator begin() const {
    return const_iterator(*this, unfiltered_.begin());
  }
  const_iterator end() const {
    return const_iterator(*this, unfiltered_.end());
  }

 private:
  Map unfiltered_;
  std::reference_wrapper<std::vector<std::shared_ptr<re2::RE2 const>> const>
      filters_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_FILTERED_MAP_H
