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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_QUERY_ROW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_QUERY_ROW_H

#include "google/cloud/bigtable/internal/tuple_utils.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_status.h"
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
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class QueryRow;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct QueryRowFriend {
  static bigtable::QueryRow MakeQueryRow(
      std::vector<bigtable::Value>,
      std::shared_ptr<std::vector<std::string> const>);
};
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A `QueryRow` is a sequence of columns each with a name and an associated
 * `Value`.
 *
 * The `QueryRow` class is a regular value type that may be copied, moved,
 * assigned, compared for equality, etc. Instances may be large if they hold
 * lots of `Value` data, so copy only when necessary.
 *
 * `QueryRow` instances are typically returned as the result of queries or reads
 * of a Cloud Bigtable table (see `Client::Read` and `Client::ExecuteQuery`).
 * Users will mostly just use the accessor methods on `QueryRow, and will rarely
 * (if ever) need to construct a `QueryRow` of their own.
 *
 * The number of columns in a `QueryRow` can be obtained from the `size()`
 * member function. The `Value`s can be obtained using the `values()` accessor.
 * The names of each column in the row can be obtained using the `columns()`
 * accessor.
 *
 * Perhaps the most convenient way to access the `Values` in a row is through
 * the variety of "get" accessors. A user may access a column's `Value` by
 * calling `get` with a `std::size_t` 0-indexed position, or a `std::string`
 * column name. Furthermore, callers may directly extract the native C++ type
 * by specifying the C++ type along with the column's position or name.
 *
 * @par Example
 *
 * @code
 * QueryRow row = ...;
 * if (StatusOr<std::string> x = row.get<std::string>("LastName")) {
 *   std::cout << "LastName=" << *x << "\n";
 * }
 * @endcode
 */
class QueryRow {
 public:
  /// Default constructs an empty row with no columns nor values.
  QueryRow();

  /// @name Copy and move.
  ///@{
  QueryRow(QueryRow const&) = default;
  QueryRow& operator=(QueryRow const&) = default;
  QueryRow(QueryRow&&) = default;
  QueryRow& operator=(QueryRow&&) = default;
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
      auto constexpr kMsg = "Tuple has the wrong number of elements";
      return google::cloud::internal::InvalidArgumentError(kMsg,
                                                           GCP_ERROR_INFO());
    }
    Tuple tup;
    auto it = values_.begin();
    Status status;
    bigtable_internal::ForEach(tup, ExtractValue{status}, it);
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
      auto constexpr kMsg = "Tuple has the wrong number of elements";
      return Status(StatusCode::kInvalidArgument, kMsg);
    }
    Tuple tup;
    auto it = std::make_move_iterator(values_.begin());
    Status status;
    bigtable_internal::ForEach(tup, ExtractValue{status}, it);
    if (!status.ok()) return status;
    return tup;
  }

  /// @name Equality
  ///@{
  friend bool operator==(QueryRow const& a, QueryRow const& b);
  friend bool operator!=(QueryRow const& a, QueryRow const& b) {
    return !(a == b);
  }
  ///@}

 private:
  friend struct bigtable_internal::QueryRowFriend;
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
   * Constructs a `QueryRow` with the given @p values and @p columns.
   *
   * @note columns must not be nullptr
   * @note columns.size() must equal values.size()
   */
  QueryRow(std::vector<Value> values,
           std::shared_ptr<std::vector<std::string> const> columns);

  std::vector<Value> values_;
  std::shared_ptr<std::vector<std::string> const> columns_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_QUERY_ROW_H
