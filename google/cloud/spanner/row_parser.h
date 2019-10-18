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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_

#include "google/cloud/spanner/internal/tuple_utils.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <array>
#include <functional>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * A functor that consumes and yields a `Value` object from some source.
 *
 * The returned `Value` is wrapped in an `optional` and a `StatusOr`. If there
 * was an error getting the `Value` to return, a non-OK `Status` should be
 * returned. If there was no error, but the source is empty, an OK `Status`
 * with an empty `optional<Value>` should be returned.
 *
 * @par Example
 *
 * The following example shows how to create a `ValueSource` from a vector:
 *
 * @code
 * ValueSource MakeValueSource(std::vector<Value> const& v) {
 *   std::size_t i = 0;
 *   return [=]() mutable -> StatusOr<optional<Value>> {
 *       if (i == v.size()) return optional<Value>{};
 *       return {v[i++]};
 *   };
 * }
 * @endcode
 */
using ValueSource = std::function<StatusOr<optional<Value>>()>;

/**
 * A `RowParser` converts the given `ValueSource` into a single-pass iterable
 * range of `std::tuple<Ts...>` objects.
 *
 * Instances of this class are typically obtained from the
 * `ResultSet::Rows<std::tuple<Ts...>>` member function. Callers should iterate
 * `RowParser` using a range-for loop as follows.
 *
 * @warning Moving a `RowParser` invalidates all iterators referring to the
 *     moved-from instance.
 *
 * @par Example
 *
 * @code
 * ValueSource vs = ...
 * RowParser<std::tuple<bool, std::int64_t>> rp(std::move(vs));
 * for (auto row : rp) {
 *   if (!row) {
 *     // handle error
 *     break;
 *   }
 *   bool b = std::get<0>(*row);
 *   std::int64_t i = std::get<1>(*row);
 *
 *   // Using C++17 structured bindings
 *   auto [b2, i2] = *row;
 * }
 * @endcode
 *
 * @tparam TupleType a `std::tuple<Ts...>`
 */
template <typename TupleType>
class RowParser {
 public:
  using value_type = StatusOr<TupleType>;

  /// A single-pass input iterator that coalesces multiple `Value` results into
  /// a `std::tuple<Ts...>`.
  class iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = RowParser::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    reference operator*() { return *parser_->curr_; }
    pointer operator->() { return &*parser_->curr_; }

    iterator& operator++() {
      parser_->Advance();
      return *this;
    }

    iterator operator++(int) {
      auto const old = *this;
      operator++();
      return old;
    }

    friend bool operator==(iterator const& a, iterator const& b) {
      return a.Equals(b);
    }
    friend bool operator!=(iterator const& a, iterator const& b) {
      return !(a == b);
    }

   private:
    friend RowParser;
    explicit iterator(RowParser* p) : parser_(p) {
      if (parser_ && !parser_->curr_) parser_->Advance();
    }

    bool Equals(iterator const& b) const {
      // Input iterators may only be compared to (copies of) themselves and
      // end. See https://en.cppreference.com/w/cpp/named_req/InputIterator
      //
      // Note: An "end" iterator has two representations that must compare
      // equal:
      //   1. When iterator::parser_ is nullptr_
      //   2. When iterator::parser_::value_source_ is nullptr_
      iterator const& a = *this;
      bool const a_is_end = !a.parser_ || !a.parser_->value_source_;
      bool const b_is_end = !b.parser_ || !b.parser_->value_source_;
      return a_is_end == b_is_end;
    }

    RowParser* parser_;  // nullptr means end
  };

  /// Constructs a `RowParser` for the given `ValueSource`.
  explicit RowParser(ValueSource vs) : value_source_(std::move(vs)) {}

  // Copy and assignable.
  RowParser(RowParser const&) = default;
  RowParser& operator=(RowParser const&) = default;
  RowParser(RowParser&&) = default;
  RowParser& operator=(RowParser&&) = default;

  /// Returns the begin iterator.
  iterator begin() { return iterator(this); }

  /// Returns the end iterator.
  iterator end() { return iterator(nullptr); }

 private:
  // A helper used with `internal::ForEach` to convert an array of `Value`
  // objects into a std::tuple.
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

  template <std::size_t N>
  StatusOr<TupleType> ParseRow(std::array<Value, N> const& array) {
    TupleType row;
    auto it = array.begin();
    Status status;
    internal::ForEach(row, ExtractValue{status}, it);
    if (!status.ok()) return status;
    return row;
  }

  // Consumes Values from value_source_ and stores the consumed std::tuple in
  // curr_. Called by iterator::operator++().
  void Advance() {
    if (curr_ && !*curr_) {  // Last row was an error; jump to end
      value_source_ = nullptr;
      return;
    }
    std::array<Value, std::tuple_size<TupleType>::value> values;
    for (std::size_t i = 0; i < values.size(); ++i) {
      StatusOr<optional<Value>> v = value_source_();
      if (!v) {
        curr_ = std::move(v).status();
        return;
      }
      if (!*v) {
        if (i == 0) {  // We've successfully reached the end.
          value_source_ = nullptr;
        } else {
          curr_ = Status(StatusCode::kUnknown, "incomplete row");
        }
        return;
      }
      values[i] = **std::move(v);
    }
    curr_ = ParseRow(values);
  }

  ValueSource value_source_;  // nullpr is end
  optional<value_type> curr_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_
