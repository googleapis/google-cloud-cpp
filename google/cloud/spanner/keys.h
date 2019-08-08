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
#include <string>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
template <typename T>
struct IsRow : std::false_type {};

template <typename... Ts>
struct IsRow<Row<Ts...>> : std::true_type {};
}  // namespace internal

template <typename RowType>
class KeyRange;

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
  Bound() : Bound(RowType(), Mode::MODE_CLOSED) {}
  /**
   * Constructs a closed `Bound` with the provided key.
   * @param key spanner::Row<Types...>
   */
  explicit Bound(RowType key) : Bound(std::move(key), Mode::MODE_CLOSED) {}

  // Copy and move constructors and assignment operators.
  Bound(Bound const& key_range) = default;
  Bound& operator=(Bound const& rhs) = default;
  Bound(Bound&& key_range) = default;
  Bound& operator=(Bound&& rhs) = default;

  RowType const& key() const { return key_; }
  bool IsClosed() const { return mode_ == Mode::MODE_CLOSED; }
  bool IsOpen() const { return mode_ == Mode::MODE_OPEN; }

  friend bool operator==(Bound const& lhs, Bound const& rhs) {
    return lhs.key_ == rhs.key_ && lhs.mode_ == rhs.mode_;
  }
  friend bool operator!=(Bound const& lhs, Bound const& rhs) {
    return !(lhs == rhs);
  }

 private:
  enum class Mode { MODE_CLOSED, MODE_OPEN };

  Bound(RowType key, Mode mode) : key_(std::move(key)), mode_(mode) {}

  template <typename T>
  friend Bound<T> MakeBoundClosed(T key);

  template <typename T>
  friend Bound<T> MakeBoundOpen(T key);

  template <typename T>
  friend KeyRange<T> MakeKeyRange(T start, T end);

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
  return Bound<RowType>(std::move(key), Bound<RowType>::Mode::MODE_CLOSED);
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
  return Bound<RowType>(std::move(key), Bound<RowType>::Mode::MODE_OPEN);
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
  KeyRange() : start_(), end_() {}
  ~KeyRange() = default;

  /**
   * Constructs a `KeyRange` with closed `Bound`s on the keys provided.
   */
  explicit KeyRange(Bound<RowType> start, Bound<RowType> end)
      : start_(std::move(start)), end_(std::move(end)) {}

  /**
   * Constructs a `KeyRange` closed on both `Bound`s.
   */
  explicit KeyRange(RowType start, RowType end)
      : KeyRange(std::move(Bound<RowType>(start)),
                 std::move(Bound<RowType>(end))) {}

  // Copy and move constructors and assignment operators.
  KeyRange(KeyRange const& key_range) = default;
  KeyRange& operator=(KeyRange const& rhs) = default;
  KeyRange(KeyRange&& key_range) = default;
  KeyRange& operator=(KeyRange&& rhs) = default;

  Bound<RowType> const& start() const { return start_; }
  Bound<RowType> const& end() const { return end_; }

 private:
  friend bool operator==(KeyRange const& lhs, KeyRange const& rhs) {
    return lhs.start_ == rhs.start_ && lhs.end_ == rhs.end_;
  }
  friend bool operator!=(KeyRange const& lhs, KeyRange const& rhs) {
    return !(lhs == rhs);
  }

  Bound<RowType> start_;
  Bound<RowType> end_;
};

/**
 * The `KeySet` class is a regular type that represents the collection of
 * `spanner::Row`s necessary to uniquely identify a arbitrary group of rows in
 * its index.
 *
 * @warning TODO(#202) This class is currently just a stub; it can only
 * represent "all keys" or "no keys".
 */
class KeySet {
 public:
  static KeySet All() { return KeySet(true); }
  /**
   * Constructs an empty `KeySet`.
   */
  KeySet() = default;

  /**
   * Does the `KeySet` represent all keys for an index.
   */
  bool IsAll() const { return all_; }

 private:
  explicit KeySet(bool all) : all_(all) {}

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
 * // A KeySet where EmployeeID >= 1 and EmployeeID < 10.
 * auto first_ten_employees =
 *   KeySetBuilder<EmployeeTablePrimaryKey>(
 *     MakeKeyRangeBoundClosed(MakeRow(1)),
 *     MakeKeyRangeBoundOpen(MakeRow(10)));
 *
 * // EmployeeTable also has an index on LastName, FirstName.
 * using EmployeeNameKey = Row<std::string, std::string>;
 *
 * // A KeySet where LastName, FirstName is ("Smith", "John").
 * auto all_employees_named_john_smith =
 *   KeySetBuilder<EmployeeNameKey>(MakeRow("Smith", "John"));
 *
 * @endcode
 */
template <typename RowType>
class KeySetBuilder {
 public:
  static_assert(internal::IsRow<RowType>::value,
                "KeyType must be of type spanner::Row<>.");

  static KeySetBuilder<RowType> All() { return KeySetBuilder<RowType>(true); }

  /**
   * Constructs an empty `KeySetBuilder`.
   */
  KeySetBuilder() = default;

  /**
   * Constructs a `KeySetBuilder` with a single key `spanner::Row`.
   */
  explicit KeySetBuilder(RowType key) : all_(false), keys_(), key_ranges_() {
    keys_.emplace_back(std::move(key));
  }

  /**
   * Constructs a `KeySetBuilder` with a single `KeyRange`.
   */
  explicit KeySetBuilder(KeyRange<RowType> key_range)
      : all_(false), keys_(), key_ranges_() {
    key_ranges_.emplace_back(std::move(key_range));
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
    keys_.emplace_back(std::move(key));
    return *this;
  }

  /**
   * Adds a `KeyRange` to the `KeySetBuilder`.
   */
  KeySetBuilder& Add(KeyRange<RowType> key_range) {
    key_ranges_.emplace_back(std::move(key_range));
    return *this;
  }

  /**
   * Does the `KeySetBuilder` represent all keys for an index.
   */
  bool IsAll() const { return all_; }

  /**
   * Builds a type-erased `KeySet` from the contents of the `KeySetBuilder`.
   *
   * @warning Currently returns an empty `KeySet`.
   */
  KeySet Build() const { return KeySet(); }

  // TODO(#322): Add methods to insert ranges of Keys and KeyRanges.
  // TODO(#323): Add methods to remove Keys or KeyRanges.

 private:
  explicit KeySetBuilder(bool all) : all_(all), keys_(), key_ranges_() {}
  bool all_;
  std::vector<RowType> keys_;
  std::vector<KeyRange<RowType>> key_ranges_;
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
  return KeyRange<RowType>(
      Bound<RowType>(std::move(start), Bound<RowType>::Mode::MODE_CLOSED),
      Bound<RowType>(std::move(end), Bound<RowType>::Mode::MODE_CLOSED));
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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_KEYS_H_
