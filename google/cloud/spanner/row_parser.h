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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_

#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
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
 * range of `Row<Ts...>` objects.
 *
 * Instances of this class are typically obtained from the
 * `ResultSet::rows<Ts...>` member function. Callers should iterate `RowParser`
 * using a range-for loop as follows.
 *
 * @par Example
 *
 * @code
 * ValueSource vs = ...
 * RowParser<bool, std::int64_t> rp(std::move(vs));
 * for (auto row : rp) {
 *   if (!row) {
 *     // handle error
 *     break;
 *   }
 *   bool b = row->get<0>();
 *   std::int64_t i = row->get<1>();
 *
 *   // Using C++17 structured bindings
 *   auto [b2, i2] = row->get();
 * }
 * @endcode
 *
 * @tparam T the C++ type of the first column in each `Row` (required)
 * @tparam Ts... the types of the remaining columns in each `Row` (optional)
 */
template <typename T, typename... Ts>
class RowParser {
 public:
  /// A single-pass input iterator that coalesces multiple `Value` results into
  /// a `Row<Ts...>`.
  class iterator {
   public:
    using RowType = Row<T, Ts...>;
    using iterator_category = std::input_iterator_tag;
    using value_type = StatusOr<RowType>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    reference operator*() { return curr_; }
    pointer operator->() { return &curr_; }

    iterator& operator++() {
      if (!curr_) {
        value_source_ = nullptr;
        return *this;
      }
      std::array<Value, RowType::size()> values;
      for (std::size_t i = 0; i < values.size(); ++i) {
        StatusOr<optional<Value>> v = value_source_();
        if (!v) {
          curr_ = std::move(v).status();
          return *this;
        }
        if (!*v) {
          if (i == 0) {  // We've successfully reached the end.
            value_source_ = nullptr;
            return *this;
          }
          curr_ = Status(StatusCode::kUnknown, "incomplete row");
          return *this;
        }
        values[i] = **std::move(v);
      }
      curr_ = ParseRow<T, Ts...>(values);
      return *this;
    }

    iterator operator++(int) {
      auto const old = *this;
      operator++();
      return old;
    }

    friend bool operator==(iterator const& a, iterator const& b) {
      // Input iterators may only be compared to (copies of) themselves and
      // end. See https://en.cppreference.com/w/cpp/named_req/InputIterator
      return !a.value_source_ == !b.value_source_;  // Both end, or both not end
    }
    friend bool operator!=(iterator const& a, iterator const& b) {
      return !(a == b);
    }

   private:
    friend RowParser;
    explicit iterator(ValueSource f)
        : value_source_(std::move(f)), curr_(RowType{}) {
      if (value_source_) {
        operator++();  // Parse the first row
      }
    }

    ValueSource value_source_;  // nullptr means end
    value_type curr_;
  };

  /// Constructs a `RowParser` for the given `ValueSource`.
  explicit RowParser(ValueSource vs) : value_source_(std::move(vs)) {}

  // Copy and assignable.
  RowParser(RowParser const&) = default;
  RowParser& operator=(RowParser const&) = default;
  RowParser(RowParser&&) = default;
  RowParser& operator=(RowParser&&) = default;

  /// Returns the begin iterator.
  iterator begin() { return iterator(value_source_); }

  /// Returns the end iterator.
  iterator end() { return iterator(nullptr); }

 private:
  ValueSource value_source_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_
