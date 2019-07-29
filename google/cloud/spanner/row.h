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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_H_

#include "google/cloud/spanner/internal/tuple_utils.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/disjunction.h"
#include "google/cloud/status_or.h"
#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * The `Row<Ts...>` class template represents a heterogeneous set of C++ values.
 *
 * The values stored in a row may have any of the valid Spanner types that can
 * be stored in `Value`. Each value in a row is identified by its column index,
 * with the first value corresponding to column 0. The number of columns in the
 * row is given by the `Row::size()` member function (This exactly matches the
 * number of types the `Row` was created with).
 *
 * A `Row<Ts...>` is a regular C++ type, supporting default construction, copy,
 * assignment, move, and equality as expected. It should typically be
 * constructed with values using the non-member `spanner::MakeRow(Ts...)`
 * function template (see below). Once an instance is created, you can access
 * the values in the row using the `Row::get()` overloaded functions.
 *
 * @par Example
 *
 * @code
 * spanner::Row<bool, std::int64_t, std::string> row;
 * row = spanner::MakeRow(true, 42, "hello");
 * static_assert(row.size() == 3, "Created with three types");
 *
 * // Gets all values as a std::tuple
 * std::tuple<bool, std::int64_t, std::string> tup = row.get();
 *
 * // Gets one value at the specified column index
 * std::int64_t i = row.get<1>();
 *
 * // Gets the values from the specified columns (any order), and returns a
 * // tuple. Since C++17, this tuple will work with structured bindings to
 * // allow assigning to individually  named variables.
 * auto [name, age] = row.get<2, 1>();
 * @endcode
 *
 * @tparam Types... The C++ types for each column in the row.
 */
template <typename... Types>
class Row {
  template <std::size_t I>
  using ColumnType = typename std::tuple_element<I, std::tuple<Types...>>::type;

 public:
  /// Returns the number of columns in this row.
  static constexpr std::size_t size() { return sizeof...(Types); }

  // Regular value type, supporting copy, assign, move, etc.
  Row() {}
  Row(Row const&) = default;
  Row& operator=(Row const&) = default;
  Row(Row&&) = default;
  Row& operator=(Row&&) = default;

  /**
   * Constructs a Row from the specified values.
   *
   * See also the `MakeRow()` helper function template for a convenient way to
   * make rows without having to also specify the value's types.
   *
   * This constructor is disabled if any of the specified types is `Row`, which
   * prevents this constructor from taking over copy construction.
   */
  template <typename... Ts,
            typename std::enable_if<
                !google::cloud::internal::disjunction<
                    std::is_same<typename std::decay<Ts>::type, Row>...>::value,
                int>::type = 0>
  explicit Row(Ts&&... ts) : values_(std::forward<Ts>(ts)...) {}

  /**
   * Returns a reference to the value at position `I`.
   *
   * The value category of the returned reference matches the value category
   * of `this`. That is, calling `get<I>()` on a non-const lvalue returns a
   * non-const lvalue. Calling `get<I>()` on an rvalue returns an rvalue, etc.
   *
   * @par Example
   *
   * @code
   * auto row = MakeRow(true, "foo");
   * assert(row.get<0>() == true);
   * assert(row.get<1>() == "foo");
   *
   * Row<bool, std::string> F();
   * std::string x = F().get<1>();
   * @endcode
   */
  template <std::size_t I>
  ColumnType<I>& get() & {
    return std::get<I>(values_);
  }
  template <std::size_t I>
  ColumnType<I> const& get() const& {
    return std::get<I>(values_);
  }
  template <std::size_t I>
  ColumnType<I>&& get() && {
    return std::get<I>(std::move(values_));
  }
  template <std::size_t I>
  ColumnType<I> const&& get() const&& {
    return std::get<I>(std::move(values_));
  }

  /**
   * Returns a std::tuple of the values at the specified positions (specified
   * in any order).
   *
   * This overload is only available if 2 or more index template arguments are
   * specified, thus the result is returned as a std::tuple.
   *
   * @par Example
   *
   * @code
   * auto row = MakeRow(true, "foo", 42);
   * std::tuple<std::int64_t, bool> tup = row.get<2, 0>();
   * assert(std::get<0>(tup) == 42);
   * assert(std::get<1>(tup) == true);
   * @endcode
   *
   * @tparam Is... a list of indexes to be returned in a `std::tuple`
   */
  template <std::size_t... Is,
            typename std::enable_if<(sizeof...(Is) > 1), int>::type = 0>
  std::tuple<ColumnType<Is>...> get() const {
    return std::make_tuple(get<Is>()...);
  }

  /**
   * Returns a reference to the const std::tuple containing all the values in
   * the row.
   *
   * This function is const/non-const x lvalue/rvalue overloaded. This enables
   * a caller to move the returned tuple out of the Row.
   *
   * @par Example
   *
   * @code
   * auto row = MakeRow(true, "foo", 42);
   * std::tuple<bool, std::string, std::int64_t> tup
   *     = row.get();
   * assert(std::get<0>(tup) == true);
   * assert(std::get<1>(tup) == "foo");
   * assert(std::get<2>(tup) == 42);
   *
   * auto moved_tup = std::move(row).get();
   * assert(moved_tup == tup);
   * @endcode
   */
  std::tuple<Types...>& get() & { return values_; }
  std::tuple<Types...> const& get() const& { return values_; }
  std::tuple<Types...>&& get() && { return std::move(values_); }
  std::tuple<Types...> const&& get() const&& { return std::move(values_); }

  /**
   * Returns a std::array of `Value` objects holding all the items in this row.
   *
   * @note: If the Row's values are large, it may be move efficient to "move"
   *     them into the returned array.
   *
   * @par Example
   *
   * @code
   * auto row = MakeRow(true, "foo", 42);
   * std::array<Value, 3> a = row.values();
   * assert(a[0].get<bool>() == true);
   * assert(a[1].get<std::string>() == "foo");
   * assert(a[2].get<std::int64_t>() == 42);
   *
   * // Potentially more efficient if `row` is no longer needed.
   * std::array<Value, 3> b = std::move(row).values();
   * @endcode
   */
  std::array<Value, size()> values() const& {
    std::array<Value, size()> array;
    internal::ForEach(values_, InsertValue{array, 0});
    return array;
  }
  /// @copydoc values()
  std::array<Value, size()> values() && {
    std::array<Value, size()> array;
    internal::ForEach(std::move(values_), InsertValue{array, 0});
    return array;
  }

  /// @name Equality operators
  ///@{
  friend bool operator==(Row const& a, Row const& b) {
    return a.values_ == b.values_;
  }
  friend bool operator!=(Row const& a, Row const& b) {
    return a.values_ != b.values_;
  }
  ///@}

  /// @name Relational operators
  ///@{
  friend bool operator<(Row const& a, Row const& b) {
    return a.values_ < b.values_;
  }
  friend bool operator<=(Row const& a, Row const& b) {
    return a.values_ <= b.values_;
  }
  friend bool operator>(Row const& a, Row const& b) {
    return a.values_ > b.values_;
  }
  friend bool operator>=(Row const& a, Row const& b) {
    return a.values_ >= b.values_;
  }
  ///@}

 private:
  // A helper functor to be used with `internal::ForEach` that adds each
  // element of the values_ tuple to an array of `Value` objects.
  struct InsertValue {
    std::array<Value, size()>& array;
    std::size_t i;
    template <typename T>
    void operator()(T&& t) {
      array[i++] = Value(std::forward<T>(t));
    }
  };

  std::tuple<Types...> values_;
};

// Returns the row-element at column `I`. This function will be found via ADL
// to make `Row<Ts...>` work with `internal::ForEach`. Users should not call
// this function directly; instead, simply call `row.get<I>()`.
template <std::size_t I, typename... Ts>
auto GetElement(Row<Ts...>& row) -> decltype(row.template get<I>()) {
  return row.template get<I>();
}

namespace internal {
// A helper metafunction that promots some C++ literal types to the types
// required by Cloud Spanner. For example, a literal 42 is of type int, but
// this will promote it to type std::int64_t. Similarly, a C++ string literal
// will be "promoted" to type std::string.
template <typename T>
T PromoteLiteralImpl(T);
std::string PromoteLiteralImpl(char const*);
std::int64_t PromoteLiteralImpl(int);
template <typename T>
using PromoteLiteral = decltype(PromoteLiteralImpl(std::declval<T>()));

// A helper functor to be used with `internal::ForEach` to iterate the columns
// of a `Row<Ts...>` in the ParseRow() function.
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
  template <typename It>
  void operator()(Value& v, It& it) const {
    v = *it++;
  }
};
}  // namespace internal

/**
 * Helper factory function to construct `Row<Ts...>` objects holding the
 * specified values.
 *
 * This is helpful because it deduces the types of the arguments, and it
 * promotes integer and string literals to the necessary Spanner types
 * `std::int64_t` and `std::string`, respectively.
 *
 * @par Example
 *
 * @code
 * auto row = MakeRow(42, "hello");
 * static_assert(
 *     std::is_same<Row<std::int64_t, std::string>, decltype(row)>::value, "");
 * @endcode
 */
template <typename... Ts>
Row<internal::PromoteLiteral<Ts>...> MakeRow(Ts&&... ts) {
  return Row<internal::PromoteLiteral<Ts>...>{std::forward<Ts>(ts)...};
}

/**
 * Parses a `std::array` of `Value` objects into the specified C++ types and
 * returns a `Row<Ts...>` with all the parsed values. If the specified C++ type
 * is unable to be extracted from a `Value`, an error `Status` is returned. The
 * given array size must exactly match the number of specified C++ types.
 *
 * @par Example
 *
 * @code
 * std::array<Value, 3> array = {Value(true), Value(42), Value("hello")};
 * auto row = ParseRow<bool, std::int64_t, std::string>(array);
 * assert(row.ok());
 * assert(MakeRow(true, 42, "hello"), *row);
 * @endcode
 */
template <typename... Ts>
StatusOr<Row<internal::PromoteLiteral<Ts>...>> ParseRow(
    std::array<Value, sizeof...(Ts)> const& array) {
  Row<internal::PromoteLiteral<Ts>...> row;
  auto it = array.begin();
  Status status;
  internal::ForEach(row, internal::ExtractValue{status}, it);
  if (!status.ok()) return status;
  return row;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_H_
