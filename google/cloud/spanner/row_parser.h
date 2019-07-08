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
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * A RowParser takes a range of `Value` objects and a list of types, and it
 * parses the values into a range of `Row` objects with the specified types.
 *
 * Callers should create instances of this class with the `MakeRowParser()`
 * function (defined below), which takes an input range and a list of types
 * that correspond to each row. The returned `RowParser` can be iterated,
 * producing a range of `StatusOr<Row<Ts...>>`. The `StatusOr` is needed in
 * case the input `Value` objects cannot be parsed to the requested type.
 *
 * Example:
 *
 *     std::vector<Value> const values = {
 *         Value(true), Value(0),  // Row 0
 *         Value(true), Value(1),  // Row 1
 *         Value(true), Value(2),  // Row 2
 *         Value(true), Value(3),  // Row 3
 *     };
 *     for (auto row : MakeRowParser<bool, std::int64_t>(values)) {
 *       if (!row) {
 *         // handle error
 *       }
 *       bool b = row->get<0>();
 *       std::int64_t i = row->get<1>();
 *     }
 *
 * @tparam ValueIterator the iterate for iterating `Value` objects.
 * @tparam Ts... the column types for each `Row`
 */
template <typename ValueIterator, typename... Ts>
class RowParser {
 public:
  /// A single-pass input iterator that coalesces multiple `Value` results into
  /// a `Row<Ts...>`.
  class iterator {
   public:
    using RowType = Row<Ts...>;
    using iterator_category = std::input_iterator_tag;
    using value_type = StatusOr<RowType>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    reference operator*() { return curr_; }
    pointer operator->() { return &curr_; }

    iterator& operator++() {
      curr_ = value_type{};
      if (it_ == end_) return *this;
      std::array<Value, RowType::size()> values;
      for (auto& e : values) {
        if (it_ == end_) {
          curr_ = Status(StatusCode::kUnknown, "incomplete row");
          return *this;
        }
        e = *it_++;
      }
      curr_ = ParseRow<Ts...>(values);
      return *this;
    }

    iterator operator++(int) {
      auto const old = *this;
      operator++();
      return old;
    }

    friend bool operator==(iterator const& a, iterator const& b) {
      return std::tie(a.curr_, a.it_, a.end_) ==
             std::tie(b.curr_, b.it_, b.end_);
    }
    friend bool operator!=(iterator const& a, iterator const& b) {
      return !(a == b);
    }

   private:
    friend RowParser;
    explicit iterator(ValueIterator begin, ValueIterator end)
        : it_(begin), end_(end) {
      operator++();  // Parse the first row
    }

    value_type curr_;
    ValueIterator it_;
    ValueIterator end_;
  };

  /// Constructs a `RowParser` for the given range of `Value`s.
  template <typename Range>
  explicit RowParser(Range&& range)
      : it_(std::begin(std::forward<Range>(range))),
        end_(std::end(std::forward<Range>(range))) {}

  /// Copy and assignable.
  RowParser(RowParser const&) = default;
  RowParser& operator=(RowParser const&) = default;
  RowParser(RowParser&&) = default;
  RowParser& operator=(RowParser&&) = default;

  /// Returns the begin iterator.
  iterator begin() { return iterator(it_, end_); }

  /// Returns the end iterator.
  iterator end() { return iterator(end_, end_); }

 private:
  ValueIterator it_;
  ValueIterator end_;
};

/**
 * Factory function to create a `RowParser` for the given range of `Value`s.
 *
 * See the `RowParser` documentation above for an example usage.
 *
 * @tparam Ts... the column types for each `Row`.
 * @tparam Range the input range
 */
template <typename... Ts, typename Range>
auto MakeRowParser(Range&& range)
    -> RowParser<decltype(std::begin(std::forward<Range>(range))), Ts...> {
  return RowParser<decltype(std::begin(std::forward<Range>(range))), Ts...>(
      std::forward<Range>(range));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_ROW_PARSER_H_
