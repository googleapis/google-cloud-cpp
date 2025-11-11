// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_STREAM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_STREAM_H

#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/bigtable/result_source_interface.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `RowStreamIterator` is an _Input Iterator_ (see below) that returns a
 * sequence of `StatusOr<QueryRow>` objects.
 *
 * As an [Input Iterator][input-iterator], the sequence may only be consumed
 * once. Default constructing a `RowStreamIterator` creates an instance
 * that represents "end".
 *
 * @note The term "stream" in this name refers to the general nature
 *     of the data source, and is not intended to suggest any similarity to
 *     C++'s I/O streams library. Syntactically, this class is an "iterator".
 *
 * [input-iterator]: https://en.cppreference.com/w/cpp/named_req/InputIterator
 */
class RowStreamIterator {
 public:
  /**
   * A function that returns a sequence of `StatusOr<QueryRow>` objects.
   * Returning an empty `QueryRow` indicates that there are no more rows to be
   * returned.
   */
  using Source = std::function<StatusOr<QueryRow>()>;

  /// @name Iterator type aliases
  ///@{
  using iterator_category = std::input_iterator_tag;
  using value_type = StatusOr<QueryRow>;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  using const_pointer = value_type const*;
  using const_reference = value_type const&;
  ///@}

  /// Default constructs an "end" iterator.
  RowStreamIterator();

  /**
   * Constructs a `RowStreamIterator` that will consume rows from the given
   * @p source, which must not be `nullptr`.
   */
  explicit RowStreamIterator(Source source);

  reference operator*() { return row_; }
  pointer operator->() { return &row_; }

  const_reference operator*() const { return row_; }
  const_pointer operator->() const { return &row_; }

  RowStreamIterator& operator++();
  RowStreamIterator operator++(int);

  friend bool operator==(RowStreamIterator const&, RowStreamIterator const&);
  friend bool operator!=(RowStreamIterator const&, RowStreamIterator const&);

 private:
  bool row_ok_{true};
  value_type row_{QueryRow{}};
  Source source_;  // nullptr means "end"
};

/**
 * Represents the stream of `QueryRows` returned from
 * `bigtable::Client::ExecuteQuery`.
 *
 */
class RowStream {
 public:
  RowStream() = default;
  explicit RowStream(std::unique_ptr<ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  RowStream(RowStream&&) = default;
  RowStream& operator=(RowStream&&) = default;

  /// Returns a `RowStreamIterator` defining the beginning of this range.
  RowStreamIterator begin() {
    return RowStreamIterator([this]() mutable { return source_->NextRow(); });
  }

  /// Returns a `RowStreamIterator` defining the end of this range.
  static RowStreamIterator end() { return {}; }

 private:
  std::unique_ptr<ResultSourceInterface> source_;
};

/**
 * Returns the only row from a range that contains exactly one row.
 *
 * An error is returned if the given range does not contain exactly one row.
 * This is a convenience function that may be useful when the caller knows that
 * a range should contain exactly one row, such as when `LIMIT 1` is used in an
 * SQL query, or when a read is performed on a guaranteed unique key such that
 * only a single row could possibly match. In cases where the caller does not
 * know how many rows may be returned, they should instead consume the range in
 * a loop.
 *
 */
template <typename QueryRowRange>
auto GetSingularRow(QueryRowRange range)
    -> std::decay_t<decltype(*range.begin())> {
  auto const e = range.end();
  auto it = range.begin();
  if (it == e) {
    return google::cloud::internal::InvalidArgumentError("no rows",
                                                         GCP_ERROR_INFO());
  }
  auto row = std::move(*it);
  if (++it != e) {
    return google::cloud::internal::InvalidArgumentError("too many rows",
                                                         GCP_ERROR_INFO());
  }
  return row;
}

/**
 * A `TupleStreamIterator<Tuple>` is an "Input Iterator" that wraps a
 * `RowStreamIterator`,
 * parsing its elements into a sequence of
 * `StatusOr<Tuple>` objects.
 *
 * As an Input Iterator, the sequence may only be consumed once. See
 * https://en.cppreference.com/w/cpp/named_req/InputIterator for more details.
 *
 * Default constructing this object creates an instance that represents "end".
 *
 * Each `QueryRow` returned by the wrapped `RowStreamIterator` must be
 * convertible to the specified `Tuple` template parameter.
 *
 * @note The term "stream" in this name refers to the general nature
 *     of the data source, and is not intended to suggest any similarity to
 *     C++'s I/O streams library. Syntactically, this class is an "iterator".
 *
 * @tparam Tuple the std::tuple<...> to parse each `QueryRow` into.
 */
template <typename Tuple>
class TupleStreamIterator {
 public:
  /// @name Iterator type aliases
  ///@{
  using iterator_category = std::input_iterator_tag;
  using value_type = StatusOr<Tuple>;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  using const_pointer = value_type const*;
  using const_reference = value_type const&;
  ///@}

  /// Default constructs an "end" iterator.
  TupleStreamIterator() = default;

  /// Creates an iterator that wraps the given `RowStreamIterator` range.
  TupleStreamIterator(RowStreamIterator begin, RowStreamIterator end)
      : it_(std::move(begin)), end_(std::move(end)) {
    ParseTuple();
  }

  reference operator*() { return tup_; }
  pointer operator->() { return &tup_; }

  const_reference operator*() const { return tup_; }
  const_pointer operator->() const { return &tup_; }

  TupleStreamIterator& operator++() {
    if (!tup_ok_) {
      it_ = end_;
      return *this;
    }
    ++it_;
    ParseTuple();
    return *this;
  }

  TupleStreamIterator operator++(int) {
    auto const old = *this;
    ++*this;
    return old;
  }

  friend bool operator==(TupleStreamIterator const& a,
                         TupleStreamIterator const& b) {
    return a.it_ == b.it_;
  }

  friend bool operator!=(TupleStreamIterator const& a,
                         TupleStreamIterator const& b) {
    return !(a == b);
  }

 private:
  void ParseTuple() {
    if (it_ == end_) return;
    tup_ = *it_ ? std::move(*it_)->template get<Tuple>() : it_->status();
    tup_ok_ = tup_.ok();
  }

  bool tup_ok_{false};
  value_type tup_;
  RowStreamIterator it_;
  RowStreamIterator end_;
};

/**
 * A `TupleStream<Tuple>` defines a range that parses `Tuple` objects from the
 * given range of `RowStreamIterator`s.
 *
 * Users create instances using the `StreamOf<T>(range)` non-member factory
 * function (defined below). The following is a typical usage of this class in
 * a range-for loop.
 *
 * @code
 * auto row_range = ...
 * using RowType = std::tuple<std::int64_t, std::string, bool>;
 * for (auto const& row : StreamOf<RowType>(row_range)) {
 *   if (!row) {
 *     // Handle error;
 *   }
 *   std::int64_t x = std::get<0>(*row);
 *   ...
 * }
 * @endcode
 *
 * @note The term "stream" in this name refers to the general nature
 *     of the data source, and is not intended to suggest any similarity to
 *     C++'s I/O streams library. Syntactically, this class is a "range"
 *     defined by two "iterator" objects of type `TupleStreamIterator<Tuple>`.
 *
 * @tparam Tuple the std::tuple<...> to parse each `QueryRow` into.
 */
template <typename Tuple>
class TupleStream {
 public:
  using iterator = TupleStreamIterator<Tuple>;
  static_assert(bigtable_internal::IsTuple<Tuple>::value,
                "TupleStream<T> requires a std::tuple parameter");

  iterator begin() const { return begin_; }
  iterator end() const { return end_; }

 private:
  template <typename T, typename QueryRowRange>
  friend TupleStream<T> StreamOf(QueryRowRange&& range);

  template <typename It>
  explicit TupleStream(It&& start, It&& end)
      : begin_(std::forward<It>(start), std::forward<It>(end)) {}

  iterator begin_;
  iterator end_;
};

/**
 * A factory that creates a `TupleStream<Tuple>` by wrapping the given @p
 * range. The `QueryRowRange` must be a range defined by
 * `RowStreamIterator` objects.
 *
 *
 * @note Ownership of the @p range is not transferred, so it must outlive the
 *     returned `TupleStream`.
 *
 * @tparam QueryRowRange must be a range defined by `RowStreamIterator`s.
 */
template <typename Tuple, typename QueryRowRange>
TupleStream<Tuple> StreamOf(QueryRowRange&& range) {
  static_assert(std::is_lvalue_reference<decltype(range)>::value,
                "range must be an lvalue since it must outlive StreamOf");
  return TupleStream<Tuple>(std::begin(std::forward<QueryRowRange>(range)),
                            std::end(std::forward<QueryRowRange>(range)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_STREAM_H
