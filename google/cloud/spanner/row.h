// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_H

#include "google/cloud/spanner/internal/tuple_utils.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class Row;
namespace internal {
Row MakeRow(std::vector<Value>,
            std::shared_ptr<const std::vector<std::string>>);
}  // namespace internal

/**
 * A `Row` is a sequence of columns each with a name and an associated `Value`.
 *
 * The `Row` class is a regular value type that may be copied, moved, assigned,
 * compared for equality, etc. Instances may be large if they hold lots of
 * `Value` data, so copy only when necessary.
 *
 * `Row` instances are typically returned as the result of queries or reads of
 * a Cloud Spanner table (see `Client::Read` and `Client::ExecuteQuery`). Users
 * will mostly just use the accessor methods on `Row, and will rarely (if ever)
 * need to construct a `Row` of their own.
 *
 * The number of columns in a `Row` can be obtained from the `size()` member
 * function. The `Value`s can be obtained using the `values()` accessor. The
 * names of each column in the row can be obtained using the `columns()`
 * accessor.
 *
 * Perhaps the most convenient way to access the `Values` in a row is through
 * the variety of "get" accessors. A user may access a column's `Value' by
 * calling `get` with a `std::size_t` 0-indexed position, or a `std::string`
 * column name. Furthermore, callers may directly extract the native C++ type
 * by specifying the C++ type along with the column's position or name.
 *
 * @par Example
 *
 * @code
 * Row row = ...;
 * if (StatusOr<std::string> x = row.get<std::string>("LastName")) {
 *   std::cout << "LastName=" << *x << "\n";
 * }
 * @endcode
 *
 * @note There is a helper function defined below named `MakeTestRow()` to make
 *     creating `Row` instances for testing easier.
 */
class Row {
 public:
  /// Default constructs an empty row with no columns nor values.
  Row();

  /// @name Copy and move.
  ///@{
  Row(Row const&) = default;
  Row& operator=(Row const&) = default;
  Row(Row&&) = default;
  Row& operator=(Row&&) = default;
  ///@}

  /// Returns the number of columns in the row.
  std::size_t size() const { return columns_->size(); }

  /// Returns the column names for the row.
  std::vector<std::string> const& columns() const { return *columns_; }

  /// Returns the `Value` objects in the given row.
  std::vector<Value> const& values() const& { return values_; }

  /// Returns the `Value` objects in the given row.
  std::vector<Value>&& values() && { return std::move(values_); }

  /// Returns the `Value` at the given @p pos.
  StatusOr<Value> get(std::size_t pos) const;

  /// Returns the `Value` in the column with @p name
  StatusOr<Value> get(std::string const& name) const;

  /**
   * Returns the native C++ value at the given position or column name.
   *
   * @tparam T the native C++ type, e.g., std::int64_t or std::string
   * @tparam Arg a deduced parameter convertible to a std::size_t or std::string
   */
  template <typename T, typename Arg>
  StatusOr<T> get(Arg&& arg) const {
    auto v = get(std::forward<Arg>(arg));
    if (v) return v->template get<T>();
    return v.status();
  }

  /**
   * Returns all the native C++ values for the whole row in a `std::tuple` with
   * the specified type.
   *
   * @tparam Tuple the `std::tuple` type that the whole row must unpack into.
   */
  template <typename Tuple>
  StatusOr<Tuple> get() const& {
    if (size() != std::tuple_size<Tuple>::value) {
      auto const msg = "Tuple has the wrong number of elements";
      return Status(StatusCode::kInvalidArgument, msg);
    }
    Tuple tup;
    auto it = values_.begin();
    Status status;
    internal::ForEach(tup, ExtractValue{status}, it);
    if (!status.ok()) return status;
    return tup;
  }

  /**
   * Returns all the native C++ values for the whole row in a `std::tuple` with
   * the specified type.
   *
   * @tparam Tuple the `std::tuple` type that the whole row must unpack into.
   */
  template <typename Tuple>
  StatusOr<Tuple> get() && {
    if (size() != std::tuple_size<Tuple>::value) {
      auto const msg = "Tuple has the wrong number of elements";
      return Status(StatusCode::kInvalidArgument, msg);
    }
    Tuple tup;
    auto it = std::make_move_iterator(values_.begin());
    Status status;
    internal::ForEach(tup, ExtractValue{status}, it);
    if (!status.ok()) return status;
    return tup;
  }

  /// @name Equality
  ///@{
  friend bool operator==(Row const& a, Row const& b);
  friend bool operator!=(Row const& a, Row const& b) { return !(a == b); }
  ///@}

 private:
  friend Row internal::MakeRow(std::vector<Value>,
                               std::shared_ptr<const std::vector<std::string>>);
  struct ExtractValue {
    Status& status;
    template <typename T, typename It>
    void operator()(T& t, It& it) const {
      auto x = it++->template get<T>();
      if (!x) {
        status = std::move(x).status();
      } else {
        t = *std::move(x);
      }
    }
  };

  /**
   * Constructs a `Row` with the given @p values and @p columns.
   *
   * @note columns must not be nullptr
   * @note columns.size() must equal values.size()
   */
  Row(std::vector<Value> values,
      std::shared_ptr<const std::vector<std::string>> columns);

  std::vector<Value> values_;
  std::shared_ptr<const std::vector<std::string>> columns_;
};

/**
 * Creates a `Row` with the specified column names and values.
 *
 * This overload accepts a vector of pairs, allowing the caller to specify both
 * the column names and the `Value` that goes in each column.
 *
 * This function is intended for application developers who are mocking the
 * results of a `Client::ExecuteQuery` call.
 */
Row MakeTestRow(std::vector<std::pair<std::string, Value>> pairs);

/**
 * Creates a `Row` with `Value`s created from the given arguments and with
 * auto-generated column names.
 *
 * This overload accepts a variadic list of arguments that will be used to
 * create the `Value`s in the row. The column names will be implicitly
 * generated, the first column being "0", the second "1", and so on,
 * corresponding to the argument's position.
 *
 * This function is intended for application developers who are mocking the
 * results of a `Client::ExecuteQuery` call.
 */
template <typename... Ts>
Row MakeTestRow(Ts&&... ts) {
  auto columns = std::make_shared<std::vector<std::string>>();
  for (std::size_t i = 0; i < sizeof...(ts); ++i) {
    columns->emplace_back(std::to_string(i));
  }
  std::vector<Value> v{Value(std::forward<Ts>(ts))...};
  return internal::MakeRow(std::move(v), std::move(columns));
}

/**
 * A `RowStreamIterator` is an _Input Iterator_ (see below) that returns a
 * sequence of `StatusOr<Row>` objects.
 *
 * As an [Input Iterator][input-iterator], the sequence may only be consumed
 * once. Default constructing a `RowStreamIterator` creates an instance that
 * represents "end".
 *
 * @note The term "stream" in this name refers to the general nature
 *     of the the data source, and is not intended to suggest any similarity to
 *     C++'s I/O streams library. Syntactically, this class is an "iterator".
 *
 * [input-iterator]: https://en.cppreference.com/w/cpp/named_req/InputIterator
 */
class RowStreamIterator {
 public:
  /**
   * A function that returns a sequence of `StatusOr<Row>` objects. Returning
   * an empty `Row` indicates that there are no more rows to be returned.
   */
  using Source = std::function<StatusOr<Row>()>;

  /// @name Iterator type aliases
  ///@{
  using iterator_category = std::input_iterator_tag;
  using value_type = StatusOr<Row>;
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
  value_type row_;
  Source source_;  // nullptr means "end"
};

/**
 * A `TupleStreamIterator<Tuple>` is an "Input Iterator" that wraps a
 * `RowStreamIterator`, parsing its elements into a sequence of
 * `StatusOr<Tuple>` objects.
 *
 * As an Input Iterator, the sequence may only be consumed once. See
 * https://en.cppreference.com/w/cpp/named_req/InputIterator for more details.
 *
 * Default constructing this object creates an instance that represents "end".
 *
 * Each `Row` returned by the wrapped `RowStreamIterator` must be convertible
 * to the specified `Tuple` template parameter.
 *
 * @note The term "stream" in this name refers to the general nature
 *     of the the data source, and is not intended to suggest any similarity to
 *     C++'s I/O streams library. Syntactically, this class is an "iterator".
 *
 * @tparam Tuple the std::tuple<...> to parse each `Row` into.
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
    if (!tup_) {
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
  }

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
 *     of the the data source, and is not intended to suggest any similarity to
 *     C++'s I/O streams library. Syntactically, this class is a "range"
 *     defined by two "iterator" objects of type `TupleStreamIterator<Tuple>`.
 *
 * @tparam Tuple the std::tuple<...> to parse each `Row` into.
 */
template <typename Tuple>
class TupleStream {
 public:
  using iterator = TupleStreamIterator<Tuple>;
  static_assert(internal::IsTuple<Tuple>::value,
                "TupleStream<T> requires a std::tuple parameter");

  iterator begin() const { return begin_; }
  iterator end() const { return end_; }

 private:
  template <typename T, typename RowRange>
  friend TupleStream<T> StreamOf(RowRange&& range);

  template <typename It>
  explicit TupleStream(It&& start, It&& end)
      : begin_(std::forward<It>(start), std::forward<It>(end)) {}

  iterator begin_;
  iterator end_;
};

/**
 * A factory that creates a `TupleStream<Tuple>` by wrapping the given @p
 * range. The `RowRange` must be a range defined by `RowStreamIterator`
 * objects.
 *
 * @snippet samples.cc stream-of
 *
 * @note ownership of the @p range is not transferred, so it must outlive the
 *     returned `TupleStream`.
 *
 * @tparam RowRange must be a range defined by `RowStreamIterator`s.
 */
template <typename Tuple, typename RowRange>
TupleStream<Tuple> StreamOf(RowRange&& range) {
  static_assert(std::is_lvalue_reference<decltype(range)>::value,
                "range must be an lvalue since it must outlive StreamOf");
  return TupleStream<Tuple>(std::begin(std::forward<RowRange>(range)),
                            std::end(std::forward<RowRange>(range)));
}

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
 * @snippet samples.cc get-singular-row
 *
 * @warning Due to the fact that a `RowStreamIteartor` is an input iterator,
 *     this function may consume the first element in the range, even in cases
 *     where an error is returned. But again, this function should not be used
 *     if @p range might contain multiple rows.
 */
template <typename RowRange>
auto GetSingularRow(RowRange&& range) -> typename std::decay<
    decltype(*std::forward<RowRange>(range).begin())>::type {
  auto const e = std::forward<RowRange>(range).end();
  auto it = std::forward<RowRange>(range).begin();
  if (it == e) return Status(StatusCode::kInvalidArgument, "no rows");
  auto row = std::move(*it);
  if (++it != e) return Status(StatusCode::kInvalidArgument, "too many rows");
  return row;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_H
