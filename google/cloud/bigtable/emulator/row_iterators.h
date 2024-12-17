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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_ROW_ITERATORS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_ROW_ITERATORS_H

#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/bigtable/emulator/sorted_row_set.h"
#include <list>
#include <functional>
#include <queue>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

template <typename Iterator, typename IteratorLess>
class MergedSortedIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = typename std::iterator_traits<Iterator>::value_type;
  using difference_type = std::size_t;
  using reference = value_type&;
  using pointer = value_type*;
  using const_reference = value_type const&;
  using const_pointer = value_type const*;

  // end() iterator.
  MergedSortedIterator() = default;
  MergedSortedIterator(MergedSortedIterator const& other) = default;
  MergedSortedIterator(MergedSortedIterator&& other) = default;

  MergedSortedIterator(std::vector<std::pair<Iterator, Iterator>> ranges) {
    for (auto & range : ranges) {
      if (range.first != range.second) {
        ranges_.emplace(std::move(range));
      }
    }
  }

  value_type operator*() const {
    return *ranges_.top().first;
  }

  MergedSortedIterator& operator++() {
    auto prev_top = ranges_.top();;
    // We need to remove it from the priority queue because we're likely to
    // change the order.
    ranges_.pop();

    ++prev_top.first;

    if (prev_top.first != prev_top.second) {
      ranges_.emplace(std::move(prev_top));
    }

    return *this;
  }

  bool operator==(MergedSortedIterator const& other) const {
    if (ranges_.empty() || other.ranges_.empty()) {
      return ranges_.empty() == other.ranges_.empty();
    }
    return ranges_.top() == other.ranges_.top();
  }

  bool operator!=(MergedSortedIterator const& other) const {
    return !(*this == other);
  }

 private:
  struct InternalGreater {
    bool operator()(std::pair<Iterator, Iterator> const &lhs,
                    std::pair<Iterator, Iterator> const &rhs) const {
      return IteratorLess()(*rhs.first, *lhs.first);
    }
  };

  std::priority_queue<std::pair<Iterator, Iterator>,
                      std::vector<std::pair<Iterator, Iterator>>,
                      InternalGreater>
      ranges_;
};

template <typename OuterIterator, typename DescendFunctor,
          typename ValueCombineFunctor>
class FlattenedIterator {
 public:
   using InnerCollection = typename std::result_of<DescendFunctor(
      typename std::iterator_traits<OuterIterator>::value_type const&)>::type;
  using InnerIterator = typename std::decay_t<InnerCollection>::iterator;
  using iterator_category = std::input_iterator_tag;
  using value_type = typename std::result_of<ValueCombineFunctor(
      typename std::iterator_traits<OuterIterator>::value_type const&,
      typename std::iterator_traits<InnerIterator>::value_type const&)>::type;
  using difference_type =
      typename std::iterator_traits<OuterIterator>::difference_type;
  using pointer = value_type const*;
  using reference = value_type const&;

  FlattenedIterator(OuterIterator begin, OuterIterator end)
      : outer_pos_(std::move(begin)), outer_end_(std::move(end)) {
    if (outer_pos_ != outer_end_) {
      inner_pos_ = DescendFunctor()(*outer_pos_).begin();
      EnsureIteratorValid();
    }
  }

  value_type operator*() const {
    assert(inner_pos_ != DescendFunctor()(*outer_pos_).end());
    return ValueCombineFunctor()(*outer_pos_, *inner_pos_);
  }

  FlattenedIterator& operator++() {
    ++inner_pos_;
    EnsureIteratorValid();
    return *this;
  }

  FlattenedIterator operator++(int) {
    FlattenedIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(FlattenedIterator const& other) const {
    return outer_pos_ == other.outer_pos_ &&
           (outer_pos_ == outer_end_ || inner_pos_ == other.inner_pos_);
  }

  bool operator!=(FlattenedIterator const& other) const {
    return !(*this == other);
  }

 private:
  OuterIterator outer_pos_;
  OuterIterator outer_end_;
  InnerIterator inner_pos_;

  void EnsureIteratorValid() {
    while (outer_pos_ != outer_end_ &&
           inner_pos_ == DescendFunctor()(*outer_pos_).end()) {
      ++outer_pos_;
      if (outer_pos_ != outer_end_) {
        inner_pos_ = DescendFunctor()(*outer_pos_).begin();
      }
    }
  }
};

template <typename InputIterator, typename Functor>
class TransformIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = typename std::result_of<Functor(
      typename std::iterator_traits<InputIterator>::value_type)>::type;
  using difference_type =
      typename std::iterator_traits<InputIterator>::difference_type;
  using pointer = value_type*;
  using reference = value_type&;

  TransformIterator(InputIterator it, Functor func)
      : current(std::move(it)), transformer(std::move(func)) {}

  value_type operator*() const { return transformer(*current); }

  TransformIterator& operator++() {
    ++current;
    return *this;
  }

  TransformIterator operator++(int) {
    TransformIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(TransformIterator const& other) const {
    return current == other.current;
  }

  bool operator!=(TransformIterator const& other) const {
    return current != other.current;
  }

 private:
  InputIterator current;
  Functor transformer;
};

// Helper function to create a TransformIterator
template <typename InputIterator, typename Functor>
std::pair<TransformIterator<InputIterator, Functor>,
          TransformIterator<InputIterator, Functor>>
TransformIteratorRange(InputIterator begin, InputIterator end, Functor func) {
  Functor func_copy(func);  // avoid two copies
  return std::make_pair(TransformIterator<InputIterator, Functor>(
                            std::move(begin), std::move(func)),
                        TransformIterator<InputIterator, Functor>(
                            std::move(end), std::move(func_copy)));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_ROW_ITERATORS_H
