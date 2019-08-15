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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_

#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/keys.pb.h>
#include <string>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class KeySet;
template <typename RowType>
class KeySetBuilder;

namespace internal {
::google::spanner::v1::KeySet ToProto(KeySet keyset);

template <typename T>
struct IsRow : std::false_type {};

template <typename... Ts>
struct IsRow<Row<Ts...>> : std::true_type {};
}  // namespace internal

/**
 * The `Bound` class is a regular type that represents one endpoint of an
 * interval of keys.
 *
 * `Bound`s are `Closed` by default, meaning the row matching
 * the Bound will be included in the result. `Bound`s can also be
 * specified as `Open`, which will exclude the Bounds from
 * the results.
 *
 * @tparam KeyType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 */
template <typename RowType>
class Bound {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");
  /**
   * Constructs a closed `Bound` with a default constructed key of `KeyType`.
   */
  Bound() : Bound({}) {}
  /**
   * Constructs a closed `Bound` with the provided key.
   * @param key spanner::Row<Types...>
   */
  explicit Bound(RowType key) : Bound(std::move(key), Mode::kClosed) {}

  // Copy and move constructors and assignment operators.
  Bound(Bound const& key_range) = default;
  Bound& operator=(Bound const& rhs) = default;
  Bound(Bound&& key_range) = default;
  Bound& operator=(Bound&& rhs) = default;

  RowType const& key() const { return key_; }
  bool IsClosed() const { return mode_ == Mode::kClosed; }
  bool IsOpen() const { return mode_ == Mode::kOpen; }

  friend bool operator==(Bound const& lhs, Bound const& rhs) {
    return lhs.key_ == rhs.key_ && lhs.mode_ == rhs.mode_;
  }
  friend bool operator!=(Bound const& lhs, Bound const& rhs) {
    return !(lhs == rhs);
  }

 private:
  enum class Mode { kClosed, kOpen };

  Bound(RowType key, Mode mode) : key_(std::move(key)), mode_(mode) {}

  template <typename T>
  friend Bound<T> MakeBoundClosed(T key);

  template <typename T>
  friend Bound<T> MakeBoundOpen(T key);

  RowType key_;
  Mode mode_;
};

/**
 * Helper function to create a closed `Bound` on the key `spanner::Row`
 * provided.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param key spanner::Row<Types...>
 * @return Bound<RowType>
 */
template <typename RowType>
Bound<RowType> MakeBoundClosed(RowType key) {
  return Bound<RowType>(std::move(key), Bound<RowType>::Mode::kClosed);
}

/**
 * Helper function to create an open `Bound` on the key `spanner::Row` provided.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param key spanner::Row<Types...>
 * @return Bound<RowType>
 */
template <typename RowType>
Bound<RowType> MakeBoundOpen(RowType key) {
  return Bound<RowType>(std::move(key), Bound<RowType>::Mode::kOpen);
}

/**
 * The `KeyRange` class is a regular type that represents the pair of `Bound`s
 * necessary to uniquely identify a contiguous group of key `spanner::Row`s in
 * its index.
 *
 * @tparam KeyType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 */
template <typename RowType>
class KeyRange {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");
  /**
   * Constructs an empty `KeyRange`.
   */
  KeyRange() = default;
  ~KeyRange() = default;

  /**
   * Constructs a `KeyRange` with the given `Bound`s.
   */
  explicit KeyRange(Bound<RowType> start, Bound<RowType> end)
      : start_(std::move(start)), end_(std::move(end)) {}

  /**
   * Constructs a `KeyRange` closed on both `Bound`s.
   */
  explicit KeyRange(RowType start, RowType end)
      : KeyRange(MakeBoundClosed(std::move(start)),
                 MakeBoundClosed(std::move(end))) {}

  // Copy and move constructors and assignment operators.
  KeyRange(KeyRange const& key_range) = default;
  KeyRange& operator=(KeyRange const& rhs) = default;
  KeyRange(KeyRange&& key_range) = default;
  KeyRange& operator=(KeyRange&& rhs) = default;

  Bound<RowType> const& start() const { return start_; }
  Bound<RowType> const& end() const { return end_; }

  friend bool operator==(KeyRange const& lhs, KeyRange const& rhs) {
    return lhs.start_ == rhs.start_ && lhs.end_ == rhs.end_;
  }
  friend bool operator!=(KeyRange const& lhs, KeyRange const& rhs) {
    return !(lhs == rhs);
  }

 private:
  Bound<RowType> start_;
  Bound<RowType> end_;
};

/**
 * Helper function to create a `KeyRange` between two keys `spanner::Row`s with
 * both `Bound`s closed.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param start spanner::Row<Types...> of the starting key.
 * @param end spanner::Row<Types...> of the ending key.
 * @return KeyRange<RowType>
 */
template <typename RowType>
KeyRange<RowType> MakeKeyRange(RowType start, RowType end) {
  return MakeKeyRange(MakeBoundClosed(std::move(start)),
                      MakeBoundClosed(std::move(end)));
}

/**
 * Helper function to create a `KeyRange` between the `Bound`s provided.
 *
 * @tparam RowType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 * @param start spanner::Row<Types...> of the starting key.
 * @param end spanner::Row<Types...> of the ending key.
 * @return KeyRange<RowType>
 */
template <typename RowType>
KeyRange<RowType> MakeKeyRange(Bound<RowType> start, Bound<RowType> end) {
  return KeyRange<RowType>(std::move(start), std::move(end));
}

/**
 * The `KeySet` class is a regular type that represents the collection of
 * `spanner::Row`s necessary to uniquely identify a arbitrary group of rows in
 * its index, or all the keys in an index.
 *
 * Unlike `KeySetBuilder`, `KeySet` stores its keys and key ranges in a type
 * erased fashion.
 *
 */
class KeySet {
 public:
  /**
   * Returns a `KeySet` that represents the set of "All" keys for the index.
   */
  static KeySet All() { return KeySet(true); }

  /**
   * Constructs an empty `KeySet`.
   */
  KeySet() = default;

  /**
   * Constructs a `KeySet` from a `KeySetBuilder`.
   * @tparam RowType spanner::Row<Types...> that corresponds to the desired
   * index definition.
   */
  template <typename RowType>
  explicit KeySet(KeySetBuilder<RowType> const& builder) {
    for (auto const& key : builder.keys()) {
      key_values_.push_back(ValueRow(key.values()));
    }

    for (auto const& range : builder.key_ranges()) {
      key_ranges_.push_back(ValueKeyRange(
          ValueBound(ValueRow(range.start().key().values()),
                     range.start().IsClosed() ? ValueBound::Mode::kClosed
                                              : ValueBound::Mode::kOpen),
          ValueBound(ValueRow(range.end().key().values()),
                     range.end().IsClosed() ? ValueBound::Mode::kClosed
                                            : ValueBound::Mode::kOpen)));
    }
  }

  /**
   * Does the `KeySet` represent all keys for an index.
   */
  bool IsAll() const { return all_; }

 private:
  friend ::google::spanner::v1::KeySet internal::ToProto(KeySet keyset);

  explicit KeySet(bool all) : all_(all) {}

  class ValueRow {
   public:
    template <std::size_t N>
    explicit ValueRow(std::array<Value, N> column_values)
        : column_values_(std::make_move_iterator(column_values.begin()),
                         std::make_move_iterator(column_values.end())) {}

    std::vector<Value>& mutable_column_values() { return column_values_; }

   private:
    std::vector<Value> column_values_;
  };

  class ValueBound {
   public:
    enum class Mode { kClosed, kOpen };
    explicit ValueBound(ValueRow key, Mode mode)
        : key_(std::move(key)), mode_(mode) {}

    ValueRow& mutable_key() { return key_; }
    Mode mode() { return mode_; }

   private:
    ValueRow key_;
    Mode mode_;
  };

  class ValueKeyRange {
   public:
    explicit ValueKeyRange(ValueBound start, ValueBound end)
        : start_(std::move(start)), end_(std::move(end)) {}

    ValueBound& mutable_start() { return start_; }
    ValueBound& mutable_end() { return end_; }

   private:
    ValueBound start_;
    ValueBound end_;
  };

  std::vector<ValueRow> key_values_;
  std::vector<ValueKeyRange> key_ranges_;
  bool all_ = false;
};

/**
 * The `KeySetBuilder` class is a regular type that represents the collection of
 * `spanner::Row`s necessary to uniquely identify a arbitrary group of rows in
 * its index.
 *
 * A `KeySetBuilder` can consist of multiple `KeyRange` and/or `spanner::Row`
 * instances.
 *
 * If the same key is specified multiple times in the `KeySetBuilder` (for
 * example if two `KeyRange`s, two keys, or a key and a `KeyRange` overlap),
 * Cloud Spanner behaves as if the key were only specified once.
 *
 * @tparam KeyType spanner::Row<Types...> that corresponds to the desired index
 * definition.
 *
 * @par Example
 *
 * // TODO(#328): use snippet for example code
 * @code
 * // EmployeeTable has a primary key on EmployeeID.
 * using EmployeeTablePrimaryKey = Row<std::int64_t>;
 *
 * // A KeySet where EmployeeID >= 1 and EmployeeID <= 10.
 * auto first_ten_employees =
 *   KeySetBuilder<EmployeeTablePrimaryKey>()
 *       .Add(MakeKeyRange(MakeRow(1), MakeRow(10))
 *       .Build();
 *
 * // EmployeeTable also has an index on LastName, FirstName.
 * using EmployeeNameKey = Row<std::string, std::string>;
 *
 * // A KeySet where LastName, FirstName is ("Smith", "John").
 * auto all_employees_named_john_smith =
 *   KeySetBuilder<EmployeeNameKey>()
 *       .Add(MakeRow("Smith", "John")))
 *       .Build();
 * @endcode
 */
template <typename RowType>
class KeySetBuilder {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");
  /**
   * Constructs an empty `KeySetBuilder`.
   */
  KeySetBuilder() = default;

  /**
   * Constructs a `KeySetBuilder` with a single key `spanner::Row`.
   */
  explicit KeySetBuilder(RowType key) { keys_.push_back(std::move(key)); }

  /**
   * Constructs a `KeySetBuilder` with a single `KeyRange`.
   */
  explicit KeySetBuilder(KeyRange<RowType> key_range) {
    key_ranges_.push_back(std::move(key_range));
  }

  /**
   * Returns the key `spanner::Row`s in the `KeySetBuilder`.
   * These keys are separate from the collection of `KeyRange`s.
   */
  std::vector<RowType> const& keys() const { return keys_; }

  /**
   * Returns the `KeyRange`s in the `KeySetBuilder`.
   * These are separate from the collection of individual key `spanner::Row`s.
   */
  std::vector<KeyRange<RowType>> const& key_ranges() const {
    return key_ranges_;
  }

  /**
   * Adds a key `spanner::Row` to the `KeySetBuilder`.
   */
  KeySetBuilder& Add(RowType key) {
    keys_.push_back(std::move(key));
    return *this;
  }

  /**
   * Adds a `KeyRange` to the `KeySetBuilder`.
   */
  KeySetBuilder& Add(KeyRange<RowType> key_range) {
    key_ranges_.push_back(std::move(key_range));
    return *this;
  }

  /**
   * Builds a type-erased `KeySet` from the contents of the `KeySetBuilder`.
   *
   */
  KeySet Build() const { return KeySet(*this); }

  // TODO(#322): Add methods to insert ranges of Keys and KeyRanges.
  // TODO(#323): Add methods to remove Keys or KeyRanges.

 private:
  std::vector<RowType> keys_;
  std::vector<KeyRange<RowType>> key_ranges_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
