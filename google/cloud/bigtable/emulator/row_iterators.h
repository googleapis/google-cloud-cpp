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

#include <new>
#include "google/cloud/bigtable/emulator/cell_view.h"
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
  using difference_type =
      typename std::iterator_traits<Iterator>::difference_type;
  using reference = typename std::iterator_traits<Iterator>::reference;
  using pointer = typename std::iterator_traits<Iterator>::pointer;

  MergedSortedIterator() = default;  // end()
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

  pointer operator->() const {
    return ranges_.top().first;
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
  using InnerCollection = std::decay_t<typename std::result_of<DescendFunctor(
      typename std::iterator_traits<OuterIterator>::reference)>::type>;
  using InnerIterator = typename InnerCollection::const_iterator;

  using iterator_category = std::input_iterator_tag;
  using value_type = std::decay_t<typename std::result_of<ValueCombineFunctor(
      typename std::iterator_traits<OuterIterator>::reference,
      typename std::iterator_traits<InnerIterator>::reference)>::type> const;
  using difference_type =
      typename std::iterator_traits<OuterIterator>::difference_type;
  using pointer = value_type*;
  using reference = value_type&;

  FlattenedIterator(OuterIterator begin, OuterIterator end)
      : outer_pos_(std::move(begin)), outer_end_(std::move(end)) {
    if (outer_pos_ != outer_end_) {
      inner_pos_ = DescendFunctor()(*outer_pos_).begin();
      EnsureIteratorValid();
    }
  }

  value_type operator*() const {
    assert(inner_pos_ != DescendFunctor()(*outer_pos_).end());
    return GetCachedValue();
  }

  pointer operator->() const {
    assert(inner_pos_ != DescendFunctor()(*outer_pos_).end());
    return &GetCachedValue(); 
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
  mutable absl::optional<std::decay_t<value_type>> cached_value_;

  void EnsureIteratorValid() {
    cached_value_.reset();
    while (outer_pos_ != outer_end_ &&
           inner_pos_ == DescendFunctor()(*outer_pos_).end()) {
      ++outer_pos_;
      if (outer_pos_ != outer_end_) {
        inner_pos_ = DescendFunctor()(*outer_pos_).begin();
      }
    }
  }

  reference GetCachedValue() const {
    if (!cached_value_) {
      cached_value_.emplace(ValueCombineFunctor()(*outer_pos_, *inner_pos_));
    }
    return *cached_value_;
  }
};

template <typename InputIterator, typename Functor>
class TransformIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = std::decay_t<typename std::result_of<Functor(
      typename std::iterator_traits<InputIterator>::value_type)>::type> const;
  using difference_type =
      typename std::iterator_traits<InputIterator>::difference_type;
  using pointer = value_type*;
  using reference = value_type&;

  TransformIterator(InputIterator it, Functor func)
      : current_(std::move(it)), transformer_(std::move(func)) {}
  TransformIterator(TransformIterator const& other) = default;
  TransformIterator(TransformIterator&& other) = default;

  value_type operator*() const { return GetCachedValue(); }
  pointer operator->() const { return &GetCachedValue(); }

  TransformIterator& operator++() {
    cached_value_.reset();
    ++current_;
    return *this;
  }

  TransformIterator operator++(int) {
    TransformIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(TransformIterator const& other) const {
    return current_ == other.current_;
  }

  bool operator!=(TransformIterator const& other) const {
    return current_ != other.current_;
  }

  TransformIterator &operator=(TransformIterator const &other) {
    if (this == &other) {
      return *this;
    }
    current_ = other.current_;
    transformer_ = other.transformer_;
    if (other.cached_value_) {
      cached_value_.emplace(other.cached_value_.get());
    } else {
      cached_value_.reset();
    }
    return this;
  }

  TransformIterator &operator=(TransformIterator &&other) {
    if (this == &other) {
      return *this;
    }
    current_ = std::move(other.current_);
    transformer_ = std::move(other.transformer_);
    if (other.cached_value_) {
      cached_value_.emplace(*std::move(other.cached_value_));
    } else {
      cached_value_.reset();
    }
    return *this;
  }

  reference GetCachedValue() const {
    if (!cached_value_) {
      cached_value_.emplace(transformer_(*current_));
    }
    return *cached_value_;
  }

 private:
  InputIterator current_;
  Functor transformer_;
  mutable absl::optional<std::decay_t<value_type>> cached_value_;
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
